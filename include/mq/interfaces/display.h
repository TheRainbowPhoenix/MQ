//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//
// mq.interfaces.display: Generic interface for display devices

#ifndef MQ_INTERFACES_DISPLAY_H
#define MQ_INTERFACES_DISPLAY_H

#include <mq/defs.h>
MQ_START_DEFS

/* Framebuffer storage formats. Stride is currently not allowed. */
enum mqDisplay_format {
    /* 8-bit luminance. This is used for mono (with grayscale). */
    MQ_DISPLAY_FORMAT_L8,
    /* 16-bit RGB565, host-endian. Used for the modern color displays. */
    MQ_DISPLAY_FORMAT_RGB565,
};

typedef enum mqDisplay_format mqDisplay_format;

/* A display framebuffer. This is the interface between emulated devices and
   other (UI) code. */
struct mqDisplay {
    /* Pixel storage and color format */
    mqDisplay_format format;
    /* Framebuffer dimensions */
    uint width, height;
    /* Pointer to raw pixel values in row-major, left-to-right order */
    void *data;
    /* Tracker for whether the display has changed since some UI/consumer-
       directed event. This can be set to false by users. */
    bool dirty;
};

typedef struct mqDisplay mqDisplay;

/* CRD functions for mqDisplay. The default state is dimensions 0x0 with a
   NULL data pointer. */
mqDisplay *mq_display_create(void);
void mq_display_reset(mqDisplay *display);
void mq_display_destroy(mqDisplay *display);

/* Compute the expected framebuffer size in bytes for a given format/dimensions
   and for a given display directly.. */
uint mq_display_framebufferSizeFor(mqDisplay_format fmt, uint w, uint h);
uint mq_display_framebufferSize(mqDisplay const *display);

/* Change the framebuffer's allocation, if needed, to match the requested
   format and size. The new framebuffer is zero-initialized. If the same format
   and size are specified multiple times, the buffer is not reallocated but the
   contents are still cleared. Returns false on allocation failure. */
bool mq_display_setFormat(mqDisplay *d, mqDisplay_format fmt, uint w, uint h);

/* Set the display's dirty bit. This is intended to be used in emulation code
   to set the bit (when the display is modified) and in UI code to clear the
   bit (once screen textures have been updated). */
void mq_display_setDirty(mqDisplay *d, bool dirty);

MQ_END_DEFS
#endif /* MQ_INTERFACES_DISPLAY_H */
