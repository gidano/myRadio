#include "text_utils.h"

static void appendUTF8(String& out, uint16_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

bool text_looks_like_utf8(const char* s) {
  if (!s) return true;

  const uint8_t* p = (const uint8_t*)s;
  while (*p) {
    if (*p < 0x80) {
      p++;
      continue;
    }
    if ((*p & 0xE0) == 0xC0) {
      if ((p[1] & 0xC0) != 0x80) return false;
      p += 2;
      continue;
    }
    if ((*p & 0xF0) == 0xE0) {
      if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false;
      p += 3;
      continue;
    }
    if ((*p & 0xF8) == 0xF0) {
      if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return false;
      p += 4;
      continue;
    }
    return false;
  }
  return true;
}

static const uint16_t latin2_00A0_FF[96] = {
  0x00A0,0x0104,0x02D8,0x0141,0x00A4,0x013D,0x015A,0x00A7,0x00A8,0x0160,0x015E,0x0164,0x0179,0x00AD,0x017D,0x017B,
  0x00B0,0x0105,0x02DB,0x0142,0x00B4,0x013E,0x015B,0x02C7,0x00B8,0x0161,0x015F,0x0165,0x017A,0x02DD,0x017E,0x017C,
  0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
  0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
  0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
  0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9
};

String text_latin2_to_utf8(const char* in) {
  String out;
  if (!in) return out;

  while (*in) {
    uint8_t c = (uint8_t)*in++;
    if (c < 0x80) { out += (char)c; continue; }
    if (c >= 0xA0) { appendUTF8(out, latin2_00A0_FF[c - 0xA0]); continue; }
    if (c == 0x96) { appendUTF8(out, 0x2013); continue; }
    if (c == 0x97) { appendUTF8(out, 0x2014); continue; }
    if (c == 0x85) { appendUTF8(out, 0x2026); continue; }
    if (c == 0x91) { appendUTF8(out, 0x2018); continue; }
    if (c == 0x92) { appendUTF8(out, 0x2019); continue; }
    if (c == 0x93) { appendUTF8(out, 0x201C); continue; }
    if (c == 0x94) { appendUTF8(out, 0x201D); continue; }
    if (c == 0x84) { appendUTF8(out, 0x201E); continue; }
  }
  return out;
}

String text_fix(const char* s) {
  if (!s) return "";
  if (text_looks_like_utf8(s)) return String(s);
  return text_latin2_to_utf8(s);
}

int text_parseFirstInt(const char* s) {
  int val = 0;
  bool found = false;
  if (!s) return 0;
  while (*s) {
    if (*s >= '0' && *s <= '9') {
      found = true;
      val = val * 10 + (*s - '0');
    } else if (found) {
      break;
    }
    s++;
  }
  return found ? val : 0;
}

String text_trimCopy(String s) {
  s.trim();
  return s;
}

String text_extractAfterColon(const String& s) {
  int p = s.indexOf(':');
  if (p < 0) return "";
  String v = s.substring(p + 1);
  v.trim();
  return v;
}

bool text_startsWithNoCase(const String& s, const char* prefix) {
  String a = s; a.toLowerCase();
  String b = String(prefix); b.toLowerCase();
  return a.startsWith(b);
}

bool text_endsWithIgnoreCase(const String& s, const char* suffix) {
  String a = s; a.toLowerCase();
  String b = String(suffix); b.toLowerCase();
  return a.endsWith(b);
}

String text_urlPercentDecode(const String& in) {
  String out;
  out.reserve(in.length());
  for (int i = 0; i < (int)in.length(); i++) {
    char c = in[i];
    if (c == '%' && i + 2 < (int)in.length()) {
      char h1 = in[i + 1];
      char h2 = in[i + 2];
      auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
        if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
        return -1;
      };
      int a = hex(h1);
      int b = hex(h2);
      if (a >= 0 && b >= 0) {
        out += char((a << 4) | b);
        i += 2;
        continue;
      }
    }
    if (c == '+') { out += ' '; }
    else { out += c; }
  }
  return out;
}

String text_detectCodecFromText(const String& s) {
  String u = s; u.toUpperCase();
  if (u.indexOf("OPUS") >= 0) return "OPUS";
  if (u.indexOf("FLAC") >= 0) return "FLAC";
  if (u.indexOf("VORBIS") >= 0) return "VORBIS";
  if (u.indexOf("OGG") >= 0) return "OGG";
  if (u.indexOf("AAC") >= 0) return "AAC";
  if (u.indexOf("MP3") >= 0) return "MP3";
  if (u.indexOf("MPEG-1 LAYER III") >= 0) return "MP3";
  return "";
}
