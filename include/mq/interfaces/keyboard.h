//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.interfaces.keyboard: Generic interface for keyboard input

#ifndef MQ_INTERFACES_KEYBOARD_H
#define MQ_INTERFACES_KEYBOARD_H

#include <mq/defs.h>
MQ_START_DEFS

/* A unified calculator key enumeration that the GUI can use to assign
   shortcuts to keyboard keys. This enumeration covers the set of keys of (at
   least) all supported models. */
enum mqKeyboardKeycode {
   /* F-keys */
   MQ_KEY_F1, MQ_KEY_F2, MQ_KEY_F3, MQ_KEY_F4, MQ_KEY_F5, MQ_KEY_F6,
   /* Main control keys */
   MQ_KEY_SHIFT, MQ_KEY_ALPHA, MQ_KEY_EXIT, MQ_KEY_MENU, MQ_KEY_OPTN,
   MQ_KEY_VARS, MQ_KEY_SETTINGS, MQ_KEY_CATALOG, MQ_KEY_TOOLS,
   MQ_KEY_EXE, MQ_KEY_OK,
   /* Arrow keys and similar directional keys */
   MQ_KEY_UP, MQ_KEY_DOWN, MQ_KEY_LEFT, MQ_KEY_RIGHT,
   MQ_KEY_PREVTAB, MQ_KEY_NEXTTAB, MQ_KEY_PAGEUP, MQ_KEY_PAGEDOWN,
};

/* A physical keyboard key. */
struct mqKeyboardKey {
    /* Key name/label for UI display */
    char const *name;
    /* Geometric parameters for display, no influence on emulation */
    struct { int x, y, w, h; } geometry;
    /* Internal row/column layout for this keyboard. This specifies how the
       hardware module maps keys to I/O registers. */
    int row, col;
    /* Associated keycode for GUI identification */
    enum mqKeyboardKeycode keycode;
};

struct mqKeyboard {
    /* Number of keys in this keyboard */
    uint keyCount;
    /* Descriptions of physical keys */
    struct mqKeyboardKey *keyInfo;
    /* ID of the power-on key */
    uint onKey;

    //=== Varying information ===//

    /* Current key states; array of length keyCount */
    u8 *keyStatus;
    /* Tracker for whether the keyboard has changed since some enumation/
       consumer-directed event. This can be set to true by users. */
    bool dirty;
};

typedef struct mqKeyboardKey mqKeyboardKey;
typedef struct mqKeyboard mqKeyboard;

/* Some standard keyboard layouts.
   TODO: Move keyboard descriptions to a model description module? */
enum mqKeyboardStandardLayout {
    // The fx/fx-CG keyboard layout, roughly.
    // TODO: Does this need variations based on model?
    MQ_KEYBOARD_STANDARD_LAYOUT_FX,
};

/* CRD functions for mqKeyboard. The default state is a keyboard with no keys
   at all, and nothing is pressed. */
mqKeyboard *mq_keyboard_create(void);
void mq_keyboard_reset(mqKeyboard *kbd);
void mq_keyboard_destroy(mqKeyboard *kbd);

/* Initialize a keyboard with a standard layout. */
void mq_keyboard_initialize(mqKeyboard *kbd, enum mqKeyboardStandardLayout l);

/* Check whether a key is pressed. */
bool mq_keyboard_isKeyPressed(mqKeyboard *kbd, uint keyNumber);
/* Get the key number of a pressed key. If no keys are pressed, returns 0. If
   multiple keys are pressed, returns a consistent but unspecified one. */
int mq_keyboard_getPressedKey(mqKeyboard *kbd);

/* Set whether a key is pressed. This should be called from GUI code. */
void mq_keyboard_setKeyPressed(mqKeyboard *kbd, uint keyNumber, bool pressed);
/* Set whether all keys associated with a standard keycode (if any) are
   pressed. This should be called from GUI code. */
void mq_keyboard_setKeycodePressed(
   mqKeyboard *kbd, enum mqKeyboardKeycode keycode, bool pressed);

MQ_END_DEFS
#endif /* MQ_INTERFACES_KEYBOARD_H */
