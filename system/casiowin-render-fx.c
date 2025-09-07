//--  ____  -----------------------------------------------------------------//
//   [>___] /010  MQ: A user/hardware-level emulator for CASIO calculators.  //
//   |...+|/110   Written by Lephe' with help from Yatis.                    //
//   |:::o/010    License: MIT <https://opensource.org/licenses/MIT>         //
//-- `---/101 ---------------------------------------------------------------//

#include <mq/machine.h>
#include <mq/memory.h>
#include <mq/system/casiowin.h>
#include "autogen/assets.h"
#include <string.h>

/* Screen dimensions, in pixels. Width must be a multiple of 8. */
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
/* Size of the framebuffer in memory. */
#define VRAM_SIZE       (SCREEN_WIDTH * SCREEN_HEIGHT / 8)
/* Character width for the standard font. */
#define CHAR_WIDTH      6
#define CHAR_HEIGHT     8

#define get_bit(value, bit) (((value) >> (bit)) & 1)

//=== Basic rendering primitives =============================================//

void mq_casiowin_mono_set_pixel(u8 *vramLE, uint x, uint y, int color)
{
    if(x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;

    int index = ((y * SCREEN_WIDTH + x) / 8) ^ 3;
    u8 mask = 0x80 >> (x & 7);
    vramLE[index] = (vramLE[index] & ~mask) | (mask * (color != 0));
}

int mq_casiowin_mono_get_pixel(u8 *vramLE, uint x, uint y)
{
    if(x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return 0;

    int index = ((y * SCREEN_WIDTH + x) / 8) ^ 3;
    u8 mask = 0x80 >> (x & 7);
    return (vramLE[index] & mask) != 0;
}

//=== Text rendering =========================================================//

// PrintXY drawing modes
#define TEXT_NORMAL  0
#define TEXT_REVERSE 1

// PrintMini drawing modes
#define MINI_OVER    0x10
#define MINI_OR      0x11
#define MINI_REV     0x12
#define MINI_REVOR   0x13

static u8 get_mini_char_width(u8 const *char_data) {
    return get_bit(char_data[6], 5) | (get_bit(char_data[6], 6) << 1) | (get_bit(char_data[6], 7) << 2);
}

static int nextCodePoint(mqMachine *mach, u32 *addr)
{
    u32 byte;
    if(!mq_memory_read8(mach, mach->memory, (*addr)++, &byte))
        return 0;

    int codePoint = 0;
    if(byte == 0xe5) codePoint = 0x100;
    if(byte == 0xe6) codePoint = 0x200;
    if(byte == 0x7f) codePoint = 0x300;

    if(codePoint && !mq_memory_read8(mach, mach->memory, (*addr)++, &byte))
        return 0;

    return codePoint + byte;
}

static u8 const *get_character_data(int codePoint, u8 const *char_data)
{
    return char_data + codePoint * 7;
}

static void draw_character(
    u8 *vramLE, int codePoint, int x0, int y0, int mode)
{
    if(mode != TEXT_NORMAL && mode != TEXT_REVERSE) {
        mq_log(MQ_LOG_WARNING, "draw_character_mini: Invalid mode: 0x%x", mode);
        mode = 0;
    }

    u8 const *char_data = get_character_data(codePoint, mq_assets_fx_font);

    for(int y = 0; y < CHAR_HEIGHT; y++) {
        for(int x = 0; x < CHAR_WIDTH; x++) {
            int vram_x = x0 + x;
            int vram_y = y0 + y;
            if(vram_x < 0 || vram_x >= SCREEN_WIDTH || vram_y < 0 || vram_y >= SCREEN_HEIGHT) break;

            int char_pixel = (x == 0 || y == CHAR_HEIGHT - 1) ? 0 : get_bit(char_data[y], x - 1);

            if(mode == TEXT_REVERSE) char_pixel = !char_pixel;

            mq_casiowin_mono_set_pixel(vramLE, vram_x, vram_y, char_pixel);
        }
    }
}

static u8 draw_character_mini(
    u8 *vramLE, int codePoint, int x0, int y0, int mode)
{
    if(mode < MINI_OVER || mode > MINI_REVOR) {
        mq_log(MQ_LOG_WARNING, "draw_character_mini: Invalid mode: %x", mode);
        mode = 0;
    }

    u8 const *char_data = get_character_data(codePoint, mq_assets_fx_font_mini);
    u8 width = get_mini_char_width(char_data);

    for(int y = 0; y < 6; y++) {
        for(int x = 0; x < width; x++) {
            int vram_x = x0 + x;
            int vram_y = y0 + y;
            if(vram_x < 0 || vram_x >= SCREEN_WIDTH || vram_y < 0 || vram_y >= SCREEN_HEIGHT) break;

            int vram_pixel = mq_casiowin_mono_get_pixel(vramLE, vram_x, vram_y);
            int char_pixel = get_bit(char_data[y], x);

            if      (mode == MINI_OVER)  ; // char_pixel = char_pixel;
            else if(mode == MINI_OR)    char_pixel = char_pixel || vram_pixel;
            else if(mode == MINI_REV)   char_pixel = !char_pixel;
            else if(mode == MINI_REVOR) char_pixel = !char_pixel || vram_pixel;

            mq_casiowin_mono_set_pixel(vramLE, vram_x, vram_y, char_pixel);
        }
    }

    return width;
}

void mq_casiowin_mono_Print(mqMachine *mach, u32 stringAddress, int max)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);

    while(Casiowin->BdispCursorX <= 21 && Casiowin->BdispCursorX < max - 1) {
        int codePoint = nextCodePoint(mach, &stringAddress);
        if(codePoint == 0)
            break;

        // Pixel start x,y on screen (128x64)
        int screen_x = Casiowin->BdispCursorX * CHAR_WIDTH;
        int screen_y = Casiowin->BdispCursorY * CHAR_HEIGHT;

        // Draw the character on the VRAM
        draw_character(Casiowin->vramLE, codePoint, screen_x, screen_y, 0);

        // Move the cursor to the right
        Casiowin->BdispCursorX++;
    }
}

void mq_casiowin_mono_PrintMini(
    mqMachine *mach, int x, int y, u32 stringAddress, int mode)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);

    mode |= 0x10; // Values in 0-3 range seem to also work on the real device

    while(x < SCREEN_WIDTH) {
        int codePoint = nextCodePoint(mach, &stringAddress);
        if(codePoint == 0)
            break;

        x += draw_character_mini(Casiowin->vramLE, codePoint, x, y, mode);
    }
}

void mq_casiowin_mono_PrintXY(
    mqMachine *mach, int x, int y, u32 stringAddress, int mode)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    // printf("Run syscall: PrintXY (%d %d %s %d)\n", x, y, str, mode);

    mode = mode & 3; // Only keep the first 2 bits

    if(mode != TEXT_NORMAL && mode != TEXT_REVERSE) {
        mq_log(MQ_LOG_WARNING, "PrintXY mode not supported: %x", mode);
        mach->stuck = true;
    }

    while(x < SCREEN_WIDTH) {
        int codePoint = nextCodePoint(mach, &stringAddress);
        if(codePoint == 0)
            break;

        draw_character(Casiowin->vramLE, codePoint, x, y, mode);
        x += CHAR_WIDTH;
    }
}

//=== SaveDisp and RestoreDisp ===============================================//

#define SAVEDISP_PAGE1 1
#define SAVEDISP_PAGE2 5
#define SAVEDISP_PAGE3 6

static int SaveDisp_vramBufferId(int id)
{
    switch(id) {
    case SAVEDISP_PAGE1: return 1;
    case SAVEDISP_PAGE2: return 2;
    case SAVEDISP_PAGE3: return 3;
    default: return -1;
    }
}

void mq_casiowin_mono_SaveDisp(mqMachine *mach, int id)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    id = SaveDisp_vramBufferId(id);
    if(id < 0)
        return;

    /* As long as VRAMs are 4-aligned we can just copy contiguously */
    void *src = mq_memory_access(mach->memory, Casiowin->dataVramAddresses[0]);
    void *dst = mq_memory_access(mach->memory, Casiowin->dataVramAddresses[id]);
    memcpy(dst, src, Casiowin->info->dataVramSize);
}

void mq_casiowin_mono_RestoreDisp(mqMachine *mach, int id)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);
    id = SaveDisp_vramBufferId(id);
    if(id < 0)
        return;

    /* As long as VRAMs are 4-aligned we can just copy contiguously */
    void *src = mq_memory_access(mach->memory, Casiowin->dataVramAddresses[id]);
    void *dst = mq_memory_access(mach->memory, Casiowin->dataVramAddresses[0]);
    memcpy(dst, src, Casiowin->info->dataVramSize);
}
