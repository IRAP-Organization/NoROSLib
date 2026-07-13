// dynmsg.cpp -- the `.msg` parser, the ROS md5 algorithm, and the generic codec.
#include "irap_noroslib/dynmsg.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <set>
#include <sstream>

namespace irap_noroslib {
namespace {

const char* kPrimitives[] = {"bool", "int8", "byte", "uint8", "char", "int16",
                             "uint16", "int32", "uint32", "int64", "uint64",
                             "float32", "float64", "string", "time", "duration"};

std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : text) {
    if (c == '\n') { out.push_back(cur); cur.clear(); }
    else if (c != '\r') { cur += c; }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

// "float64[9]" -> ("float64", true, 9);  "Point[]" -> ("Point", true, -1)
void split_array(const std::string& tok, std::string* base, bool* is_array, int* len) {
  *base = tok;
  *is_array = false;
  *len = -1;
  size_t open = tok.find('[');
  if (open == std::string::npos || tok.back() != ']') return;
  *base = tok.substr(0, open);
  *is_array = true;
  std::string inner = tok.substr(open + 1, tok.size() - open - 2);
  if (!inner.empty()) *len = std::atoi(inner.c_str());
}

// A `string NAME=...` line. ROS lets a string constant's value run to the end of
// the line -- a '#' in it is NOT a comment -- so this must be matched BEFORE
// comments are stripped.
bool string_constant(const std::string& raw, Field* out) {
  std::string s = trim(raw);
  if (s.rfind("string ", 0) != 0) return false;
  size_t eq = s.find('=');
  if (eq == std::string::npos) return false;
  std::string decl = trim(s.substr(0, eq));
  // decl must be exactly "string NAME"
  size_t sp = decl.find_first_of(" \t");
  if (sp == std::string::npos) return false;
  std::string name = trim(decl.substr(sp));
  if (name.empty() || name.find_first_of(" \t") != std::string::npos) return false;
  out->base_type = "string";
  out->name = name;
  out->is_constant = true;
  out->const_value = trim(s.substr(eq + 1));   // genmsg strips string constants
  return true;
}

const std::string kSep(80, '=');

}  // namespace

bool is_primitive(const std::string& t) {
  for (const char* p : kPrimitives)
    if (t == p) return true;
  return false;
}

std::string resolve_type(const std::string& base_type, const std::string& pkg) {
  if (base_type == "Header") return "std_msgs/Header";
  if (is_primitive(base_type)) return base_type;
  if (base_type.find('/') != std::string::npos) return base_type;
  return pkg.empty() ? base_type : pkg + "/" + base_type;
}

std::vector<Field> parse_msg_text(const std::string& text) {
  std::vector<Field> fields;
  for (const std::string& raw : split_lines(text)) {
    Field sc;
    if (string_constant(raw, &sc)) { fields.push_back(sc); continue; }

    std::string line = raw;
    size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;

    size_t sp = line.find_first_of(" \t");
    if (sp == std::string::npos) continue;          // a lone token: not a field
    std::string type_tok = line.substr(0, sp);
    std::string rest = trim(line.substr(sp));
    if (rest.empty()) continue;

    size_t eq = rest.find('=');
    if (eq != std::string::npos && is_primitive(type_tok) &&
        type_tok != "time" && type_tok != "duration") {
      Field f;
      f.base_type = type_tok;
      f.name = trim(rest.substr(0, eq));
      f.is_constant = true;
      f.const_value = trim(rest.substr(eq + 1));
      fields.push_back(f);
      continue;
    }

    Field f;
    split_array(type_tok, &f.base_type, &f.is_array, &f.array_len);
    f.name = rest;
    fields.push_back(f);
  }
  return fields;
}

// ---------------------------------------------------------------- Value ----
Value::Value(const DynamicMessage& m)
    : k_(MSG), m_(new DynamicMessage(m)) {}
Value::Value(std::vector<Value> a)
    : k_(ARRAY), a_(new std::vector<Value>(std::move(a))) {}
Value::Value(std::vector<uint8_t> b)
    : k_(BYTES), raw_(new std::vector<uint8_t>(std::move(b))) {}

Value::Value(const Value& o)
    : k_(o.k_), i_(o.i_), u_(o.u_), d_(o.d_), t_(o.t_), du_(o.du_), s_(o.s_) {
  if (o.m_) m_.reset(new DynamicMessage(*o.m_));
  if (o.a_) a_.reset(new std::vector<Value>(*o.a_));
  if (o.raw_) raw_.reset(new std::vector<uint8_t>(*o.raw_));
}

Value& Value::operator=(const Value& o) {
  if (this == &o) return *this;
  Value tmp(o);
  *this = std::move(tmp);
  return *this;
}

Value::Value(Value&& o) noexcept = default;
Value& Value::operator=(Value&& o) noexcept = default;
Value::~Value() = default;

int64_t Value::as_i64() const {
  switch (k_) {
    case BOOL:
    case INT: return i_;
    case UINT: return static_cast<int64_t>(u_);
    case REAL: return static_cast<int64_t>(d_);
    default:
      throw std::runtime_error("irap_noroslib: field is not a number (" + str() + ")");
  }
}

double Value::as_f64() const {
  switch (k_) {
    case BOOL:
    case INT: return static_cast<double>(i_);
    case UINT: return static_cast<double>(u_);
    case REAL: return d_;
    default:
      throw std::runtime_error("irap_noroslib: field is not a number (" + str() + ")");
  }
}

std::string Value::as_string() const {
  if (k_ != STRING) throw std::runtime_error("irap_noroslib: field is not a string");
  return s_;
}
Time Value::as_time() const {
  if (k_ != TIME) throw std::runtime_error("irap_noroslib: field is not a time");
  return t_;
}
Duration Value::as_duration() const {
  if (k_ != DURATION) throw std::runtime_error("irap_noroslib: field is not a duration");
  return du_;
}

std::vector<Value>& Value::array() {
  if (k_ != ARRAY) throw std::runtime_error("irap_noroslib: field is not an array");
  return *a_;
}
const std::vector<Value>& Value::array() const {
  if (k_ != ARRAY) throw std::runtime_error("irap_noroslib: field is not an array");
  return *a_;
}
std::vector<uint8_t>& Value::bytes() {
  if (k_ != BYTES) throw std::runtime_error("irap_noroslib: field is not a uint8[]");
  return *raw_;
}
const std::vector<uint8_t>& Value::bytes() const {
  if (k_ != BYTES) throw std::runtime_error("irap_noroslib: field is not a uint8[]");
  return *raw_;
}
DynamicMessage& Value::msg() {
  if (k_ != MSG) throw std::runtime_error("irap_noroslib: field is not a message");
  return *m_;
}
const DynamicMessage& Value::msg() const {
  if (k_ != MSG) throw std::runtime_error("irap_noroslib: field is not a message");
  return *m_;
}

std::string Value::str() const {
  std::ostringstream o;
  switch (k_) {
    case NONE: o << "<none>"; break;
    case BOOL: o << (i_ ? "true" : "false"); break;
    case INT: o << i_; break;
    case UINT: o << u_; break;
    case REAL: o << d_; break;
    case STRING: o << '"' << s_ << '"'; break;
    case TIME: o << t_.sec << "." << t_.nsec; break;
    case DURATION: o << du_.sec << "." << du_.nsec; break;
    case MSG: o << m_->str(); break;
    case BYTES: o << "<" << raw_->size() << " bytes>"; break;
    case ARRAY: {
      o << "[";
      for (size_t i = 0; i < a_->size(); ++i) {
        if (i) o << ", ";
        if (i == 8 && a_->size() > 10) { o << "... " << a_->size() << " items"; break; }
        o << (*a_)[i].str();
      }
      o << "]";
      break;
    }
  }
  return o.str();
}

// -------------------------------------------------------------- MsgSpec ----
MsgSpec::MsgSpec(std::string full_type, std::string text, Registry* reg)
    : type_(std::move(full_type)), reg_(reg) {
  size_t slash = type_.find('/');
  pkg_ = slash == std::string::npos ? "" : type_.substr(0, slash);

  // keep the text verbatim (minus edge newlines) -- it IS the message_definition
  size_t a = text.find_first_not_of("\n");
  size_t b = text.find_last_not_of("\n");
  text_ = (a == std::string::npos) ? "" : text.substr(a, b - a + 1);

  fields_ = parse_msg_text(text);
  field_slot_.resize(fields_.size(), -1);
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i].is_constant) continue;
    field_slot_[i] = nslots_;
    slots_[fields_[i].name] = nslots_;
    ++nslots_;
  }
}

int MsgSpec::slot_of(const std::string& name) const {
  auto it = slots_.find(name);
  return it == slots_.end() ? -1 : it->second;
}

const Field* MsgSpec::field_of(const std::string& name) const {
  for (const Field& f : fields_)
    if (!f.is_constant && f.name == name) return &f;
  return nullptr;
}

const Field* MsgSpec::constant(const std::string& name) const {
  for (const Field& f : fields_)
    if (f.is_constant && f.name == name) return &f;
  return nullptr;
}

void MsgSpec::finalize() {
  // -- md5 text: the exact ROS gentools rules.
  //    constants first; builtin fields KEEP their array suffix; a complex field
  //    is replaced by the sub-message's md5 with the brackets DROPPED. A bare
  //    `Header` is not a primitive, so it takes the complex branch -- which is
  //    what ROS does.
  std::string consts, decls;
  for (const Field& f : fields_) {
    if (f.is_constant) {
      consts += f.base_type + " " + f.name + "=" + f.const_value + "\n";
      continue;
    }
    if (is_primitive(f.base_type)) {
      std::string t = f.base_type;
      if (f.is_array) {
        t += "[";
        if (f.array_len >= 0) t += std::to_string(f.array_len);
        t += "]";
      }
      decls += t + " " + f.name + "\n";
    } else {
      std::string full = resolve_type(f.base_type, pkg_);
      decls += reg_->spec(full)->md5() + " " + f.name + "\n";
    }
  }
  md5_text_ = consts + decls;
  if (!md5_text_.empty() && md5_text_.back() == '\n') md5_text_.pop_back();
  md5_ = md5_hex(md5_text_);

  // -- the concatenated message_definition: our text, then every dependency.
  //    Walk on is_primitive (NOT "is builtin"), so a bare `Header` still pulls in
  //    the MSG: std_msgs/Header block -- rostopic echo / rosbag need it.
  std::vector<std::string> order;
  std::set<std::string> seen;
  std::function<void(const MsgSpec*)> walk = [&](const MsgSpec* s) {
    for (const Field& f : s->fields_) {
      if (f.is_constant || is_primitive(f.base_type)) continue;
      std::string dep = resolve_type(f.base_type, s->pkg_);
      if (seen.insert(dep).second) {
        order.push_back(dep);
        walk(reg_->spec(dep).get());
      }
    }
  };
  walk(this);

  definition_ = text_;
  for (const std::string& dep : order)
    definition_ += "\n" + kSep + "\nMSG: " + dep + "\n" + reg_->spec(dep)->text();
  definition_ += "\n";
}

Value MsgSpec::default_scalar(const std::string& base) const {
  if (base == "bool") return Value(false);
  if (base == "string") return Value(std::string());
  if (base == "time") return Value(Time{});
  if (base == "duration") return Value(Duration{});
  if (base == "float32" || base == "float64") return Value(0.0);
  if (base == "uint8" || base == "char" || base == "uint16" || base == "uint32" ||
      base == "uint64")
    return Value(static_cast<uint64_t>(0));
  if (is_primitive(base)) return Value(static_cast<int64_t>(0));
  return Value(reg_->spec(resolve_type(base, pkg_))->make_default());
}

Value MsgSpec::default_of(const Field& f) const {
  if (!f.is_array) return default_scalar(f.base_type);
  bool blob = f.base_type == "uint8" || f.base_type == "char";
  if (blob) {
    return Value(std::vector<uint8_t>(f.array_len >= 0 ? f.array_len : 0, 0));
  }
  std::vector<Value> a;
  if (f.array_len >= 0)
    for (int i = 0; i < f.array_len; ++i) a.push_back(default_scalar(f.base_type));
  return Value(std::move(a));
}

DynamicMessage MsgSpec::make_default() const {
  DynamicMessage m(reg_->spec(type_));
  m.values().clear();
  m.values().reserve(nslots_);
  for (const Field& f : fields_) {
    if (f.is_constant) continue;
    m.values().push_back(default_of(f));
  }
  return m;
}

void MsgSpec::write_scalar(const std::string& base, const Value& v, Writer& w) const {
  if (base == "bool") w.boolean(v.as<bool>());
  else if (base == "int8" || base == "byte") w.i8(v.as<int8_t>());
  else if (base == "uint8" || base == "char") w.u8(v.as<uint8_t>());
  else if (base == "int16") w.i16(v.as<int16_t>());
  else if (base == "uint16") w.u16(v.as<uint16_t>());
  else if (base == "int32") w.i32(v.as<int32_t>());
  else if (base == "uint32") w.u32(v.as<uint32_t>());
  else if (base == "int64") w.i64(v.as<int64_t>());
  else if (base == "uint64") w.u64(v.as<uint64_t>());
  else if (base == "float32") w.f32(v.as<float>());
  else if (base == "float64") w.f64(v.as<double>());
  else if (base == "string") w.str(v.as<std::string>());
  else if (base == "time") w.time(v.as<Time>());
  else if (base == "duration") w.duration(v.as<Duration>());
  else reg_->spec(resolve_type(base, pkg_))->write(v.msg(), w);
}

Value MsgSpec::read_scalar(const std::string& base, Reader& r) const {
  if (base == "bool") return Value(r.boolean());
  if (base == "int8" || base == "byte") return Value(static_cast<int64_t>(r.i8()));
  if (base == "uint8" || base == "char") return Value(static_cast<uint64_t>(r.u8()));
  if (base == "int16") return Value(static_cast<int64_t>(r.i16()));
  if (base == "uint16") return Value(static_cast<uint64_t>(r.u16()));
  if (base == "int32") return Value(static_cast<int64_t>(r.i32()));
  if (base == "uint32") return Value(static_cast<uint64_t>(r.u32()));
  if (base == "int64") return Value(static_cast<int64_t>(r.i64()));
  if (base == "uint64") return Value(static_cast<uint64_t>(r.u64()));
  if (base == "float32") return Value(static_cast<double>(r.f32()));
  if (base == "float64") return Value(r.f64());
  if (base == "string") return Value(r.str());
  if (base == "time") return Value(r.time());
  if (base == "duration") return Value(r.duration());
  return Value(reg_->spec(resolve_type(base, pkg_))->read(r));
}

void MsgSpec::write_field(const Field& f, const Value& v, Writer& w) const {
  if (!f.is_array) { write_scalar(f.base_type, v, w); return; }

  bool blob = f.base_type == "uint8" || f.base_type == "char";
  if (blob) {
    const std::vector<uint8_t>& b = v.bytes();
    if (f.array_len < 0) { w.bytes(b); return; }
    std::vector<uint8_t> fixed(f.array_len, 0);   // pad/truncate, as ROS does
    std::copy_n(b.begin(), std::min<size_t>(b.size(), f.array_len), fixed.begin());
    w.raw(fixed);
    return;
  }

  const std::vector<Value>& a = v.array();
  if (f.array_len >= 0) {
    if (a.size() != static_cast<size_t>(f.array_len))
      throw std::runtime_error("irap_noroslib: fixed array '" + f.name + "' needs " +
                               std::to_string(f.array_len) + " elements, got " +
                               std::to_string(a.size()));
  } else {
    w.u32(static_cast<uint32_t>(a.size()));
  }
  for (const Value& e : a) write_scalar(f.base_type, e, w);
}

Value MsgSpec::read_field(const Field& f, Reader& r) const {
  if (!f.is_array) return read_scalar(f.base_type, r);

  bool blob = f.base_type == "uint8" || f.base_type == "char";
  if (blob) {
    size_t n = f.array_len >= 0 ? static_cast<size_t>(f.array_len) : r.u32();
    return Value(r.raw(n));
  }
  size_t n = f.array_len >= 0 ? static_cast<size_t>(f.array_len) : r.u32();
  std::vector<Value> a;
  a.reserve(n);
  for (size_t i = 0; i < n; ++i) a.push_back(read_scalar(f.base_type, r));
  return Value(std::move(a));
}

void MsgSpec::write(const DynamicMessage& m, Writer& w) const {
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i].is_constant) continue;
    write_field(fields_[i], m.values()[field_slot_[i]], w);
  }
}

DynamicMessage MsgSpec::read(Reader& r) const {
  DynamicMessage m(reg_->spec(type_));
  m.values().clear();
  m.values().reserve(nslots_);
  for (const Field& f : fields_) {
    if (f.is_constant) continue;
    m.values().push_back(read_field(f, r));
  }
  return m;
}

// ------------------------------------------------------- DynamicMessage ----
DynamicMessage::DynamicMessage(std::shared_ptr<const MsgSpec> spec)
    : spec_(std::move(spec)) {
  if (spec_) values_.resize(spec_->fields().size());
}

const std::string& DynamicMessage::type() const { return spec().type(); }

const MsgSpec& DynamicMessage::spec() const {
  if (!spec_) throw std::runtime_error("irap_noroslib: empty DynamicMessage");
  return *spec_;
}

// "header.stamp", "poses[2].position.x"
Value* DynamicMessage::resolve(const std::string& path, const MsgSpec** owner,
                               const Field** field) const {
  const MsgSpec* s = &spec();
  Value* cur = nullptr;
  std::vector<Value>* vals = &values_;

  size_t i = 0;
  while (i <= path.size()) {
    size_t dot = path.find('.', i);
    std::string tok = path.substr(i, dot == std::string::npos ? std::string::npos : dot - i);

    int index = -1;
    size_t br = tok.find('[');
    if (br != std::string::npos && tok.back() == ']') {
      index = std::atoi(tok.substr(br + 1, tok.size() - br - 2).c_str());
      tok = tok.substr(0, br);
    }

    int slot = s->slot_of(tok);
    if (slot < 0)
      throw std::runtime_error("irap_noroslib: no field '" + tok + "' in " + s->type() +
                               " (path '" + path + "')");
    cur = &(*vals)[slot];
    if (owner) *owner = s;
    if (field) *field = s->field_of(tok);

    if (index >= 0) {
      if (cur->kind() == Value::BYTES)
        throw std::runtime_error("irap_noroslib: index a uint8[] with bytes(\"" + tok +
                                 "\") instead of [" + std::to_string(index) + "]");
      std::vector<Value>& a = cur->array();
      if (static_cast<size_t>(index) >= a.size())
        throw std::runtime_error("irap_noroslib: index " + std::to_string(index) +
                                 " out of range for '" + tok + "' (size " +
                                 std::to_string(a.size()) + ")");
      cur = &a[index];
    }

    if (dot == std::string::npos) break;

    // step into a nested message for the next token
    if (cur->kind() != Value::MSG)
      throw std::runtime_error("irap_noroslib: '" + tok + "' is not a message, cannot "
                               "follow '" + path + "'");
    DynamicMessage& sub = cur->msg();
    s = &sub.spec();
    vals = &sub.values();
    i = dot + 1;
  }
  return cur;
}

Value& DynamicMessage::at(const std::string& path) { return *resolve(path); }
const Value& DynamicMessage::at(const std::string& path) const { return *resolve(path); }

size_t DynamicMessage::size(const std::string& path) const {
  const Value& v = at(path);
  if (v.kind() == Value::BYTES) return v.bytes().size();
  return v.array().size();
}

Value& DynamicMessage::append(const std::string& path) {
  const MsgSpec* owner = nullptr;
  const Field* f = nullptr;
  Value* v = resolve(path, &owner, &f);
  if (!f || !f->is_array)
    throw std::runtime_error("irap_noroslib: append() needs an array field, '" + path +
                             "' is not one");
  std::vector<Value>& a = v->array();          // throws for a uint8[] blob
  a.push_back(owner->default_element(*f));
  return a.back();
}

std::vector<uint8_t> DynamicMessage::serialize() const {
  Writer w;
  spec().write(*this, w);
  return w.b;
}

std::string DynamicMessage::str() const {
  if (!spec_) return "<empty>";
  std::ostringstream o;
  std::string name = spec_->type();
  size_t slash = name.find('/');
  o << (slash == std::string::npos ? name : name.substr(slash + 1)) << "(";
  bool first = true;
  size_t slot = 0;
  for (const Field& f : spec_->fields()) {
    if (f.is_constant) continue;
    if (!first) o << ", ";
    first = false;
    o << f.name << "=" << values_[slot].str();
    ++slot;
  }
  o << ")";
  return o.str();
}

// ------------------------------------------------------------- Registry ----
Registry& Registry::global() {
  static Registry r;
  return r;
}

void Registry::ensure_builtins() {
  if (seeded_ || seeding_) return;
  seeding_ = true;
  seed_builtin_types(*this);
  seeding_ = false;
  seeded_ = true;
}

MsgType Registry::register_msg(const std::string& full_type, const std::string& text) {
  std::lock_guard<std::recursive_mutex> lk(mu_);
  ensure_builtins();
  return register_locked(full_type, text);
}

MsgType Registry::register_locked(const std::string& full_type, const std::string& text) {
  if (full_type.find('/') == std::string::npos)
    throw std::runtime_error("irap_noroslib: message type must be \"pkg/Type\", got \"" +
                             full_type + "\"");

  auto existing = specs_.find(full_type);
  if (existing != specs_.end()) {
    // idempotent for identical text; refuse a conflicting redefinition
    MsgSpec probe(full_type, text, this);
    if (probe.text() == existing->second->text()) return MsgType(existing->second);
    throw std::runtime_error("irap_noroslib: \"" + full_type +
                             "\" is already registered with different text; refusing "
                             "to redefine it");
  }

  auto spec = std::make_shared<MsgSpec>(full_type, text, this);
  specs_[full_type] = spec;          // insert BEFORE finalize, so a self-reference
  try {                              // through a dependency can still find us
    spec->finalize();                // needs every dependency registered
  } catch (...) {
    specs_.erase(full_type);
    throw;
  }
  return MsgType(spec);
}

std::shared_ptr<const MsgSpec> Registry::spec(const std::string& full_type) {
  std::lock_guard<std::recursive_mutex> lk(mu_);
  ensure_builtins();
  auto it = specs_.find(full_type);
  if (it == specs_.end()) {
    std::string name = full_type;
    size_t slash = full_type.find('/');
    std::string pkg = slash == std::string::npos ? "" : full_type.substr(0, slash);
    if (slash != std::string::npos) name = full_type.substr(slash + 1);
    throw std::runtime_error(
        "irap_noroslib: unknown message type \"" + full_type +
        "\". It is nested by a type you loaded, so load its file too:\n"
        "    load_msg_file(\"/path/to/" + name + ".msg\", \"" + pkg + "\");");
  }
  return it->second;
}

MsgType Registry::get(const std::string& full_type) {
  return MsgType(spec(full_type));
}

bool Registry::has(const std::string& full_type) {
  std::lock_guard<std::recursive_mutex> lk(mu_);
  ensure_builtins();
  return specs_.count(full_type) != 0;
}

}  // namespace irap_noroslib
