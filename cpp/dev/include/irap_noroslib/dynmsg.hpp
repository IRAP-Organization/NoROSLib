// dynmsg.hpp -- runtime message types, parsed from ROS `.msg` text.
//
// The structs in std_msgs.hpp & friends need their md5 typed in by hand. These
// don't: give irap_noroslib the `.msg` text and it derives the md5sum (the exact
// ROS "gentools" algorithm) and the wire codec for you. That is what makes
// load_msg_file() possible -- see msgfile.hpp.
//
//   MsgType T = load_msg_file("/home/me/msgs/Pose2D.msg", "my_pkg");
//   DynamicMessage m = T.create();
//   m.set("x", 1.0);
//   double x = m.get<double>("x");
//
// Fields are addressed by name; nest with a dot, index with brackets:
//   m.set("header.frame_id", "odom");
//   m.get<double>("poses[2].position.x");
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "irap_noroslib/md5calc.h"
#include "irap_noroslib/message.hpp"

namespace irap_noroslib {

class Registry;
class DynamicMessage;
class MsgSpec;

/// One line of a `.msg`: a field, or a constant.
struct Field {
  std::string base_type;    // "float64", "Header", "geometry_msgs/Point"
  std::string name;
  bool is_array = false;
  int array_len = -1;       // >=0 for a fixed [N]; -1 for a variable []
  bool is_constant = false;
  std::string const_value;  // raw text, exactly as written -- it feeds the md5
};

/// A builtin scalar (bool/intN/floatN/string/time/duration). NOT "Header".
bool is_primitive(const std::string& t);

/// "Header" -> "std_msgs/Header"; a bare "Point" in package P -> "P/Point";
/// an already-qualified "pkg/Type" or a primitive comes back unchanged.
std::string resolve_type(const std::string& base_type, const std::string& pkg);

/// Parse `.msg` text into fields + constants, in declaration order.
std::vector<Field> parse_msg_text(const std::string& text);

/// A field value. The exact ROS type lives in the Field, so the codec is
/// spec-driven and this only has to hold and coerce -- which collapses the 11
/// numeric ROS types down to three kinds.
class Value {
 public:
  enum Kind { NONE, BOOL, INT, UINT, REAL, STRING, TIME, DURATION, MSG, ARRAY, BYTES };

  Value() = default;
  Value(bool v) : k_(BOOL), i_(v ? 1 : 0) {}
  Value(int8_t v) : k_(INT), i_(v) {}
  Value(int16_t v) : k_(INT), i_(v) {}
  Value(int32_t v) : k_(INT), i_(v) {}
  Value(long v) : k_(INT), i_(v) {}
  Value(long long v) : k_(INT), i_(v) {}
  Value(uint8_t v) : k_(UINT), u_(v) {}
  Value(uint16_t v) : k_(UINT), u_(v) {}
  Value(uint32_t v) : k_(UINT), u_(v) {}
  Value(unsigned long v) : k_(UINT), u_(v) {}
  Value(unsigned long long v) : k_(UINT), u_(v) {}
  Value(float v) : k_(REAL), d_(v) {}
  Value(double v) : k_(REAL), d_(v) {}
  Value(const char* v) : k_(STRING), s_(v ? v : "") {}
  Value(std::string v) : k_(STRING), s_(std::move(v)) {}
  Value(Time v) : k_(TIME), t_(v) {}
  Value(Duration v) : k_(DURATION), du_(v) {}
  Value(const DynamicMessage& m);
  Value(std::vector<Value> a);
  Value(std::vector<uint8_t> b);

  // Deep copy -- two DynamicMessages must never share a nested message.
  Value(const Value& o);
  Value& operator=(const Value& o);
  Value(Value&& o) noexcept;
  Value& operator=(Value&& o) noexcept;
  ~Value();

  Kind kind() const { return k_; }

  /// Coerces across the numeric kinds; throws across categories.
  template <typename T>
  T as() const {
    if constexpr (std::is_same_v<T, std::string>) return as_string();
    else if constexpr (std::is_same_v<T, Time>) return as_time();
    else if constexpr (std::is_same_v<T, Duration>) return as_duration();
    else if constexpr (std::is_same_v<T, bool>) return as_i64() != 0;
    else if constexpr (std::is_floating_point_v<T>) return static_cast<T>(as_f64());
    else if constexpr (std::is_integral_v<T>) return static_cast<T>(as_i64());
    else static_assert(sizeof(T) == 0, "irap_noroslib: unsupported field type");
  }

  std::vector<Value>& array();
  const std::vector<Value>& array() const;
  std::vector<uint8_t>& bytes();
  const std::vector<uint8_t>& bytes() const;
  DynamicMessage& msg();
  const DynamicMessage& msg() const;

  std::string str() const;   // debug repr

 private:
  int64_t as_i64() const;
  double as_f64() const;
  std::string as_string() const;
  Time as_time() const;
  Duration as_duration() const;

  Kind k_ = NONE;
  int64_t i_ = 0;
  uint64_t u_ = 0;
  double d_ = 0;
  Time t_{};
  Duration du_{};
  std::string s_;
  std::unique_ptr<DynamicMessage> m_;
  std::unique_ptr<std::vector<Value>> a_;
  std::unique_ptr<std::vector<uint8_t>> raw_;
};

/// A parsed message type: fields, md5, definition, and a codec driven by them.
class MsgSpec {
 public:
  MsgSpec(std::string full_type, std::string text, Registry* reg);

  const std::string& type() const { return type_; }
  const std::string& pkg() const { return pkg_; }
  const std::string& text() const { return text_; }
  const std::vector<Field>& fields() const { return fields_; }

  /// The pre-hash text: constants first, then fields, with nested types replaced
  /// by their md5. Hashing it gives the message md5; a SERVICE md5 hashes the
  /// request's md5_text() concatenated with the response's.
  const std::string& md5_text() const { return md5_text_; }
  const std::string& md5() const { return md5_; }
  /// The connection-header "message_definition": this text, then every dependency
  /// behind a `MSG:` separator.
  const std::string& definition() const { return definition_; }

  int slot_of(const std::string& name) const;         // -1 if absent or constant
  const Field* field_of(const std::string& name) const;
  const Field* constant(const std::string& name) const;

  void write(const DynamicMessage& m, Writer& w) const;
  DynamicMessage read(Reader& r) const;
  DynamicMessage make_default() const;
  /// A default element for an array field (used by DynamicMessage::append).
  Value default_element(const Field& f) const { return default_scalar(f.base_type); }

  /// Compute md5/definition. Called once every dependency is registered, so a
  /// spec is immutable afterwards -- which is what lets callbacks on socket
  /// threads hold a shared_ptr<const MsgSpec> without locking.
  void finalize();

 private:
  void write_field(const Field& f, const Value& v, Writer& w) const;
  Value read_field(const Field& f, Reader& r) const;
  void write_scalar(const std::string& base, const Value& v, Writer& w) const;
  Value read_scalar(const std::string& base, Reader& r) const;
  Value default_of(const Field& f) const;
  Value default_scalar(const std::string& base) const;

  std::string type_, pkg_, text_;
  Registry* reg_ = nullptr;
  std::vector<Field> fields_;
  std::map<std::string, int> slots_;   // field name -> slot in DynamicMessage
  std::vector<int> field_slot_;        // fields_ index -> slot, -1 for constants
  int nslots_ = 0;
  std::string md5_text_, md5_, definition_;
};

/// An instance of a runtime-loaded message type.
class DynamicMessage {
 public:
  DynamicMessage() = default;
  explicit DynamicMessage(std::shared_ptr<const MsgSpec> spec);

  bool valid() const { return spec_ != nullptr; }
  const std::string& type() const;
  const MsgSpec& spec() const;

  /// Set a field. `path` may nest/index: "header.frame_id", "pts[1].x".
  template <typename T>
  DynamicMessage& set(const std::string& path, const T& v) {
    at(path) = Value(v);
    return *this;
  }
  DynamicMessage& set(const std::string& path, const char* v) {
    at(path) = Value(std::string(v));
    return *this;
  }
  DynamicMessage& set(const std::string& path, const DynamicMessage& v) {
    at(path) = Value(v);
    return *this;
  }
  /// Fill an array field from a typed vector.
  template <typename T>
  DynamicMessage& set_array(const std::string& path, const std::vector<T>& v) {
    std::vector<Value> a;
    a.reserve(v.size());
    for (const T& e : v) a.emplace_back(e);
    at(path) = Value(std::move(a));
    return *this;
  }
  DynamicMessage& set_bytes(const std::string& path, std::vector<uint8_t> v) {
    at(path) = Value(std::move(v));
    return *this;
  }

  template <typename T>
  T get(const std::string& path) const {
    return at(path).template as<T>();
  }
  template <typename T>
  std::vector<T> get_array(const std::string& path) const {
    const Value& v = at(path);
    std::vector<T> out;
    if (v.kind() == Value::BYTES) {
      for (uint8_t b : v.bytes()) out.push_back(static_cast<T>(b));
      return out;
    }
    for (const Value& e : v.array()) out.push_back(e.template as<T>());
    return out;
  }

  Value& at(const std::string& path);
  const Value& at(const std::string& path) const;

  DynamicMessage& msg(const std::string& path) { return at(path).msg(); }
  const DynamicMessage& msg(const std::string& path) const { return at(path).msg(); }
  std::vector<Value>& array(const std::string& path) { return at(path).array(); }
  const std::vector<Value>& array(const std::string& path) const { return at(path).array(); }
  std::vector<uint8_t>& bytes(const std::string& path) { return at(path).bytes(); }
  const std::vector<uint8_t>& bytes(const std::string& path) const { return at(path).bytes(); }
  size_t size(const std::string& path) const;

  /// Append a default element to an array field and return it (for arrays of
  /// messages, so you can fill the new element in place).
  Value& append(const std::string& path);

  std::vector<uint8_t> serialize() const;
  std::string str() const;

  std::vector<Value>& values() { return values_; }
  const std::vector<Value>& values() const { return values_; }

 private:
  Value* resolve(const std::string& path, const MsgSpec** owner = nullptr,
                 const Field** field = nullptr) const;

  std::shared_ptr<const MsgSpec> spec_;
  mutable std::vector<Value> values_;
};

/// A handle on a runtime-loaded message type. Cheap to copy, safe to share.
class MsgType {
 public:
  MsgType() = default;
  explicit MsgType(std::shared_ptr<const MsgSpec> s) : spec_(std::move(s)) {}

  bool valid() const { return spec_ != nullptr; }
  const std::string& type() const { return get()->type(); }
  const std::string& md5() const { return get()->md5(); }
  const std::string& text() const { return get()->text(); }
  const std::string& definition() const { return get()->definition(); }
  const std::string& md5_text() const { return get()->md5_text(); }

  DynamicMessage create() const { return get()->make_default(); }
  DynamicMessage decode(const std::vector<uint8_t>& body) const {
    Reader r(body);
    return get()->read(r);
  }
  std::shared_ptr<const MsgSpec> shared() const { return spec_; }

 private:
  const MsgSpec* get() const {
    if (!spec_) throw std::runtime_error("irap_noroslib: empty MsgType");
    return spec_.get();
  }
  std::shared_ptr<const MsgSpec> spec_;
};

/// Every registered runtime type, by "pkg/Type". Process-global.
class Registry {
 public:
  static Registry& global();

  /// Register from `.msg` text. Identical text re-registers to the same type;
  /// conflicting text for a name already in use is refused (live publishers hold
  /// the old spec, and dependents have cached its md5).
  MsgType register_msg(const std::string& full_type, const std::string& text);

  /// Look up, or throw naming the file the caller still has to load.
  MsgType get(const std::string& full_type);
  bool has(const std::string& full_type);

  std::shared_ptr<const MsgSpec> spec(const std::string& full_type);  // used by MsgSpec

 private:
  Registry() = default;
  MsgType register_locked(const std::string& full_type, const std::string& text);
  void ensure_builtins();

  std::recursive_mutex mu_;
  std::map<std::string, std::shared_ptr<MsgSpec>> specs_;
  bool seeding_ = false;
  bool seeded_ = false;
};

/// Seed the registry with the 64 built-in types. Defined in msgfile.cpp (which is
/// where the *_msgs.hpp headers live); declared here so Registry can call it.
void seed_builtin_types(Registry& r);

}  // namespace irap_noroslib
