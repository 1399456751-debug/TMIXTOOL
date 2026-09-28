#pragma once

/* The single place where miniaudio's compile-time configuration is fixed.
 *
 * Every translation unit that touches miniaudio MUST include this header
 * rather than <miniaudio.h> directly. If the implementation TU and a consumer
 * were to disagree about these macros, the struct layouts they each compile
 * could differ - an ODR violation that shows up as memory corruption rather
 * than a build error.
 *
 * Only WAV and MP3 decoding plus WASAPI playback are needed, so everything
 * else is compiled out to keep the build fast and the binary small.
 */

#define MA_NO_FLAC
#define MA_NO_VORBIS
#define MA_NO_ENCODING
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#define MA_NO_DSOUND
#define MA_NO_WINMM

#include "miniaudio.h"
