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
#include <optional>

/* The GUI state and actions can be accessed globally. */
struct GUI;
extern struct GUI gui;

/* All commands queued in render and executed in update. Resets every frame. */
struct GUIActions
{
    /* Close the entire application. */
    bool appQuit = false;
    /* Clear the message console. */
    bool appClearConsole = false;

    /* Replace currently-running program with this file. */
    std::optional<fs::path> fileLoadPath;
    /* Update the inotify watch on the currently-running add-in. */
    bool fileUpdateWatch = false;

    /* Initialize machine with given initializeKind. */
    std::optional<int> machineInitialize;
    /* Set machine's pendingCycles count. */
    std::optional<int> machineSetPendingCycles;
    /* Generate a mono or RGB frame on the display. */
    bool machineGenerateMonoFrame = false;
    bool machineGenerateRGBFrame = false;
    /* Set whether the MMU should be bound or unbound. */
    bool machineMMUBind = false;
    bool machineMMUUnbind = false;
    /* Initialize the OS heap */
    bool machineSystemHeapInitialize = false;

    /* Set the hex editor to visualize a given region.
       TODO: Why does this use a direct pointer into emulated structures? */
    struct ViewHex { mqMemoryBuffer *buffer; int offset, size; u32 address; };
    std::optional<ViewHex> viewHex;
    void setViewHex(mqMemoryBuffer *buffer, int offset, int size, u32 address) {
        viewHex = ViewHex { buffer, offset, size, address };
    }
};

/* All dynamic UI data. The state doesn't consist only of the data directly in
   this structure, windows have internal state too. */
struct GUI
{
    struct GUIActions actions;

    //=== Controlling files ==================================================//

    /* File that just got opened. Filled asynchronously by dialog */
    struct OpenFileBuffer inputFile;
    /* List of add-ins in CWD (detected at startup) */
    std::vector<std::string> workingFolderAddins;
    /* File tracked for reloading the currently active file when changed */
    bool watch_enabled = false;
    struct WatchInfo watch_info = { .fd = -1, .wd = -1 };

    /* Path of the currently-running program, "" if none. */
    std::filesystem::path current_program_path = "";

    //=== Widgets and co. ====================================================//

    /* OpenGL logic for the display window */
    DisplayGlWindow DGW;
    /* OpenGL texture for the mono or RGB display */
    Texture displayTexture;

    /* Hexadecimal viewer widget */
    ImGui::HexViewer HV;
    /* Message console data where logs are collected */
    RichText::Text ConsoleText;
    /* Message console view showing the data above */
    RichText::View ConsoleView;

    //=== Miscellaneous ======================================================//

    /* ... add here ... */
};

#endif /* MQ_UI_GUI_H */
