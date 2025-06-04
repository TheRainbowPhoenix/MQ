const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int CHAR_WIDTH = 6;
const int CHAR_HEIGHT = 8;

#define VRAM_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 8 + 1)
#define CHARACTER_SET_SIZE  (4 * 256 * 7)

#define get_bit(value, bit) (((value) >> (bit)) & 1)
#define update_bit(value, bit, new_bit) ((value) = ((value) & ~(1 << (bit))) | ((new_bit) << (bit)))
// #define set_bit(value, bit) ((value) |= (1 << (bit)))
// #define clear_bit(value, bit) ((value) &= ~(1 << (bit)))
// #define toggle_bit(value, bit) ((value) ^= (1 << (bit)))

//////   [ MEMORY ACCESS ]   //////

const unsigned char *mq_memory_read_str(mqMachine *mach, u32 addr) 
{
    char *src = mq_memory_access(mach->memory, addr);
    char *dst = malloc(sizeof(char) * 256);
    
    for (int i = 0; i < 255; i++) {
        uint src_i = i + 3 - 2 * (i % 4);

        dst[i] = src[src_i];

        if (dst[i] == '\0') {
            break;
        }
    }

    return (const unsigned char*)dst;
}

void vram_write_pixel(mqMachine *mach, int x, int y, int val)
{
    u8 *vram = mq_memory_access(mach->memory, 0x8c000000);

    int id = (y * 128) + x;
    int vram_id = id / 8;

    int m = vram_id % 4;
    vram_id = (vram_id-m) + (3-m);

    int vram_bit = 7 - x % 8;

    update_bit(vram[vram_id], vram_bit, val ? 1 : 0);
}

u8 vram_read_pixel(mqMachine *mach, int x, int y) 
{
    u8 *vram = mq_memory_access(mach->memory, 0x8c000000);

    int id = (y * 128) + x;
    int vram_id = id / 8;

    int m = vram_id % 4;
    vram_id = (vram_id-m) + (3-m);

    int vram_bit = 7 - x % 8;

    return get_bit(vram[vram_id], vram_bit);
}

//////   [ TEXT UTILS ]   //////

// PrintXY drawing modes
#define TEXT_NORMAL  0
#define TEXT_REVERSE 1

// PrintMini drawing modes
#define MINI_OVER    0x10
#define MINI_OR      0x11
#define MINI_REV     0x12
#define MINI_REVOR   0x13

bool is_multibyte_char(const unsigned char c) {
    return c == 0xE5 || c == 0xE6 || c == 0x7F;
}

u8 get_mini_char_width(u8 *char_data) {
    return get_bit(char_data[6], 5) | (get_bit(char_data[6], 6) << 1) | (get_bit(char_data[6], 7) << 2);
}

u8 *get_character_data(const unsigned char *c, u8 *char_data)
{
    // Calculate offset in the character set for 2-byte characters
    if (is_multibyte_char(*c)) {
        if (*c == 0xE5) char_data += CHARACTER_SET_SIZE / 4;
        else if (*c == 0xE6) char_data += 2 * CHARACTER_SET_SIZE / 4;
        else if (*c == 0x7F) char_data += 3 * CHARACTER_SET_SIZE / 4;
        char_data += c[1] * 7;
    }
    else {
        char_data += c[0] * 7;
    }

    return char_data;
}

void draw_character(mqMachine *mach, const unsigned char *c, int pixel_start_x, int pixel_start_y, int mode)
{
    if (mode != TEXT_NORMAL && mode != TEXT_REVERSE) {
        mq_log(MQ_LOG_WARNING, "draw_character_mini: Invalid mode: 0x%x", mode);
        mode = 0;
    }

    u8 *char_data = get_character_data(c, mach->characterSet);

    for (int y = 0; y < CHAR_HEIGHT; y++) {
        for (int x = 0; x < CHAR_WIDTH; x++) {
            int vram_x = pixel_start_x + x;
            int vram_y = pixel_start_y + y;
            if (vram_x < 0 || vram_x >= SCREEN_WIDTH || vram_y < 0 || vram_y >= SCREEN_HEIGHT) break;

            int char_pixel = (x == 0 || y == CHAR_HEIGHT - 1) ? 0 : get_bit(char_data[y], x - 1);

            if (mode == TEXT_REVERSE) char_pixel = !char_pixel;

            vram_write_pixel(mach, vram_x, vram_y, char_pixel);
        }
    }
}

u8 draw_character_mini(mqMachine *mach, const unsigned char *c, int pixel_start_x, int pixel_start_y, int mode)
{
    if (mode < MINI_OVER || mode > MINI_REVOR) {
        mq_log(MQ_LOG_WARNING, "draw_character_mini: Invalid mode: %x", mode);
        mode = 0;
    }

    u8 *char_data = get_character_data(c, mach->characterSetMini);
    u8 width = get_mini_char_width(char_data);

    for (int y = 0; y < 6; y++) {
        for (int x = 0; x < width; x++) {
            int vram_x = pixel_start_x + x;
            int vram_y = pixel_start_y + y;
            if (vram_x < 0 || vram_x >= SCREEN_WIDTH || vram_y < 0 || vram_y >= SCREEN_HEIGHT) break;

            int vram_pixel = vram_read_pixel(mach, vram_x, vram_y);
            int char_pixel = get_bit(char_data[y], x);

            if      (mode == MINI_OVER)  ; // char_pixel = char_pixel;
            else if (mode == MINI_OR)    char_pixel = char_pixel || vram_pixel;
            else if (mode == MINI_REV)   char_pixel = !char_pixel;
            else if (mode == MINI_REVOR) char_pixel = !char_pixel || vram_pixel;

            vram_write_pixel(mach, vram_x, vram_y, char_pixel);
        }
    }

    return width;
}

void syscall_Print(mqMachine *mach, int str_addr, int max)
{
    const unsigned char* str = mq_memory_read_str(mach, str_addr);
    
    int i = 0;
    while(mach->locX <= 21 && mach->locX < max) {
        const unsigned char *c = &str[i++]; // Current character

        if (*c == 0x00) break; // Line terminator

        // Pixel start x,y on screen (128x64)
        int screen_x = (mach->locX - 1) * CHAR_WIDTH;
        int screen_y = (mach->locY - 1) * CHAR_HEIGHT;

        // Draw the character on the VRAM
        draw_character(mach, c, screen_x, screen_y, 0);
        
        // Move the cursor to the right
        mach->locX++;

        // Skip the second byte of a 2-byte character
        if (is_multibyte_char(*c)) i++;
    }
}

void syscall_PrintMini(mqMachine *mach, int x, int y, int str_addr, int mode)
{
    const unsigned char *str = mq_memory_read_str(mach, str_addr);
    // mq_log(MQ_LOG_DEBUG, "Run syscall PrintMini: %s", str);

    mode |= 0x10; // Values in 0-3 range seem to also work on the real device

    int i = 0;
    while (x < SCREEN_WIDTH) {
        const unsigned char *c = &str[i++]; // Current character

        if (*c == 0x00) break; // Line terminator

        // Draw the character
        int char_width = draw_character_mini(mach, c, x, y, mode);
        
        // Move to the right 
        x += char_width;

        // Skip the second byte of a 2-byte character
        if (is_multibyte_char(*c)) i++;
    }
}

void syscall_PrintXY(mqMachine *mach, int x, int y, u32 str_ptr, int mode)
{
    const unsigned char *str = mq_memory_read_str(mach, str_ptr);
    // printf("Run syscall: PrintXY (%d %d %s %d)\n", x, y, str, mode);

    mode = mode & 3; // Only keep the first 2 bits

    if (mode != TEXT_NORMAL && mode != TEXT_REVERSE) {
        mq_log(MQ_LOG_WARNING, "PrintXY mode not supported: %x", mode);
        mach->stuck = true;
    }

    int i = 0;
    while (x < SCREEN_WIDTH) {
        const unsigned char *c = &str[i++]; // Current character

        if (*c == 0x00) break; // Line terminator

        draw_character(mach, c, x, y, mode);

        if (is_multibyte_char(*c)) {
            // i++;
            mq_log(MQ_LOG_WARNING, "PrintXY: multibyte character not supported: %x", c);
            mach->stuck = true;
        }

        x += CHAR_WIDTH; // Move the cursor the the right
    }
}

//////   [ SaveDisp / RestoreDisp ]   //////

#define SAVEDISP_PAGE1 1
#define SAVEDISP_PAGE2 5
#define SAVEDISP_PAGE3 6

void syscall_SaveDisp(mqMachine *mach, int id)
{
    id = (id == SAVEDISP_PAGE1 ? id - 1 : id - 4);

    if (mach->savedDisps[id] == NULL) {
        mach->savedDisps[id] = malloc(VRAM_SIZE);
    }

    u8 *src = mq_memory_access(mach->memory, 0x8c000000);
    u8 *dst = mach->savedDisps[id];

    memcpy(dst, src, VRAM_SIZE);
}

void syscall_RestoreDisp(mqMachine *mach, int id)
{
    id = (id == SAVEDISP_PAGE1 ? id - 1 : id - 4);

    u8 *src = mach->savedDisps[id];
    u8 *dst = mq_memory_access(mach->memory, 0x8c000000);

    memcpy(dst, src, VRAM_SIZE);
}