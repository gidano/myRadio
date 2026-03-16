#include "playlist_meta.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "../core/text_utils.h"

static bool ctxOk(const PlaylistMetaCtx& ctx) {
  return ctx.playlistUrls && ctx.playlistIndex && ctx.playlistSourceUrl &&
         ctx.stationUrl && ctx.playUrl && ctx.paused &&
         ctx.autoNextRequested && ctx.autoNextRequestedAt &&
         ctx.pendingTitle && ctx.newTitleFlag &&
         ctx.id3Artist && ctx.id3Title && ctx.id3SeenAt &&
         ctx.pendingCodec && ctx.pendingBitrateK &&
         ctx.pendingCh && ctx.pendingSampleRate && ctx.pendingBitsPerSample &&
         ctx.newStatusFlag;
}

static void splitArtistTitleLocal(const String& in, String& artist, String& title) {
  int idx = in.indexOf(" - ");
  if (idx < 0) idx = in.indexOf(" – ");
  if (idx >= 0) { artist = in.substring(0, idx); title = in.substring(idx + 3); }
  else { artist = ""; title = in; }
  artist.trim();
  title.trim();
}

static void maybePublishId3(PlaylistMetaCtx& ctx) {
  if (!ctxOk(ctx)) return;
  if (ctx.id3Title->length() == 0) return;

  String display = ctx.id3Artist->length()
    ? (*ctx.id3Artist + " - " + *ctx.id3Title)
    : *ctx.id3Title;

  *(ctx.pendingTitle) = text_fix(display.c_str());
  *(ctx.newTitleFlag) = true;
}

void playlist_meta_setup(PlaylistMetaCtx& ctx) {
  (void)ctx;
}

void playlist_meta_setNowPlayingFromUrl(PlaylistMetaCtx& ctx, const String& url) {
  if (!ctxOk(ctx)) return;

  int q = url.indexOf('?');
  String u = (q >= 0) ? url.substring(0, q) : url;

  int p = u.indexOf("://");
  if (p >= 0) {
    p = u.indexOf('/', p + 3);
  } else {
    p = u.indexOf('/');
  }
  String path = (p >= 0) ? u.substring(p + 1) : u;

  String decoded = text_urlPercentDecode(path);

  int lastSlash = decoded.lastIndexOf('/');
  String file = (lastSlash >= 0) ? decoded.substring(lastSlash + 1) : decoded;
  String parent = "";
  if (lastSlash > 0) {
    int prevSlash = decoded.lastIndexOf('/', lastSlash - 1);
    parent = (prevSlash >= 0) ? decoded.substring(prevSlash + 1, lastSlash) : decoded.substring(0, lastSlash);
  }

  int dot = file.lastIndexOf('.');
  if (dot > 0) file = file.substring(0, dot);

  while (file.length() >= 3 && isDigit(file[0]) && isDigit(file[1]) &&
         (file[2] == ' ' || file[2] == '-' || file[2] == '.' || file[2] == '_')) {
    file = file.substring(3);
    file.trim();
    if (file.startsWith("-")) { file = file.substring(1); file.trim(); }
    if (file.startsWith(".")) { file = file.substring(1); file.trim(); }
  }

  String artist = "";
  String title  = file;

  if (parent.length()) {
    String a, t;
    splitArtistTitleLocal(parent, a, t);
    if (a.length()) artist = a;
  }

  {
    String a, t;
    splitArtistTitleLocal(title, a, t);
    if (a.length() && t.length()) { artist = a; title = t; }
  }

  String display = artist.length() ? (artist + " - " + title) : title;
  *(ctx.pendingTitle) = text_fix(display.c_str());
  *(ctx.newTitleFlag) = true;
}

bool playlist_meta_loadM3U(PlaylistMetaCtx& ctx, const String& m3uUrl) {
  if (!ctxOk(ctx)) return false;

  ctx.playlistUrls->clear();
  *(ctx.playlistIndex) = -1;
  *(ctx.playlistSourceUrl) = "";

  if (!WiFi.isConnected()) return false;

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(m3uUrl)) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  body.replace("\r", "");
  int start = 0;
  while (start < (int)body.length()) {
    int nl = body.indexOf('\n', start);
    if (nl < 0) nl = body.length();
    String line = body.substring(start, nl);
    line.trim();
    start = nl + 1;

    if (line.length() == 0) continue;
    if (line.startsWith("#")) continue;
    if (line.indexOf("://") < 0) continue;

    ctx.playlistUrls->push_back(line);
  }

  if (ctx.playlistUrls->empty()) return false;

  *(ctx.playlistSourceUrl) = m3uUrl;
  *(ctx.playlistIndex) = 0;
  return true;
}

bool playlist_meta_startPlaybackCurrent(PlaylistMetaCtx& ctx, bool allowReloadPlaylist) {
  if (!ctxOk(ctx)) return false;

  if (text_endsWithIgnoreCase(*(ctx.stationUrl), ".m3u")) {
    bool needReload = (*(ctx.playlistSourceUrl) != *(ctx.stationUrl)) || ctx.playlistUrls->empty() || (*(ctx.playlistIndex) < 0);
    if (allowReloadPlaylist && needReload) {
      if (!playlist_meta_loadM3U(ctx, *(ctx.stationUrl))) {
        *(ctx.playUrl) = *(ctx.stationUrl);
      } else {
        *(ctx.playUrl) = (*(ctx.playlistUrls))[*(ctx.playlistIndex)];
      }
    } else if (!ctx.playlistUrls->empty() && *(ctx.playlistIndex) >= 0 && *(ctx.playlistIndex) < (int)ctx.playlistUrls->size()) {
      *(ctx.playUrl) = (*(ctx.playlistUrls))[*(ctx.playlistIndex)];
    } else {
      *(ctx.playUrl) = *(ctx.stationUrl);
    }
  } else {
    ctx.playlistUrls->clear();
    *(ctx.playlistIndex) = -1;
    *(ctx.playlistSourceUrl) = "";
    *(ctx.playUrl) = *(ctx.stationUrl);
  }

  *(ctx.id3Artist) = "";
  *(ctx.id3Title)  = "";
  *(ctx.id3SeenAt) = 0;
  playlist_meta_setNowPlayingFromUrl(ctx, *(ctx.playUrl));
  return true;
}

void playlist_meta_requestAutoNextTrack(PlaylistMetaCtx& ctx) {
  if (!ctxOk(ctx)) return;
  const uint32_t now = millis();
  if ((int32_t)(now - *(ctx.autoNextRequestedAt)) < 500) return;
  *(ctx.autoNextRequestedAt) = now;
  *(ctx.autoNextRequested) = true;
}

bool playlist_meta_stepPlaylist(PlaylistMetaCtx& ctx, int delta, bool connectNow) {
  if (!ctxOk(ctx)) return false;
  if (!text_endsWithIgnoreCase(*(ctx.stationUrl), ".m3u")) return false;

  if (*(ctx.playlistSourceUrl) != *(ctx.stationUrl) || ctx.playlistUrls->empty()) {
    if (!playlist_meta_loadM3U(ctx, *(ctx.stationUrl))) return false;
  }
  if (ctx.playlistUrls->empty()) return false;

  const int count = (int)ctx.playlistUrls->size();
  int idx = *(ctx.playlistIndex);
  if (idx < 0) idx = 0;
  idx = (idx + delta) % count;
  if (idx < 0) idx += count;
  *(ctx.playlistIndex) = idx;
  *(ctx.playUrl) = (*(ctx.playlistUrls))[idx];

  *(ctx.id3Artist) = "";
  *(ctx.id3Title)  = "";
  *(ctx.id3SeenAt) = 0;
  playlist_meta_setNowPlayingFromUrl(ctx, *(ctx.playUrl));

  if (connectNow && ctx.connectFn) ctx.connectFn(*(ctx.playUrl));
  return true;
}

bool playlist_meta_advancePlaylistAndPlay(PlaylistMetaCtx& ctx) {
  if (!ctxOk(ctx)) return false;
  if (*(ctx.paused)) return false;
  return playlist_meta_stepPlaylist(ctx, +1, true);
}

int playlist_meta_trackCount(const PlaylistMetaCtx& ctx) {
  return ctx.playlistUrls ? (int)ctx.playlistUrls->size() : 0;
}

int playlist_meta_trackIndex(const PlaylistMetaCtx& ctx) {
  return ctx.playlistIndex ? *(ctx.playlistIndex) : -1;
}

void playlist_meta_handleAudioInfo(PlaylistMetaCtx& ctx, Audio::msg_t m) {
  if (!ctxOk(ctx)) return;

  Serial.printf("%s: %s\n", m.s, m.msg);

  {
    String tag = String(m.s);
    String msg = String(m.msg);
    String tagL = tag; tagL.toLowerCase();
    String msgL = msg; msgL.toLowerCase();
    bool isEof = (tagL.indexOf("eof") >= 0) || (msgL.indexOf("eof") >= 0) ||
                 (msgL.indexOf("end of file") >= 0) || (msgL.indexOf("stream end") >= 0) ||
                 (msgL.indexOf("end") == 0 && msgL.indexOf("end") >= 0);
    if (isEof && text_endsWithIgnoreCase(*(ctx.stationUrl), ".m3u")) {
      playlist_meta_requestAutoNextTrack(ctx);
      return;
    }
  }

  if (m.e == Audio::evt_streamtitle) {
    String t = String(m.msg);
    t.trim();

    if (!t.length()) return;
    if (t.equalsIgnoreCase("PMEDIA")) return;
    if (t.equalsIgnoreCase("NA")) return;

    if (ctx.playUrl->startsWith("http://192.") ||
        ctx.playUrl->startsWith("http://10.")  ||
        ctx.playUrl->startsWith("http://172."))
      return;

    bool allCaps = true;
    for (char c : t) {
      if (isalpha(c) && islower(c)) { allCaps = false; break; }
    }
    if (allCaps && t.length() < 12) return;

    *(ctx.pendingTitle) = text_fix(t.c_str());
    *(ctx.newTitleFlag) = true;
    return;
  }

  String tag = String(m.s);
  String msg = String(m.msg);

  {
    String tagL = tag; tagL.toLowerCase();
    String msgL = msg; msgL.toLowerCase();

    bool looksId3 = (tagL.indexOf("id3") >= 0) || (msgL.indexOf("id3") >= 0) ||
                    (msg.indexOf("TIT2") >= 0) || (msg.indexOf("TPE1") >= 0);

    if (looksId3) {
      String val;

      if (msg.indexOf("TPE1") >= 0) {
        val = text_extractAfterColon(msg);
        if (val.length() == 0) {
          int p = msg.indexOf("TPE1");
          if (p >= 0) { val = msg.substring(p); val = text_extractAfterColon(val); }
        }
        if (val.length()) { *(ctx.id3Artist) = text_trimCopy(val); *(ctx.id3SeenAt) = millis(); maybePublishId3(ctx); return; }
      }
      if (msg.indexOf("TIT2") >= 0) {
        val = text_extractAfterColon(msg);
        if (val.length() == 0) {
          int p = msg.indexOf("TIT2");
          if (p >= 0) { val = msg.substring(p); val = text_extractAfterColon(val); }
        }
        if (val.length()) { *(ctx.id3Title) = text_trimCopy(val); *(ctx.id3SeenAt) = millis(); maybePublishId3(ctx); return; }
      }

      if (text_startsWithNoCase(msg, "title") || text_startsWithNoCase(msg, "id3 title") || text_startsWithNoCase(msg, "tit2")) {
        val = text_extractAfterColon(msg);
        if (val.length()) { *(ctx.id3Title) = text_trimCopy(val); *(ctx.id3SeenAt) = millis(); maybePublishId3(ctx); return; }
      }
      if (text_startsWithNoCase(msg, "artist") || text_startsWithNoCase(msg, "id3 artist") || text_startsWithNoCase(msg, "tpe1")) {
        val = text_extractAfterColon(msg);
        if (val.length()) { *(ctx.id3Artist) = text_trimCopy(val); *(ctx.id3SeenAt) = millis(); maybePublishId3(ctx); return; }
      }
    }
  }

  String c = text_detectCodecFromText(msg);
  if (c.length() == 0) c = text_detectCodecFromText(tag);
  if (c.length()) {
    *(ctx.pendingCodec) = c;
    *(ctx.newStatusFlag) = true;
  }

  if (tag.equalsIgnoreCase("bitrate") || msg.indexOf("Bitrate") >= 0 || msg.indexOf("bitrate") >= 0) {
    int bps = text_parseFirstInt(m.msg);
    if (bps > 0) {
      int kbps = (bps + 500) / 1000;
      *(ctx.pendingBitrateK) = kbps;
      *(ctx.newStatusFlag) = true;
    }
  }

  {
    String tagL = tag; tagL.toLowerCase();
    String msgL = msg; msgL.toLowerCase();

    int ch = 0;
    if (msgL.indexOf("stereo") >= 0) ch = 2;
    else if (msgL.indexOf("mono") >= 0) ch = 1;
    else if (tagL.indexOf("channels") >= 0 || msgL.indexOf("channels") >= 0 || msgL.indexOf(" ch") >= 0) {
      int v = text_parseFirstInt(m.msg);
      if (v >= 1 && v <= 8) ch = v;
    }
    if (ch > 0) {
      *(ctx.pendingCh) = ch;
      *(ctx.newStatusFlag) = true;
    }

    // Ezt visszaállítottam az eredeti logikára, mert a refaktor során túl tág lett,
    // és bitrate üzenetekből is sample rate-et olvasott ki (pl. 124 -> 124KHz).
    if (tagL.indexOf("samplerate") >= 0 || msgL.indexOf("samplerate") >= 0 ||
        msgL.indexOf("sample rate") >= 0 || msgL.indexOf("sample_rate") >= 0 ||
        tagL.indexOf("sample") >= 0) {
      int sr = text_parseFirstInt(m.msg);
      if (sr > 0 && sr < 1000) sr *= 1000;
      if (sr >= 8000 && sr <= 384000) {
        *(ctx.pendingSampleRate) = sr;
        *(ctx.newStatusFlag) = true;
      }
    } else if (msgL.indexOf("hz") >= 0) {
      int sr = text_parseFirstInt(m.msg);
      if (sr > 0 && sr < 1000) sr *= 1000;
      if (sr >= 8000 && sr <= 384000) {
        *(ctx.pendingSampleRate) = sr;
        *(ctx.newStatusFlag) = true;
      }
    }

    // Ezt is visszaállítottam az eredeti, szűkebb felismerésre, hogy a "bitrate"
    // ne zavarjon bele a bitmélységbe.
    if (tagL.indexOf("bits") >= 0 || msgL.indexOf("bits") >= 0 ||
        msgL.indexOf("bit depth") >= 0 || msgL.indexOf("bits per sample") >= 0 ||
        msgL.indexOf("bps") >= 0) {
      int b = text_parseFirstInt(m.msg);
      if (b >= 8 && b <= 32) {
        *(ctx.pendingBitsPerSample) = b;
        *(ctx.newStatusFlag) = true;
      }
    }
  }
}
