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

/* A physical keyboard key. */
struct mqKeyboardKey {
    /* Key name/label for UI display */
    char const *name;
    /* Geometric parameters for display, no influence on emulation */
    struct { int x, y, w, h; } geometry;
    /* Internal row/column layout for this keyboard. This specifies how the
       hardware module maps keys to I/O registers. */
    int row, col;
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

MQ_END_DEFS
#endif /* MQ_INTERFACES_KEYBOARD_H */
