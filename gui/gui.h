// Main header for all UI definitions that aren't reusable utilities

#ifndef MQ_UI_GUI_H
#define MQ_UI_GUI_H

#include "windows.h"
#include "texture.h"
#include "util.h"
#include "watch.h"
#include <filesystem>
#include <vector>
#include <string>

/* All dynamic UI data. The state doesn't consist only of the data directly in
   this structure, windows have internal state too. */
struct GUI
{
    //=== Controlling files ==================================================//

    /* File that just got opened. Filled asynchronously by dialog */
    struct OpenFileBuffer inputFile;
    /* List of add-ins in CWD (detected at startup) */
    std::vector<std::string> workingFolderAddins;
    /* File tracked for reloading the currently active file when changed */
    bool watch_enabled = false;
    struct WatchInfo watch_info = { .fd = -1, .wd = -1, .addin_path = "" };

    /* Path of the file that's been requested to be opened from CLI
       TODO: This shouldn't be state, right? */
    std::filesystem::path start_path;
    /* Path of the currently-running program, "" if none. */
    std::filesystem::path current_program_path = "";

    //=== Widgets and co. ====================================================//

    /* OpenGL logic for the display window */
    DisplayGlWindow DGW;
    /* OpenGL texture for the mono or RGB display */
    Texture displayTexture;

    /* Hexadecimal viewer widget */
    ImGui::HexViewer HV;
    /* Message console widget where logs are collected */
    RichText::Text ConsoleText;

    //=== Miscellaneous ======================================================//

    /* Current emulator cycles remaining to run, -1 if running forever. */
    int mq_cycles = 0;
};

#endif /* MQ_UI_GUI_H */
