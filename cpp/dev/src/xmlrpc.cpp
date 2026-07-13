#include "irap_noroslib/xmlrpc.hpp"

#include <cstdlib>
#include <sstream>

namespace irap_noroslib {

int64_t XmlValue::as_int() const {
  switch (type) {
    case Type::Int: return i;
    case Type::Bool: return b ? 1 : 0;
    case Type::Double: return static_cast<int64_t>(d);
    case Type::String: return std::atoll(s.c_str());
    default: return 0;
  }
}

std::string XmlValue::as_str() const {
  if (type == Type::String) return s;
  if (type == Type::Int) return std::to_string(i);
  return {};
}

namespace {

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::string& in) {
  std::string out;
  out.reserve((in.size() + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 3 <= in.size(); i += 3) {
    uint32_t n = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8) | uint8_t(in[i + 2]);
    out += kB64[(n >> 18) & 63];
    out += kB64[(n >> 12) & 63];
    out += kB64[(n >> 6) & 63];
    out += kB64[n & 63];
  }
  size_t rem = in.size() - i;
  if (rem == 1) {
    uint32_t n = uint8_t(in[i]) << 16;
    out += kB64[(n >> 18) & 63];
    out += kB64[(n >> 12) & 63];
    out += "==";
  } else if (rem == 2) {
    uint32_t n = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8);
    out += kB64[(n >> 18) & 63];
    out += kB64[(n >> 12) & 63];
    out += kB64[(n >> 6) & 63];
    out += '=';
  }
  return out;
}

int b64val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;  // skip whitespace/padding/anything else
}

std::string base64_decode(const std::string& in) {
  std::string out;
  int buf = 0, bits = 0;
  for (char c : in) {
    int v = b64val(c);
    if (v < 0) continue;
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out += static_cast<char>((buf >> bits) & 0xFF);
    }
  }
  return out;
}

void xml_escape(const std::string& in, std::string* out) {
  for (char c : in) {
    switch (c) {
      case '&': *out += "&amp;"; break;
      case '<': *out += "&lt;"; break;
      case '>': *out += "&gt;"; break;
      default: *out += c;
    }
  }
}

void encode_value(const XmlValue& v, std::string* out) {
  *out += "<value>";
  switch (v.type) {
    case XmlValue::Type::Int: *out += "<i4>" + std::to_string(v.i) + "</i4>"; break;
    case XmlValue::Type::Bool: *out += std::string("<boolean>") + (v.b ? "1" : "0") + "</boolean>"; break;
    case XmlValue::Type::Double: {
      std::ostringstream ss;
      ss << v.d;
      *out += "<double>" + ss.str() + "</double>";
      break;
    }
    case XmlValue::Type::String: {
      *out += "<string>";
      xml_escape(v.s, out);
      *out += "</string>";
      break;
    }
    case XmlValue::Type::Base64: {
      *out += "<base64>";
      *out += base64_encode(v.s);
      *out += "</base64>";
      break;
    }
    case XmlValue::Type::Array: {
      *out += "<array><data>";
      for (const auto& e : v.arr) encode_value(e, out);
      *out += "</data></array>";
      break;
    }
    case XmlValue::Type::Struct: {
      *out += "<struct>";
      for (const auto& kv : v.members) {
        *out += "<member><name>";
        xml_escape(kv.first, out);
        *out += "</name>";
        encode_value(kv.second, out);
        *out += "</member>";
      }
      *out += "</struct>";
      break;
    }
  }
  *out += "</value>";
}

// --- Minimal XML tokenizer/parser -------------------------------------------
// We only need to walk a small, well-formed XML-RPC document. This is a
// forgiving hand parser: it finds tags and text between them.

struct Cursor {
  const std::string& s;
  size_t pos = 0;
  explicit Cursor(const std::string& str) : s(str) {}

  void skip_ws() { while (pos < s.size() && (unsigned char)s[pos] <= ' ') ++pos; }

  // Advance to the next '<...>' tag; returns the tag name (without brackets),
  // sets *closing if it was a </...> tag. Returns empty at end.
  std::string next_tag(bool* closing) {
    size_t lt = s.find('<', pos);
    if (lt == std::string::npos) { pos = s.size(); return {}; }
    size_t gt = s.find('>', lt);
    if (gt == std::string::npos) { pos = s.size(); return {}; }
    std::string tag = s.substr(lt + 1, gt - lt - 1);
    pos = gt + 1;
    *closing = false;
    if (!tag.empty() && tag[0] == '/') { *closing = true; tag = tag.substr(1); }
    // Strip attributes / self-close markers.
    size_t sp = tag.find_first_of(" \t/");
    if (sp != std::string::npos) tag = tag.substr(0, sp);
    return tag;
  }

  // Text content up to the next '<'.
  std::string text_until_tag() {
    size_t lt = s.find('<', pos);
    if (lt == std::string::npos) lt = s.size();
    std::string t = s.substr(pos, lt - pos);
    pos = lt;
    return t;
  }
};

void xml_unescape(std::string* s) {
  std::string out;
  out.reserve(s->size());
  for (size_t i = 0; i < s->size();) {
    if ((*s)[i] == '&') {
      if (s->compare(i, 5, "&amp;") == 0) { out += '&'; i += 5; continue; }
      if (s->compare(i, 4, "&lt;") == 0) { out += '<'; i += 4; continue; }
      if (s->compare(i, 4, "&gt;") == 0) { out += '>'; i += 4; continue; }
      if (s->compare(i, 6, "&quot;") == 0) { out += '"'; i += 6; continue; }
      if (s->compare(i, 6, "&apos;") == 0) { out += '\''; i += 6; continue; }
    }
    out += (*s)[i++];
  }
  *s = std::move(out);
}

// Parse one <value>...</value>. Cursor must be positioned right after the
// opening <value> tag. Consumes through the matching </value>.
bool parse_value(Cursor& c, XmlValue* out);

// After seeing an opening type tag, read text + consume closing tag.
bool read_typed_text(Cursor& c, std::string* text) {
  *text = c.text_until_tag();
  bool closing = false;
  c.next_tag(&closing);  // consume closing type tag
  return true;
}

bool parse_value(Cursor& c, XmlValue* out) {
  // A <value> contains either a typed child (<i4>, <string>, <array>, ...) or
  // bare text (an implicit string, as roscpp's XmlRpcpp emits: <value>foo</value>
  // and empty <value></value>). Read any text up to the next tag FIRST: for a
  // bare value it's the content; for a typed value it's empty/whitespace.
  std::string leading = c.text_until_tag();
  bool closing = false;
  std::string tag = c.next_tag(&closing);

  if (tag == "value" && closing) {
    // Bare (possibly empty) string value — the text we just read is it.
    xml_unescape(&leading);
    *out = XmlValue::Str(leading);
    return true;
  }
  if (closing) return false;  // unexpected closing tag

  // Typed child follows; any `leading` was inter-element whitespace (ignored).
  if (tag == "i4" || tag == "int") {
    std::string t; read_typed_text(c, &t);
    *out = XmlValue::Int(std::atoll(t.c_str()));
  } else if (tag == "boolean") {
    std::string t; read_typed_text(c, &t);
    *out = XmlValue::Bool(!t.empty() && t[0] == '1');
  } else if (tag == "double") {
    std::string t; read_typed_text(c, &t);
    *out = XmlValue::Double(std::atof(t.c_str()));
  } else if (tag == "string") {
    std::string t; read_typed_text(c, &t);
    xml_unescape(&t);
    *out = XmlValue::Str(t);
  } else if (tag == "base64") {
    std::string t; read_typed_text(c, &t);
    *out = XmlValue::Base64Bytes(base64_decode(t));
  } else if (tag == "array") {
    // <array><data> <value/>* </data></array>
    XmlValue arr;
    arr.type = XmlValue::Type::Array;
    // walk tags until we hit each <value>, recursing, until </data>.
    while (true) {
      bool cl = false;
      std::string t = c.next_tag(&cl);
      if (t.empty()) return false;
      if (t == "data" && cl) break;
      if (t == "data") continue;   // opening <data>
      if (t == "value" && !cl) {
        XmlValue elem;
        if (!parse_value(c, &elem)) return false;
        arr.arr.push_back(std::move(elem));
      } else if (t == "array" && cl) {
        break;
      }
    }
    *out = std::move(arr);
    // consume trailing </array> then </value>
    // We already consumed </data>; now expect </array>.
    bool cl = false;
    std::string t = c.next_tag(&cl);
    (void)t;  // </array>
  } else if (tag == "struct") {
    // <struct> <member><name>..</name><value>..</value></member>* </struct>
    XmlValue sv;
    sv.type = XmlValue::Type::Struct;
    while (true) {
      bool cl = false;
      std::string t = c.next_tag(&cl);
      if (t.empty()) return false;
      if (t == "struct" && cl) break;           // end of struct
      if (t == "member" && !cl) {
        std::string name;
        XmlValue child;
        while (true) {
          bool cl2 = false;
          std::string mt = c.next_tag(&cl2);
          if (mt.empty()) return false;
          if (mt == "member" && cl2) break;     // end of member
          if (mt == "name" && !cl2) {
            name = c.text_until_tag();
            bool x = false;
            c.next_tag(&x);                       // </name>
            xml_unescape(&name);
          } else if (mt == "value" && !cl2) {
            if (!parse_value(c, &child)) return false;
          }
        }
        sv.members.emplace_back(std::move(name), std::move(child));
      }
    }
    *out = std::move(sv);
  } else {
    // Unknown type: read as string-ish.
    std::string t; read_typed_text(c, &t);
    *out = XmlValue::Str(t);
  }

  // Consume closing </value> (for scalar types).
  if (out->type != XmlValue::Type::Array) {
    bool cl = false;
    c.next_tag(&cl);  // </value>
  } else {
    bool cl = false;
    c.next_tag(&cl);  // </value>
  }
  return true;
}

}  // namespace

std::string build_method_call(const std::string& method, const std::vector<XmlValue>& params) {
  std::string out = "<?xml version=\"1.0\"?>\r\n<methodCall><methodName>";
  out += method;
  out += "</methodName><params>";
  for (const auto& p : params) {
    out += "<param>";
    encode_value(p, &out);
    out += "</param>";
  }
  out += "</params></methodCall>";
  return out;
}

std::string build_method_response(const XmlValue& value) {
  std::string out = "<?xml version=\"1.0\"?>\r\n<methodResponse><params><param>";
  encode_value(value, &out);
  out += "</param></params></methodResponse>";
  return out;
}

bool parse_method_call(const std::string& xml, std::string* method, std::vector<XmlValue>* params) {
  Cursor c(xml);
  bool closing = false;
  // Find <methodName>.
  while (true) {
    std::string tag = c.next_tag(&closing);
    if (tag.empty()) return false;
    if (tag == "methodName" && !closing) break;
  }
  *method = c.text_until_tag();
  // Walk to each <param><value>.
  params->clear();
  while (true) {
    std::string tag = c.next_tag(&closing);
    if (tag.empty()) break;
    if (tag == "value" && !closing) {
      XmlValue v;
      if (parse_value(c, &v)) params->push_back(std::move(v));
    }
  }
  return true;
}

bool parse_method_response(const std::string& xml, XmlValue* out, std::string* fault_msg) {
  Cursor c(xml);
  bool closing = false;
  bool is_fault = false;
  while (true) {
    std::string tag = c.next_tag(&closing);
    if (tag.empty()) break;
    if (tag == "fault" && !closing) is_fault = true;
    if (tag == "value" && !closing) {
      XmlValue v;
      if (!parse_value(c, &v)) return false;
      if (is_fault) {
        if (fault_msg) *fault_msg = v.as_str();
        return false;
      }
      *out = std::move(v);
      return true;
    }
  }
  return false;
}

}  // namespace irap_noroslib
