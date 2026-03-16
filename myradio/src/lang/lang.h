#pragma once

#include "../../Lovyan_config.h"

#if MYRADIO_LANG == MYRADIO_LANG_HU
  #include "lang_hu.h"
#elif MYRADIO_LANG == MYRADIO_LANG_EN
  #include "lang_en.h"
#elif MYRADIO_LANG == MYRADIO_LANG_DE
  #include "lang_de.h"
#elif MYRADIO_LANG == MYRADIO_LANG_PL
  #include "lang_pl.h"
#else
  #error "Unsupported MYRADIO_LANG"
#endif
