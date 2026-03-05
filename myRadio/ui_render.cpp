#include "ui_render.h"

// Stored refs
static UIRenderCtx R;

// UI state (ezekre mutogat a state_meta pointerekkel)
static int32_t  s_xStation = 0;
static int32_t  s_xArtist  = 0;
static int32_t  s_xTitle   = 0;
static bool     s_holdPhase = false;
static uint32_t s_trackChangedAtMs = 0;
static bool     s_forceRedrawText = true;

// Metrics + scroll flags
static int s_wStation = 0;
static int s_wArtist  = 0;
static int s_wTitle   = 0;

static bool s_scrollStation = false;
static bool s_scrollArtist  = false;
static bool s_scrollTitle   = false;

// Marquee timing
static uint32_t s_lastMarqueeTick = 0;

// Tuning (safe defaults; nem nyúlunk a rajzoló kódhoz)
static const uint32_t HOLD_MS    = 900;   // "holdPhase" idő, amikor még nem scrolloz
static const uint32_t MARQUEE_MS = 70;    // scroll tick
static const int      STEP_PX    = 3;
static const int      GAP_PX     = 40;

static bool okPtrs() {
  return (R.stationName && R.artist && R.title &&
          R.sprStation && R.sprArtist && R.sprTitle);
}

void ui_render_setup(const UIRenderCtx& ctx)
{
  R = ctx;
  s_xStation = s_xArtist = s_xTitle = 0;
  s_holdPhase = false;
  s_trackChangedAtMs = millis();
  s_forceRedrawText = true;
  s_lastMarqueeTick = 0;
  ui_render_recalcTextMetrics();
}

void ui_render_recalcTextMetrics()
{
  if (!okPtrs()) return;

  s_wStation = R.sprStation->textWidth(R.stationName->c_str());
  s_wArtist  = R.sprArtist ->textWidth(R.artist->c_str());
  s_wTitle   = R.sprTitle  ->textWidth(R.title->c_str());

  s_scrollStation = (s_wStation > R.sprStation->width());
  s_scrollArtist  = (s_wArtist  > R.sprArtist->width());
  s_scrollTitle   = (s_wTitle   > R.sprTitle->width());
}

bool ui_render_scrollStation()
{
  return s_scrollStation;
}

int32_t ui_render_centerXStation()
{
  if (!okPtrs()) return 0;
  int32_t w = s_wStation;
  int32_t box = R.sprStation->width();
  if (w <= 0 || box <= 0) return 0;
  int32_t x = (box - w) / 2;
  if (x < 0) x = 0;
  return x;
}

// app_impl hívja miután kirajzolta a Station sort.
// Nálad ez régen cache/dirty jellegű volt. Safe: no-op + eltesszük az x-et.
void ui_render_markStationDrawn(int32_t xUsed)
{
  (void)xUsed;
  // Ha centered módban rajzoltad, nem akarunk régi scroll-x-et megtartani.
  // (Nem kötelező, de segít elkerülni "ugrást".)
  if (!s_scrollStation) s_xStation = 0;
}

// Ha bármelyik sor scroll aktív
bool ui_render_anyScrollActive()
{
  return s_scrollStation || s_scrollArtist || s_scrollTitle;
}

void ui_render_updateMarquee()
{
  // FONTOS: itt NEM RAJZOLUNK, csak az x-eket és a holdPhase-t menedzseljük.
  if (!okPtrs()) return;

  uint32_t now = millis();

  // Ha friss track/meta jött, state_meta/app_impl beállítja s_forceRedrawText-et és s_trackChangedAtMs-t.
  // Itt csak a "hold" logikát alkalmazzuk.
  if (s_holdPhase) {
    if (now - s_trackChangedAtMs >= HOLD_MS) {
      s_holdPhase = false;
      s_lastMarqueeTick = now;
    } else {
      return;
    }
  }

  if (now - s_lastMarqueeTick < MARQUEE_MS) return;
  s_lastMarqueeTick = now;

  // Station
  if (s_scrollStation) {
    s_xStation -= STEP_PX;
    if (s_xStation < -(s_wStation + GAP_PX)) {
      s_xStation = R.sprStation->width();
    }
  } else {
    s_xStation = 0;
  }

  // Artist
  if (s_scrollArtist) {
    s_xArtist -= STEP_PX;
    if (s_xArtist < -(s_wArtist + GAP_PX)) {
      s_xArtist = R.sprArtist->width();
    }
  } else {
    s_xArtist = 0;
  }

  // Title
  if (s_scrollTitle) {
    s_xTitle -= STEP_PX;
    if (s_xTitle < -(s_wTitle + GAP_PX)) {
      s_xTitle = R.sprTitle->width();
    }
  } else {
    s_xTitle = 0;
  }
}

// pointer getters
int32_t*  ui_render_xStationPtr()        { return &s_xStation; }
int32_t*  ui_render_xArtistPtr()         { return &s_xArtist; }
int32_t*  ui_render_xTitlePtr()          { return &s_xTitle; }
bool*     ui_render_holdPhasePtr()       { return &s_holdPhase; }
uint32_t* ui_render_trackChangedAtPtr()  { return &s_trackChangedAtMs; }
bool*     ui_render_forceRedrawTextPtr() { return &s_forceRedrawText; }

// convenience
void ui_render_forceRedraw()
{
  s_forceRedrawText = true;
}

void ui_render_notifyTrackChange()
{
  s_trackChangedAtMs = millis();
  s_holdPhase = true;
  s_xStation = s_xArtist = s_xTitle = 0;
  s_forceRedrawText = true;
}