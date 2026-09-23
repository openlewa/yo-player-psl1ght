#include "gifdec.h"

#include <stdlib.h>
#include <string.h>

#define GIF_MAX_TOTAL (32 * 1024 * 1024)

static uint16_t rd16(const uint8_t *p)
{
   return (uint16_t)(p[0] | (p[1] << 8));
}

static int skipSubBlocks(const uint8_t *p, int size, int i)
{
   while (i < size) {
      int n = p[i++];
      if (n == 0) return i;
      if (i + n > size) return -1;
      i += n;
   }
   return -1;
}

static void putPx(uint8_t *canvas, int w, int h, int x, int y, uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
   if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
   uint8_t *d = canvas + ((y * w + x) * 4);
   d[0] = a; d[1] = r; d[2] = g; d[3] = b;
}

static int lzwDecode(const uint8_t *p, int size, int *ioff, int minCode, uint8_t *idx, int idxCount)
{
   if (*ioff >= size) return -1;
   int i = *ioff;
   uint8_t *packed = (uint8_t *)malloc((size_t)(size - i));
   if (!packed) return -1;
   int packedN = 0;
   while (i < size) {
      int n = p[i++];
      if (n == 0) break;
      if (i + n > size) { free(packed); return -1; }
      memcpy(packed + packedN, p + i, (size_t)n);
      packedN += n;
      i += n;
   }
   *ioff = i;

   int clear = 1 << minCode;
   int eoi = clear + 1;
   int codeSize = minCode + 1;
   int next = eoi + 1;
   int prev = -1;

   uint16_t prefix[4096];
   uint8_t suffix[4096];
   uint8_t stack[4096];
   int nstack;
   for (int c = 0; c < clear; c++) { prefix[c] = 0xFFFF; suffix[c] = (uint8_t)c; }

   uint32_t acc = 0;
   int bits = 0;
   int pi = 0;
   int outN = 0;

   while (outN < idxCount) {
      while (bits < codeSize) {
         if (pi >= packedN) { free(packed); return -1; }
         acc |= (uint32_t)packed[pi++] << bits;
         bits += 8;
      }
      int code = (int)(acc & ((1u << codeSize) - 1u));
      acc >>= codeSize;
      bits -= codeSize;
      if (code == clear) {
         codeSize = minCode + 1;
         next = eoi + 1;
         prev = -1;
         continue;
      }
      if (code == eoi) break;
      if (prev < 0 && code > eoi) { free(packed); return -1; }

      nstack = 0;
      if (code < next) {
         int t = code;
         while (prefix[t] != 0xFFFF && nstack < 4096) { stack[nstack++] = suffix[t]; t = prefix[t]; }
         if (nstack >= 4096) { free(packed); return -1; }
         stack[nstack++] = suffix[t];
      } else if (code == next && prev >= 0) {
         int t = prev;
         while (prefix[t] != 0xFFFF && nstack < 4096) { stack[nstack++] = suffix[t]; t = prefix[t]; }
         if (nstack >= 4096) { free(packed); return -1; }
         stack[nstack++] = suffix[t];
         if (nstack >= 4096) { free(packed); return -1; }
         {
            uint8_t firstByte = stack[nstack - 1];
            stack[nstack++] = firstByte;
         }
      } else {
         free(packed);
         return -1;
      }

      uint8_t first = stack[nstack - 1];
      while (nstack > 0 && outN < idxCount) idx[outN++] = stack[--nstack];

      if (prev >= 0 && next < 4096) {
         prefix[next] = (uint16_t)prev;
         suffix[next] = first;
         next++;
         if (next == (1 << codeSize) && codeSize < 12) codeSize++;
      }
      prev = code;
   }
   free(packed);
   return outN == idxCount ? 0 : -1;
}

int gifFrameAt(const int *delayMs, int count, int elapsedMs)
{
   if (!delayMs || count <= 1) return 0;
   if (elapsedMs < 0) elapsedMs = 0;
   int total = 0;
   for (int i = 0; i < count; i++) {
      int d = delayMs[i] > 0 ? delayMs[i] : 100;
      total += d;
   }
   if (total <= 0) return 0;
   int t = elapsedMs % total;
   int acc = 0;
   for (int i = 0; i < count; i++) {
      int d = delayMs[i] > 0 ? delayMs[i] : 100;
      acc += d;
      if (t < acc) return i;
   }
   return count - 1;
}

void gifFree(GifAnim *g)
{
   if (!g) return;
   free(g->delayMs);
   free(g->argb);
   memset(g, 0, sizeof(*g));
}

int gifDecode(const uint8_t *data, int size, GifAnim *out)
{
   memset(out, 0, sizeof(*out));
   if (!data || size < 13) return -1;
   if (!(data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' &&
         (data[4] == '7' || data[4] == '9') && data[5] == 'a')) return -1;

   int sw = rd16(data + 6);
   int sh = rd16(data + 8);
   if (sw <= 0 || sh <= 0 || sw > GIF_MAX_W || sh > GIF_MAX_H) return -1;
   int packed = data[10];
   int bg = data[11];
   int gctN = (packed & 0x80) ? (1 << ((packed & 7) + 1)) : 0;
   int i = 13;
   if (i + gctN * 3 > size) return -1;

   uint8_t gct[256][3];
   memset(gct, 0, sizeof gct);
   for (int c = 0; c < gctN; c++) {
      gct[c][0] = data[i++];
      gct[c][1] = data[i++];
      gct[c][2] = data[i++];
   }

   int canvasBytes = sw * sh * 4;
   if (canvasBytes <= 0 || canvasBytes > GIF_MAX_TOTAL) return -1;

   uint8_t *canvas = (uint8_t *)calloc((size_t)canvasBytes, 1);
   uint8_t *backup = (uint8_t *)malloc((size_t)canvasBytes);
   if (!canvas || !backup) { free(canvas); free(backup); return -1; }

   int delayCs = 10;
   int trans = 0, transIdx = 0, disposal = 0;
   int haveGce = 0;

   int cap = 4;
   int count = 0;
   int *delays = (int *)malloc((size_t)cap * sizeof(int));
   uint8_t *frames = NULL;
   if (!delays) { free(canvas); free(backup); return -1; }

   int rc = -1;
   while (i < size) {
      uint8_t sep = data[i++];
      if (sep == 0x3B) { rc = 0; break; }
      if (sep == 0x21) {
         if (i >= size) break;
         uint8_t label = data[i++];
         if (label == 0xF9) {
            if (i >= size) break;
            int bl = data[i++];
            if (bl < 4 || i + bl > size) break;
            uint8_t gp = data[i];
            disposal = (gp >> 2) & 7;
            trans = gp & 1;
            delayCs = rd16(data + i + 1);
            transIdx = data[i + 3];
            haveGce = 1;
            i += bl;
            if (i < size && data[i] == 0) i++;
         } else {
            int n = skipSubBlocks(data, size, i);
            if (n < 0) break;
            i = n;
         }
         continue;
      }
      if (sep != 0x2C) break;
      if (i + 9 > size) break;
      int left = rd16(data + i);
      int top = rd16(data + i + 2);
      int iw = rd16(data + i + 4);
      int ih = rd16(data + i + 6);
      uint8_t ip = data[i + 8];
      i += 9;
      int lctN = (ip & 0x80) ? (1 << ((ip & 7) + 1)) : 0;
      if (i + lctN * 3 > size) break;
      uint8_t lct[256][3];
      uint8_t (*ct)[3] = gct;
      if (lctN) {
         for (int c = 0; c < lctN; c++) {
            lct[c][0] = data[i++];
            lct[c][1] = data[i++];
            lct[c][2] = data[i++];
         }
         ct = lct;
      }
      if (i >= size) break;
      int minCode = data[i++];
      if (minCode < 2 || minCode > 8) break;
      if (iw <= 0 || ih <= 0) {
         int n = skipSubBlocks(data, size, i);
         if (n < 0) break;
         i = n;
         haveGce = 0;
         continue;
      }

      int npx = iw * ih;
      uint8_t *idx = (uint8_t *)malloc((size_t)npx);
      if (!idx) break;
      if (lzwDecode(data, size, &i, minCode, idx, npx) != 0) { free(idx); break; }

      if (disposal == 3) memcpy(backup, canvas, (size_t)canvasBytes);

      int interlace = (ip & 0x40) != 0;
      static const int passStart[4] = { 0, 4, 2, 1 };
      static const int passStep[4] = { 8, 8, 4, 2 };
      int pass = 0, row = 0, pix = 0;
      for (int y = 0; y < ih; y++) {
         int dy = interlace ? (passStart[pass] + row * passStep[pass]) : y;
         for (int x = 0; x < iw; x++, pix++) {
            uint8_t k = idx[pix];
            if (trans && k == (uint8_t)transIdx) continue;
            putPx(canvas, sw, sh, left + x, top + dy, 255, ct[k][0], ct[k][1], ct[k][2]);
         }
         if (interlace) {
            row++;
            if (passStart[pass] + row * passStep[pass] >= ih) { pass++; row = 0; if (pass > 3) pass = 3; }
         }
      }
      free(idx);

      if (count >= GIF_MAX_FRAMES) {
         rc = count > 0 ? 0 : -1;
         break;
      }
      if ((long)(count + 1) * canvasBytes > GIF_MAX_TOTAL) {
         rc = count > 0 ? 0 : -1;
         break;
      }
      if (count == cap) {
         int ncap = cap * 2;
         int *nd = (int *)realloc(delays, (size_t)ncap * sizeof(int));
         uint8_t *nf = (uint8_t *)realloc(frames, (size_t)ncap * (size_t)canvasBytes);
         if (!nd || !nf) { free(nd == delays ? NULL : nd); break; }
         delays = nd;
         frames = nf;
         cap = ncap;
      } else if (!frames) {
         frames = (uint8_t *)malloc((size_t)cap * (size_t)canvasBytes);
         if (!frames) break;
      }
      memcpy(frames + (size_t)count * (size_t)canvasBytes, canvas, (size_t)canvasBytes);
      int ms = delayCs * 10;
      if (!haveGce || ms <= 0) ms = 100;
      delays[count++] = ms;

      if (disposal == 2) {
         for (int y = 0; y < ih; y++)
            for (int x = 0; x < iw; x++)
               putPx(canvas, sw, sh, left + x, top + y, 0, 0, 0, 0);
      } else if (disposal == 3) {
         memcpy(canvas, backup, (size_t)canvasBytes);
      }
      haveGce = 0;
      disposal = 0;
      trans = 0;
      (void)bg;
   }

   free(canvas);
   free(backup);
   if (count > 0 && rc != 0) rc = 0;
   if (rc != 0 || count <= 0) {
      free(delays);
      free(frames);
      return -1;
   }
   out->w = sw;
   out->h = sh;
   out->count = count;
   out->delayMs = delays;
   out->argb = frames;
   return 0;
}
