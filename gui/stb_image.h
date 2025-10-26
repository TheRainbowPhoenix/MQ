// small stb_image abstraction

#ifndef MQ_UI_STB_IMAGE_H
#define MQ_UI_STB_IMAGE_H

#include <string>
using namespace std;

#include <mq/interfaces/display.h>

int stb_image_screenshot(char const *pathname, mqDisplay *display, int scale);
std::string stb_image_screenshot_err2str(int err);

#endif /* MQ_UI_STB_IMAGE_H */
