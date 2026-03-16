#pragma once
#include <Arduino.h>

bool text_looks_like_utf8(const char* s);
String text_latin2_to_utf8(const char* in);
String text_fix(const char* s);

int text_parseFirstInt(const char* s);

String text_trimCopy(String s);
String text_extractAfterColon(const String& s);
bool text_startsWithNoCase(const String& s, const char* prefix);
bool text_endsWithIgnoreCase(const String& s, const char* suffix);
String text_urlPercentDecode(const String& in);

String text_detectCodecFromText(const String& s);
