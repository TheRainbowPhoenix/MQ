#include "./stb_image.h"

extern "C" {
#include <stb_image_write.h>
}

//=== screenshot =============================================================//

int stb_image_screenshot(char const *pathname, mqDisplay *display, int scale)
{
    if(display == NULL || display->data == NULL)
        return -1;
    mq_log(MQ_LOG_DEBUG, "try to create a screenshot...");
    int i = 0;
    int comp = 3;
    size_t r_width = display->width * scale;
    size_t r_height = display->height * scale;
    u8 *vram_data = (u8*)malloc((r_width * 3) * r_height);
    if(vram_data == NULL)
        return -2;
    if(display->format == MQ_DISPLAY_FORMAT_L8) {
        u8 *data = (u8*)display->data;
        for (size_t y = 0 ; y < r_height ; y++) {
            for (size_t x = 0 ; x < r_width ; x++) {
                size_t src_y = y / scale;
                size_t src_x = x / scale;
                vram_data[i] = data[(display->width*src_y) + src_x];
                i += 1;
            }
        }
        comp = 1;
    }
    else {
        u16 *data = (u16*)display->data;
        for (size_t y = 0 ; y < r_height ; y++) {
            for (size_t x = 0 ; x < r_width ; x++) {
                size_t src_y = y / scale;
                size_t src_x = x / scale;
                u16 color = data[(display->width*src_y) + src_x];
                vram_data[(i * 3) + 0] = ((color >> 11) & 0b011111) << 3;
                vram_data[(i * 3) + 1] = ((color >> 5)  & 0b111111) << 2;
                vram_data[(i * 3) + 2] = ((color >> 0)  & 0b011111) << 3;
                i += 1;
            }
        }
    }
    mq_log(MQ_LOG_DEBUG, "screenshot: exported at \"%s\"", pathname);
    int err = stbi_write_png(pathname, r_width, r_height, comp, vram_data, 0);
    mq_log(MQ_LOG_DEBUG, "try to create a screenshot...SUCCESS");
    free(vram_data);
    mq_log(MQ_LOG_DEBUG, "stbi_write_png(): %d", err);
    return (err == 0) ? -3 : 0;
}

std::string stb_image_screenshot_err2str(int err)
{
    switch(err) {
        case 0:
            return "success";
        case -1:
            return "no display data available";
        case -2:
            return "internal alloc fails";
        case -3:
            return "unable to export the screenshot";
        default:
            return "unknown error";
    }
}
