/* The only translation unit that compiles the miniaudio implementation.
 *
 * Compiled as C, separately from the C++ sources, so it is built once and
 * never sees the project's stricter C++ warning flags.
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio_internal.h"
