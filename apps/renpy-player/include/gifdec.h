#pragma once

// GIF87a/GIF89a decoder. Frames are composited onto the logical screen and
// stored as tightly packed A,R,G,B bytes (the same order the PS3 PNG decoder
// uploads). Delay is milliseconds per frame. The animation loops.

#include <stdint.h>

#define GIF_MAX_W      1920
#define GIF_MAX_H      1080
#define GIF_MAX_FRAMES 24

typedef struct {
   int w, h, count;
   int *delayMs;
   uint8_t *argb;   // count * w * h * 4
} GifAnim;

// 0 on success. On failure *out is left empty.
int gifDecode(const uint8_t *data, int size, GifAnim *out);
void gifFree(GifAnim *g);

// Frame to show after elapsedMs, looping over the delays. 0 when count < 1.
int gifFrameAt(const int *delayMs, int count, int elapsedMs);
