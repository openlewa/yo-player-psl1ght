#include "mainmenu.h"

#include <string.h>

#include "gfx.h"
#include "colors.h"
#include "pad.h"
#include "gui.h"
#include "assets.h"
#include "config.h"        // RR12G_PATH / RR6G_PATH
#include "ui/slice.h"
#include "focus.h"         // geometric focus navigation (the engine's, shared by all menus)
#include "dbg.h"
#include "font.h"

typedef struct {
   GfxTexture idle, hover;
   int hasIdle, hasHover;
   MmAction action;
   const char *label;
   TextTexture text;
} MmButton;

static GfxTexture mmBgTex;    static int mmBgLoaded;
static GfxTexture mmFrameTex; static int mmFrameLoaded, mmFrameCap;
static MmButton   mmBtn[GUI_MMBTN_MAX]; static int mmBtnN;
static Font      *mmFont;
static int        mmTextCw;

void initMainMenu(Font *sharedFont) { mmFont = sharedFont; }

void freeMainMenu(void)
{
   if (mmBgLoaded) { freeAssetTexture(&mmBgTex); mmBgLoaded = 0; }
   if (mmFrameLoaded) { freeAssetTexture(&mmFrameTex); mmFrameLoaded = 0; }
   for (int i = 0; i < mmBtnN; i++)
   {
      if (mmBtn[i].hasIdle) freeAssetTexture(&mmBtn[i].idle);
      if (mmBtn[i].hasHover) freeAssetTexture(&mmBtn[i].hover);
      freeTextTexture(&mmBtn[i].text);
   }
   mmBtnN = 0;
   mmTextCw = 0;
}

static MmAction actionFor(const char *label)
{
   if (strcmp(label, "Start Game") == 0) return MM_ACTION_START;
   if (strcmp(label, "Load Game") == 0)  return MM_ACTION_LOAD;
   if (strcmp(label, "Preferences") == 0) return MM_ACTION_PREFS;
   if (strcmp(label, "Quit") == 0)       return MM_ACTION_QUIT;
   return MM_ACTION_NONE;
}

static void addMenuButton(const char *label)
{
   if (!label || !label[0] || mmBtnN >= GUI_MMBTN_MAX) return;
   for (int i = 0; i < mmBtnN; i++)
      if (mmBtn[i].label && strcmp(mmBtn[i].label, label) == 0) return;
   MmAction action = actionFor(label);
   int manifestIdx = -1;
   for (int i = 0; i < gui.mmBtnCount; i++)
      if (strcmp(gui.mmBtnLabel[i], label) == 0) { manifestIdx = i; break; }
   MmButton *button = &mmBtn[mmBtnN];
   memset(button, 0, sizeof *button);
   button->label = label;
   button->action = action;
   if (manifestIdx >= 0)
   {
      button->hasIdle = loadAssetTexture(gui.mmBtnIdle[manifestIdx], &button->idle);
      if (button->hasIdle && gui.mmBtnHover[manifestIdx][0])
         button->hasHover = loadAssetTexture(gui.mmBtnHover[manifestIdx], &button->hover);
   }
   if (!button->hasIdle && action == MM_ACTION_NONE) return;   // custom label, no art -> skip
   mmBtnN++;
}

// Builds the menu from the game's actual config.main_menu (manifest mm_order). A label with
// button art is an image button. Start / Load / Preferences / Quit are still shown as text
// buttons when the game ships no art (modern gui themes have none). Falls back to that
// engine-default order when the manifest doesn't carry mm_order, and again if the order
// never produced a Start button.
void buildMainMenu(void)
{
   static const char *fallback[] = { "Start Game", "Load Game", "Preferences", "Quit" };
   freeMainMenu();
   int useOrder = gui.mmOrderCount > 0;
   int count = useOrder ? gui.mmOrderCount : (int)(sizeof fallback / sizeof fallback[0]);
   for (int idx = 0; idx < count && mmBtnN < GUI_MMBTN_MAX; idx++)
      addMenuButton(useOrder ? gui.mmOrder[idx] : fallback[idx]);
   int haveStart = 0;
   for (int i = 0; i < mmBtnN; i++) if (mmBtn[i].action == MM_ACTION_START) haveStart = 1;
   if (!haveStart)
      for (int i = 0; i < (int)(sizeof fallback / sizeof fallback[0]); i++) addMenuButton(fallback[i]);
   mmBgLoaded = gui.mmBg[0] ? loadAssetTexture(gui.mmBg, &mmBgTex) : 0;

   // RoundRect template for the menu frame: rr12g (native > 640) else rr6g -- the engine PNG
   // shipped in the player's res\, tinted by the frame colour at draw time (= OneOrTwoColor).
   // Skipped when the game re-parented mm_menu_frame to style.default (no frame box / padding).
   mmFrameLoaded = 0;
   if (!gui.mmFrameNone)
   {
      if (gui.nativeW > 0 && gui.nativeW <= 640) { mmFrameTex = loadGfxTexture(RR6G_PATH);  mmFrameCap = 6;  }
      else                                       { mmFrameTex = loadGfxTexture(RR12G_PATH); mmFrameCap = 12; }
      mmFrameLoaded = (mmFrameTex.w > 0 && mmFrameTex.h > 0);
   }
   clearFocus();
   if (mmBtnN > 0) setFocus(0);   // Start is selected before the first draw registers its rect
   logInfo("[rpp] main menu: %d buttons, bg=%d frame=%d\n", mmBtnN, mmBgLoaded, mmFrameLoaded);
}

static void ensureMenuText(int cw)
{
   if (!mmFont || mmTextCw == cw) return;
   int size = getGuiGmTextSize(cw);
   for (int i = 0; i < mmBtnN; i++)
   {
      if (!mmBtn[i].label) continue;
      renderFont(&mmBtn[i].text, mmFont, size, mmBtn[i].label, gui.gmBtnText, cw / 2, TEXT_NOWRAP);
   }
   mmTextCw = cw;
}

void drawMainMenu(int cx, int cy, int cw, int ch)
{
   clearGfx(0xFF000000);
   if (mmBgLoaded)
      drawGuiBackdrop(cx, cy, cw, ch, mmBgTex);

   if (mmBtnN == 0) return;
   ensureMenuText(cw);

   // Everything below is TRANSLATED from the classic theme source, not eyeballed:
   //   _layout/classic_main_menu.rpym : mm_menu_frame at xpos 5/6 (xanchor 0.5), ypos 0.9
   //       (yanchor 1.0); the buttons are a style.vbox (spacing 0), equal width (size_group).
   //   mm_menu_frame -> menu_frame -> style.frame, whose roundrect_frames() sets background to
   //       RoundRect(frame) = Frame(rrNg.png, N, N) (N = 6 if native<=640 else 12) tinted by
   //       `frame` (default (100,150,200,255), or the manifest), with xpadding = ypadding = 6.
   //       Drawn as a scaled 9-slice of the engine template, tinted -- exactly RoundRect/
   //       OneOrTwoColor, with the template's real shading + AA corners.
   // Positions are fractions of the native screen, applied to the content rect.
   float assetScale = getGuiAssetScale(cw);
   float scale = getGuiScale(cw);
   int pad = gui.mmFrameNone ? 0 : (int)(6 * scale + 0.5f);

   int btnW[GUI_MMBTN_MAX], btnH[GUI_MMBTN_MAX], maxBtnW = 0, totalBtnH = 0;
   int textPadX = (int)(18 * scale + 0.5f), textPadY = (int)(8 * scale + 0.5f);
   for (int i = 0; i < mmBtnN; i++)
   {
      if (mmBtn[i].hasIdle)
      {
         btnW[i] = (int)(mmBtn[i].idle.w * assetScale + 0.5f);
         btnH[i] = (int)(mmBtn[i].idle.h * assetScale + 0.5f);
      }
      else
      {
         int tw = mmBtn[i].text.valid ? mmBtn[i].text.tex.w : (int)(160 * scale);
         int th = mmBtn[i].text.valid ? mmBtn[i].text.tex.h : (int)(22 * scale);
         btnW[i] = tw + 2 * textPadX;
         btnH[i] = th + 2 * textPadY;
      }
      if (btnW[i] > maxBtnW) maxBtnW = btnW[i];
      totalBtnH += btnH[i];
   }
   int frameW = maxBtnW + 2 * pad, frameH = totalBtnH + 2 * pad;
   int colCenterX  = cx + (int)(cw * (5.0f / 6.0f) + 0.5f);   // xpos 5/6, xanchor 0.5
   int frameBottom = cy + (int)(ch * 0.9f + 0.5f);            // ypos 0.9, yanchor 1.0
   int frameX = colCenterX - frameW / 2;
   int frameY = frameBottom - frameH;

   if (mmFrameLoaded)
   {
      SpriteRegion whole = { 0, 0, mmFrameTex.w, mmFrameTex.h };
      NineSlice frameSlice;
      initNineSliceScaled(&frameSlice, mmFrameTex, frameX, frameY, frameW, frameH, whole, mmFrameCap, mmFrameCap, scale);
      frameSlice.tint = gui.mmFrameColor;   // recolour the grayscale-gradient template
      drawNineSlice(&frameSlice);
   }

   beginFocusFrame();
   int drawY = frameY + pad;
   for (int i = 0; i < mmBtnN; i++)
   {
      int btnX = colCenterX - btnW[i] / 2;                // xanchor 0.5 (centred in column)
      addFocus(i, btnX, drawY, btnW[i], btnH[i]);
      int focused = isFocused(i);
      if (mmBtn[i].hasIdle)
      {
         GfxTexture tex = (focused && mmBtn[i].hasHover) ? mmBtn[i].hover : mmBtn[i].idle;
         if (focused && !mmBtn[i].hasHover)               // selection cue when there's no hover art
            fillGfxRectangle(btnX - 4, drawY - 2, btnW[i] + 8, btnH[i] + 4, 0x55FFFFFF);
         drawGfxTexture(btnX, drawY, btnW[i], btnH[i], tex, 0.0f, 0.0f, 1.0f, 1.0f, COLOR_WHITE, GFX_FILTER_LINEAR);
      }
      else
      {
         uint32_t tint = focused ? gui.gmBtnHover : gui.gmBtnIdle;
         if (mmFrameLoaded)
         {
            SpriteRegion whole = { 0, 0, mmFrameTex.w, mmFrameTex.h };
            NineSlice btnSlice;
            initNineSliceScaled(&btnSlice, mmFrameTex, btnX, drawY, btnW[i], btnH[i], whole, mmFrameCap, mmFrameCap, scale);
            btnSlice.tint = tint;
            drawNineSlice(&btnSlice);
         }
         else fillGfxRectangle(btnX, drawY, btnW[i], btnH[i], tint);
         if (mmBtn[i].text.valid)
         {
            int tx = btnX + (btnW[i] - mmBtn[i].text.tex.w) / 2;
            int ty = drawY + (btnH[i] - mmBtn[i].text.tex.h) / 2;
            drawGfxTexture(tx, ty, mmBtn[i].text.tex.w, mmBtn[i].text.tex.h, mmBtn[i].text.tex,
                           0.0f, 0.0f, 1.0f, 1.0f, COLOR_WHITE, GFX_FILTER_LINEAR);
         }
      }
      drawY += btnH[i];
   }
}

// Drive the menu from BOTH the d-pad (geometric focus nav) and the virtual cursor (mouse focus). When
// the cursor is visible we focus the button under it (focusAt = Ren'Py point-in-rect mouse focus), so
// X activates whatever it points at; a click over empty space does nothing, like a real mouse. The
// button rects come from drawMainMenu's addFocus calls (one frame old -- invisible lag). curX/curY are
// already in screen pixels (the content-rect offset is baked into the rects we registered).
MmAction updateMainMenu(int curVisible, int curX, int curY)
{
   if (isPadButtonPressed(PAD_BTN_CIRCLE)) return MM_ACTION_QUIT;   // O backs out to the selector
   if (mmBtnN == 0) return MM_ACTION_NONE;

   int hoverId = curVisible ? focusAt(curX, curY) : -1;
   if (hoverId >= 0) setFocus(hoverId);                      // cursor hover highlights the button under it

   if (isPadButtonPressed(PAD_BTN_UP))    moveFocus(0, -1);   // the engine's geometric focus nav
   if (isPadButtonPressed(PAD_BTN_DOWN))  moveFocus(0,  1);
   if (isPadButtonPressed(PAD_BTN_LEFT))  moveFocus(-1, 0);
   if (isPadButtonPressed(PAD_BTN_RIGHT)) moveFocus(1,  0);
   if (isPadButtonPressed(PAD_BTN_CROSS))
   {
      if (curVisible && hoverId < 0) return MM_ACTION_NONE;  // click in empty space = nothing (mouse)
      int id = getFocusId();
      if (id >= 0 && id < mmBtnN) return mmBtn[id].action;   // NONE for inert buttons
   }
   return MM_ACTION_NONE;
}
