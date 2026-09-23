#include "gifdec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail;

static void expect(int cond, const char *msg)
{
   if (!cond) { fprintf(stderr, "FAIL %s\n", msg); fail++; }
}

static int pxEq(const uint8_t *p, int a, int r, int g, int b)
{
   return p[0] == a && p[1] == r && p[2] == g && p[3] == b;
}

int main(int argc, char **argv)
{
   static const uint8_t dot[] = {
      'G','I','F','8','9','a', 1,0, 1,0, 0x80,0,0,
      255,255,255, 0,0,0,
      0x2c, 0,0, 0,0, 1,0, 1,0, 0,
      0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
   };
   GifAnim g;
   expect(gifDecode(dot, (int)sizeof dot, &g) == 0, "decode 1x1");
   expect(g.w == 1 && g.h == 1 && g.count == 1, "1x1 size");
   if (g.argb) expect(pxEq(g.argb, 255, 255, 255, 255), "1x1 white");
   gifFree(&g);

   int delays[2] = { 100, 200 };
   expect(gifFrameAt(delays, 2, 0) == 0, "frame 0");
   expect(gifFrameAt(delays, 2, 99) == 0, "frame still 0");
   expect(gifFrameAt(delays, 2, 100) == 1, "frame 1");
   expect(gifFrameAt(delays, 2, 299) == 1, "frame 1 end");
   expect(gifFrameAt(delays, 2, 300) == 0, "loop");

   if (argc > 1) {
      FILE *f = fopen(argv[1], "rb");
      expect(f != NULL, "open gif");
      if (f) {
         fseek(f, 0, SEEK_END);
         long n = ftell(f);
         fseek(f, 0, SEEK_SET);
         uint8_t *buf = (uint8_t *)malloc((size_t)n);
         expect(buf && fread(buf, 1, (size_t)n, f) == (size_t)n, "read gif");
         fclose(f);
         GifAnim a;
         expect(gifDecode(buf, (int)n, &a) == 0, "decode ffmpeg gif");
         expect(a.count >= 2, "at least two frames");
         if (a.count >= 2 && a.argb) {
            const uint8_t *f0 = a.argb;
            const uint8_t *f1 = a.argb + a.w * a.h * 4;
            expect(f0[0] == 255 && f0[1] >= 250 && f0[2] == 0 && f0[3] == 0, "frame0 red");
            expect(f1[0] == 255 && f1[1] == 0 && f1[2] >= 250 && f1[3] == 0, "frame1 green");
            expect(a.delayMs[0] > 0 && a.delayMs[1] > 0, "frame delays");
         }
         gifFree(&a);
         free(buf);
      }
   }
   if (fail) { fprintf(stderr, "%d failed\n", fail); return 1; }
   fprintf(stderr, "ok\n");
   return 0;
}
