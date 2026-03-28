#include "serial_spiffs.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <FS.h>
#include <mbedtls/base64.h>
#include <Audio.h>

extern Audio audio;

namespace {

Stream* g_serial = nullptr;
bool g_active = false;
String g_rxLine;

File g_writeFile;
String g_writePath;
size_t g_writeExpected = 0;
size_t g_writeReceived = 0;
bool g_writeInProgress = false;

void resetWriteState(bool removePartial) {
  if (g_writeFile) g_writeFile.close();
  if (removePartial && g_writePath.length()) SPIFFS.remove(g_writePath);
  g_writeInProgress = false;
  g_writeExpected = 0;
  g_writeReceived = 0;
  g_writePath = "";
}

String sanitizePath(String p) {
  p.trim();
  if (p.length() == 0) return String("/");
  if (!p.startsWith("/")) p = "/" + p;
  p.replace("\\", "/");
  while (p.indexOf("..") >= 0) p.replace("..", "");
  while (p.indexOf("//") >= 0) p.replace("//", "/");
  return p;
}

void ensureParentDirs(const String& fullPath) {
  int slash = fullPath.lastIndexOf('/');
  if (slash <= 0) return;
  String dir = fullPath.substring(0, slash);
  if (dir.length() == 0) return;

  String cur = "";
  int from = 0;
  while (from < (int)dir.length()) {
    int to = dir.indexOf('/', from);
    if (to < 0) to = dir.length();
    String part = dir.substring(from, to);
    if (part.length()) {
      cur += "/" + part;
      if (!SPIFFS.exists(cur)) SPIFFS.mkdir(cur);
    }
    from = to + 1;
  }
}

void sendLine(const String& s) {
  if (!g_serial) return;
  g_serial->print("MRSPIFS|");
  g_serial->println(s);
  g_serial->flush();
}

void sendOk(const String& cmd, const String& msg = "") {
  String out = "OK|" + cmd;
  if (msg.length()) out += "|" + msg;
  sendLine(out);
}

void sendErr(const String& cmd, const String& msg) {
  sendLine("ERR|" + cmd + "|" + msg);
}

bool split3(const String& line, String& a, String& b, String& c) {
  int p1 = line.indexOf('|');
  if (p1 < 0) return false;
  int p2 = line.indexOf('|', p1 + 1);
  if (p2 < 0) return false;
  a = line.substring(0, p1);
  b = line.substring(p1 + 1, p2);
  c = line.substring(p2 + 1);
  return true;
}

bool split4(const String& line, String& a, String& b, String& c, String& d) {
  int p1 = line.indexOf('|');
  if (p1 < 0) return false;
  int p2 = line.indexOf('|', p1 + 1);
  if (p2 < 0) return false;
  int p3 = line.indexOf('|', p2 + 1);
  if (p3 < 0) return false;
  a = line.substring(0, p1);
  b = line.substring(p1 + 1, p2);
  c = line.substring(p2 + 1, p3);
  d = line.substring(p3 + 1);
  return true;
}

String b64Encode(const uint8_t* data, size_t len) {
  if (!len) return String("");
  size_t outLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outLen, data, len);
  unsigned char* out = (unsigned char*)malloc(outLen + 1);
  if (!out) return String("");
  if (mbedtls_base64_encode(out, outLen, &outLen, data, len) != 0) {
    free(out);
    return String("");
  }
  out[outLen] = 0;
  String s((const char*)out);
  free(out);
  return s;
}

bool b64Decode(const String& in, uint8_t*& outBuf, size_t& outLen) {
  outBuf = nullptr;
  outLen = 0;
  if (in.length() == 0) return true;
  size_t need = 0;
  mbedtls_base64_decode(nullptr, 0, &need, (const unsigned char*)in.c_str(), in.length());
  outBuf = (uint8_t*)malloc(need + 4);
  if (!outBuf) return false;
  if (mbedtls_base64_decode(outBuf, need + 4, &outLen, (const unsigned char*)in.c_str(), in.length()) != 0) {
    free(outBuf);
    outBuf = nullptr;
    outLen = 0;
    return false;
  }
  return true;
}

void stopForMaintenance() {
  audio.stopSong();
  WiFi.disconnect(true, false);
  delay(50);
}

void handleList() {
  File root = SPIFFS.open("/");
  if (!root) {
    sendErr("LIST", "open_root_failed");
    return;
  }

  size_t count = 0;
  File f = root.openNextFile();
  while (f) {
    String p = String(f.path());
    if (!p.length()) p = String(f.name());
    if (!p.startsWith("/")) p = "/" + p;
    p.replace("\\", "/");
    while (p.indexOf("//") >= 0) p.replace("//", "/");
    sendLine("FILE|" + p + "|" + String((uint32_t)f.size()));
    count++;
    f = root.openNextFile();
  }
  sendOk("LIST", String(count));
}

void handleRead(const String& rawPath) {
  const String path = sanitizePath(rawPath);
  File f = SPIFFS.open(path, "r");
  if (!f) {
    sendErr("READ", "open_failed");
    return;
  }

  sendLine("READ_BEGIN|" + path + "|" + String((uint32_t)f.size()));
  uint8_t buf[384];
  while (f.available()) {
    const size_t n = f.read(buf, sizeof(buf));
    if (!n) break;
    String b64 = b64Encode(buf, n);
    if (!b64.length() && n) {
      f.close();
      sendErr("READ", "b64_encode_failed");
      return;
    }
    sendLine("DATA|" + b64);
    delay(0);
  }
  f.close();
  sendOk("READ_END", path);
}

void abortWrite(const String& reason) {
  resetWriteState(true);
  sendErr("WRITE", reason);
}

void handleWriteBegin(const String& rawPath, const String& rawSize) {
  if (g_writeInProgress || g_writePath.length()) {
    resetWriteState(true);
    delay(2);
  }

  g_writePath = sanitizePath(rawPath);
  g_writeExpected = (size_t)rawSize.toInt();
  g_writeReceived = 0;

  ensureParentDirs(g_writePath);
  if (SPIFFS.exists(g_writePath)) SPIFFS.remove(g_writePath);
  g_writeFile = SPIFFS.open(g_writePath, "w");
  if (!g_writeFile) {
    g_writePath = "";
    g_writeExpected = 0;
    sendErr("WRITE_BEGIN", "open_failed");
    return;
  }

  g_writeInProgress = true;
  sendOk("WRITE_BEGIN", g_writePath);
}

void handleWriteData(const String& b64) {
  if (!g_writeInProgress || !g_writeFile) {
    sendErr("WRITE_DATA", "no_write_in_progress");
    return;
  }

  uint8_t* decoded = nullptr;
  size_t decodedLen = 0;
  if (!b64Decode(b64, decoded, decodedLen)) {
    abortWrite("b64_decode_failed");
    return;
  }

  if (decodedLen) {
    const size_t written = g_writeFile.write(decoded, decodedLen);
    free(decoded);
    if (written != decodedLen) {
      abortWrite("write_failed");
      return;
    }
    g_writeReceived += written;
    g_writeFile.flush();
    delay(1);
  } else if (decoded) {
    free(decoded);
  }

  sendOk("WRITE_DATA", String((uint32_t)g_writeReceived));
}

void handleWriteEnd() {
  if (!g_writeInProgress || !g_writeFile) {
    sendErr("WRITE_END", "no_write_in_progress");
    return;
  }

  g_writeFile.close();
  const bool ok = (g_writeReceived == g_writeExpected);
  String finalPath = g_writePath;
  resetWriteState(false);

  if (!ok) {
    SPIFFS.remove(finalPath);
    sendErr("WRITE_END", "size_mismatch");
    return;
  }
  sendOk("WRITE_END", finalPath);
}

void handleWriteAbort() {
  if (g_writeInProgress || g_writePath.length()) {
    resetWriteState(true);
    sendOk("WRITE_ABORT", "aborted");
    return;
  }
  sendOk("WRITE_ABORT", "idle");
}

void handleDelete(const String& rawPath) {
  const String path = sanitizePath(rawPath);
  if (!SPIFFS.exists(path)) {
    sendErr("DELETE", "not_found");
    return;
  }
  if (!SPIFFS.remove(path)) {
    sendErr("DELETE", "remove_failed");
    return;
  }
  sendOk("DELETE", path);
}

void handleMkdir(const String& rawPath) {
  const String path = sanitizePath(rawPath);
  if (SPIFFS.exists(path)) {
    sendOk("MKDIR", path);
    return;
  }
  if (!SPIFFS.mkdir(path)) {
    sendErr("MKDIR", "mkdir_failed");
    return;
  }
  sendOk("MKDIR", path);
}

void handleExists(const String& rawPath) {
  const String path = sanitizePath(rawPath);
  sendOk("EXISTS", SPIFFS.exists(path) ? "1" : "0");
}

void processCommand(const String& line) {
  if (!line.length()) return;

  if (!g_active) {
    if (line == "MRSPIFS:BEGIN") {
      g_active = true;
      stopForMaintenance();
      sendOk("BEGIN", "maintenance_active");
    }
    return;
  }

  if (line == "PING") { sendOk("PING", "1"); return; }
  if (line == "LIST") { handleList(); return; }
  if (line == "WRITE_END") { handleWriteEnd(); return; }
  if (line == "WRITE_ABORT") { handleWriteAbort(); return; }
  if (line == "REBOOT") {
    sendOk("REBOOT", "now");
    delay(100);
    ESP.restart();
    return;
  }

  String a, b, c, d;
  if (split3(line, a, b, c)) {
    if (a == "READ") { handleRead(c); return; }
    if (a == "DELETE") { handleDelete(c); return; }
    if (a == "MKDIR") { handleMkdir(c); return; }
    if (a == "EXISTS") { handleExists(c); return; }
    if (a == "WRITE_DATA") { handleWriteData(c); return; }
  }

  if (split4(line, a, b, c, d)) {
    if (a == "WRITE_BEGIN") { handleWriteBegin(c, d); return; }
  }

  sendErr("CMD", "unknown_command");
}

} // namespace

void serial_spiffs_begin(Stream& serial) {
  g_serial = &serial;
  g_active = false;
  g_rxLine = "";
  resetWriteState(false);
}

void serial_spiffs_poll() {
  if (!g_serial) return;

  while (g_serial->available() > 0) {
    const char ch = (char)g_serial->read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      String line = g_rxLine;
      g_rxLine = "";
      line.trim();
      processCommand(line);
      continue;
    }
    if (g_rxLine.length() < 2048) g_rxLine += ch;
  }
}

bool serial_spiffs_is_active() {
  return g_active;
}
