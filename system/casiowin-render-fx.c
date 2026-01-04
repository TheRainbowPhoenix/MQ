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

    u8 mask = 0x80 >> (x & 7);
    u8 *data = (u8*)((uintptr_t)&vramLE[(y * SCREEN_WIDTH + x) / 8] ^ 3);
    *data = (*data & ~mask) | (mask * (color != 0));
}

int mq_casiowin_mono_get_pixel(u8 *vramLE, uint x, uint y)
{
    if(x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return 0;

    u8 mask = 0x80 >> (x & 7);
    u8 *data = (u8*)((uintptr_t)&vramLE[(y * SCREEN_WIDTH + x) / 8] ^ 3);
    return (*data & mask) != 0;
}

int mq_casiowin_mono_get_vram_byte(u8 *vramLE, uint xbyte, uint y)
{
    int index = y * (SCREEN_WIDTH / 8) + xbyte;
    u8 *vramByte = (u8 *)(((uintptr_t)vramLE + index) ^ 3);
    return *vramByte;
}

void mq_casiowin_mono_set_vram_byte(u8 *vramLE, uint xbyte, uint y, int byte)
{
    int index = y * (SCREEN_WIDTH / 8) + xbyte;
    u8 *vramByte = (u8 *)(((uintptr_t)vramLE + index) ^ 3);
    *vramByte = byte;
}

//=== Base function for shape functions ======================================//

void mq_casiowin_mono_DrawShapePoint(
    u8 *vramLE, struct mqCasiowin_TShapePixelInfo *pixelinfo,
    struct mqCasiowin_TShape const *shape)
{
    int x = pixelinfo->x;
    int y = pixelinfo->y;
    if((uint)x >= 128 || (uint)y >= 64)
        return;

    int xoff = x & 7;
    int input_byte;
    int output_byte = 0;

    /* Input filter: for dashed lines, apply the dash pattern */
    if(shape->type == MQ_CASIOWIN_SHAPE_ON_OFF_LINE ||
       shape->type == MQ_CASIOWIN_SHAPE_OFF_ON_LINE) {
        int dash_current =
            pixelinfo->dash_counter % (shape->on_bits + shape->off_bits);

        if(shape->type == MQ_CASIOWIN_SHAPE_ON_OFF_LINE) {
            if(dash_current <= shape->on_bits && dash_current != 0)
                output_byte = (0x80 >> xoff);
        }
        else {
            if(shape->off_bits < dash_current || dash_current == 0)
                output_byte = (0x80 >> xoff);
        }
    }
    /* For other shapes,  use the draw mode */
    else {
        if(shape->f2 == 1)
            output_byte = (0x80 >> xoff);
        else if(shape->f2 == 2)
            output_byte = 0;
        else if(shape->f2 == 3)
            output_byte = (0x80 >> xoff) & (0xaa >> (y & 1));
    }

    int mode = pixelinfo->mode;

    if((mode & MQ_CASIOWIN_SHAPE_MODE_VRAM) == 0) {
        mq_log(MQ_LOG_ERROR, "ShapeToDD: Can't read from DD yet");
        input_byte = 0;
        // input_byte = DD_ReadByte(x >> 3, y);
    }
    else {
        input_byte = mq_casiowin_mono_get_vram_byte(vramLE, x >> 3, y);
    }

    /* Output filter */
    if(shape->f3 == 1) {
        if(output_byte)
            output_byte |= input_byte;
        else
            output_byte = input_byte & ~(0x80 >> xoff);
    }
    else if(shape->f3 == 2)
        output_byte |= input_byte;
    else if(shape->f3 == 3)
        output_byte = input_byte & (output_byte | ~(0x80 >> xoff));
    else if(shape->f3 == 4)
        output_byte = ((input_byte ^ output_byte) & (0x80 >> xoff))
                    | (input_byte & ~(0x80 >> xoff));

    /* Output the pixel */
    if(mode & MQ_CASIOWIN_SHAPE_MODE_VRAM)
        mq_casiowin_mono_set_vram_byte(vramLE, x >> 3, y, output_byte);
    if(mode & MQ_CASIOWIN_SHAPE_MODE_DD) {
        mq_log(MQ_LOG_ERROR, "ShapeToDD: Can't write to DD yet");
        // DD_WriteByte(x >> 3, y, output_byte);
    }
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
        mq_machine_setStuck(mach);
    }

    while(x < SCREEN_WIDTH) {
        int codePoint = nextCodePoint(mach, &stringAddress);
        if(codePoint == 0)
            break;

        draw_character(Casiowin->vramLE, codePoint, x, y, mode);
        x += CHAR_WIDTH;
    }
}

//=== Display access =========================================================//

void mq_casiowin_mono_dupdate(mqMachine *mach)
{
    mqCasiowin *Casiowin = mq_casiowin_get(mach);

    if(!mq_display_setFormat(mach->display, MQ_DISPLAY_FORMAT_L8, 128, 64))
        return;

    u8 *src = mq_memory_access(mach->memory, Casiowin->dataVramAddresses[0]);
    u8 *dst = mach->display->data;
    for(int p = 0; p < 1024; p++) {
        u8 value = *(u8*)((uintptr_t)(src++) ^ 3);
        for(int i = 0; i < 8; i++) {
            *dst++ = ~((i8)value >> 7);
            value <<= 1;
        }
    }
    mq_display_setDirty(mach->display, true);
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
