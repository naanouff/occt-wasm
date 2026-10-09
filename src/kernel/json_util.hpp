#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace occt_kernel {

inline std::string jsonEscape(const std::string& raw) {
  std::string out;
  out.reserve(raw.size() + 8);
  for (unsigned char c : raw) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  return out;
}

inline char* duplicate(const std::string& text) {
  char* out = static_cast<char*>(std::malloc(text.size() + 1));
  if (!out) return nullptr;
  std::memcpy(out, text.c_str(), text.size() + 1);
  return out;
}

inline char* errorJson(const char* code, const std::string& message) {
  return duplicate(std::string("{\"ok\":false,\"error\":\"") + jsonEscape(message) + "\",\"code\":\"" +
                   code + "\"}");
}

}  // namespace occt_kernel
