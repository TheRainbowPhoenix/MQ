//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/interfaces/display.h>
#include <stdlib.h>
#include <string.h>

mqDisplay *mq_display_create(void)
{
    mqDisplay *d = calloc(1, sizeof *d);
    return d;
}

void mq_display_reset(mqDisplay *d)
{
    free(d->data);
    memset(d, 0, sizeof *d);
    d->dirty = true;
}

void mq_display_destroy(mqDisplay *d)
{
    mq_display_reset(d);
    free(d);
}

uint mq_display_framebufferSizeFor(mqDisplay_format fmt, uint w, uint h)
{
    switch(fmt) {
    case MQ_DISPLAY_FORMAT_L8:
        return w * h;
    case MQ_DISPLAY_FORMAT_RGB565:
        return (w * 2) * h;
    default:
        return 0;
    }
}

uint mq_display_framebufferSize(mqDisplay const *d)
{
    return mq_display_framebufferSizeFor(d->format, d->width, d->height);
}

bool mqDisplay_setFormat(mqDisplay *d, mqDisplay_format fmt, uint w, uint h)
{
    uint size = mq_display_framebufferSizeFor(fmt, w, h);

    if(d->format == fmt && d->width == w && d->height == h) {
        memset(d->data, 0x00, size);
        return true;
    }

    void *newData = calloc(1, size);
    if(!newData)
        return false;

    free(d->data);

    d->format = fmt;
    d->width = w;
    d->height = h;
    d->data = newData;
    mqDisplay_setDirty(d, true);
    return true;
}

void mqDisplay_setDirty(mqDisplay *d, bool dirty)
{
    d->dirty = dirty;
}
