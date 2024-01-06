
#ifndef _FONT_H_
#define _FONT_H_

#include <inttypes.h>

struct font {
  const uint8_t * data;
  uint8_t width;
  uint8_t height;
  uint8_t offsets[256];
};

#endif /* _FONT_H_ */

