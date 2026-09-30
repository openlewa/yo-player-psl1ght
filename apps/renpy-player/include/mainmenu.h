#pragma once

// Classic-theme main menu (title background + config.main_menu image buttons), built from the
// game.gui manifest. Self-contained: it owns its art and selection, and reports the chosen
// action back to the caller, which drives the boot flow (start the game / leave). The button
// layout + frame are translated from the engine theme source -- see mainmenu.c.

#include "font.h"

typedef enum {
   MM_ACTION_NONE = 0,   // nothing chosen this frame (or an inert button: Gallery)
   MM_ACTION_START,      // "Start Game"
   MM_ACTION_LOAD,       // "Load Game" -> _intra_jumps("load_screen") (the "Continue" art button)
   MM_ACTION_PREFS,      // "Preferences" -> the in-game menu's preferences page
   MM_ACTION_QUIT        // "Quit", or the player backed out
} MmAction;

void     initMainMenu(Font *font);                    // shared font for text buttons (when a label has no art)
void     buildMainMenu(void);                         // build from the manifest + load art
void     drawMainMenu(int cx, int cy, int cw, int ch);// draw into the letterboxed content rect
MmAction updateMainMenu(int curVisible, int curX, int curY);  // poll pad + virtual cursor -> action
void     freeMainMenu(void);                          // release the art
