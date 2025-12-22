// Main header for all UI definitions that aren't reusable utilities

#ifndef MQ_UI_GUI_H
#define MQ_UI_GUI_H

#include "windows.h"
#include "util.h"
#include "watch.h"
#include "record.h"
#include <mq/controller.h>
#include <azur/opengl.h>
#include <filesystem>
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
    /* Toggle the demo window. */
    bool appToggleDemoWindow = false;

    /* Replace currently-running program with this file. */
    std::optional<fs::path> fileLoadPath;
    /* Update the inotify watch on the currently-running add-in. */
    bool fileUpdateWatch = false;
    /* Reload the current add-in. */
    bool fileReload = false;

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
    /* Toggle the cycle-profiling option. */
    bool machineToggleProfilingCycles = false;

    /* Set the hex editor to visualize a given region.
       TODO: Why does this use a direct pointer into emulated structures? */
    struct ViewHex { mqMemoryBuffer *buffer; int offset, size; u32 address; };
    std::optional<ViewHex> viewHex;
    void setViewHex(mqMemoryBuffer *buffer, int offset, int size, u32 address) {
        viewHex = ViewHex { buffer, offset, size, address };
    }

    /* Keys whose status should be reassigned this frame, identified by key
       numbers from the keyboard structure */
    std::map<uint, bool> physicalKeysAssigned;
    /* Keys whose status should be reassigned, identified by logical keycode */
    std::map<mqKeyboardKeycode, bool> logicalKeysAssigned;

    /* Take a screenshot */
    bool recordScreenshot = false;
    /* Start, pause, unpause, and stop video */
    bool recordStart = false;
    bool recordPause = false;
    bool recordUnpause = false;
    bool recordStop = false;
};

/* Set of windows. Each window type can instanced on multiple machines. */
struct GUIWindowSet
{
    std::unique_ptr<ControlWindow> Control;
    std::unique_ptr<MessagesWindow> Messages;
    std::unique_ptr<CPUWindow> CPU;
    std::unique_ptr<InterruptsWindow> Interrupts;
    std::unique_ptr<MemoryTreeWindow> MemoryTree;
    std::unique_ptr<MemoryBuffersWindow> MemoryBuffers;
    std::unique_ptr<MMUWindow> MMU;
    std::unique_ptr<HeapWindow> Heap;
    std::unique_ptr<HexViewerWindow> HexViewer;
    std::unique_ptr<DisplayWindow> Display;
    std::unique_ptr<KeyboardWindow> Keyboard;
    std::unique_ptr<RecordWindow> Record;

    void resetState() {
        MemoryTree->resetState();
        MemoryBuffers->resetState();
        HexViewer->resetState();
        Record->resetState();
    }
};

/* All dynamic UI data. The state doesn't consist only of the data directly in
   this structure, windows have internal state too. */
struct GUI
{
    GUI();
    struct GUIActions actions;

    //=== Controlling files ==================================================//

    /* File that just got opened. Filled asynchronously by dialog */
    struct OpenFileBuffer inputFile;
    /* List of add-ins in CWD (detected at startup) */
    std::vector<std::string> workingFolderAddins;
    std::string workingFolderPrefix;
    struct WatchInfo workingFolderWatcher = \
        { .fd = -1, .wd = -1, .is_file = false };
    /* File tracked for reloading the currently active file when changed */
    bool watch_enabled = false;
    struct WatchInfo watch_info = { .fd = -1, .wd = -1, .is_file = false };

    /* Path of the currently-running program, "" if none. */
    std::filesystem::path current_program_path = "";

    //=== Recording ==========================================================//

    /* Copy of the last frame obtained from the machine. `dirty` indicates
       whether the frame is new for the recorder. */
    mqDisplay lastDisplayFrame;
    /* Whether listDisplayFrame is new for the render function. */
    bool lastDisplayFrameNew = false;

    /* Function to generate the substitutions in output file names: %ADDIN%,
       %DATE%, etc. */
    static OutputPathPattern::SubstitutionMap makeSubstitutions();

    /* Screenshot output path, its scaling factor, and error code from the last
       attempt generate a screenshot */
    OutputPathPattern imageOutputPath;
    int imageScale = 1;
    int imageError = 0;

#if MQ_VIDEO_FFMPEG
    /* Recording output path and recorder state */
    OutputPathPattern videoOutputPath;
    /* Record only while the add-in is running */
    bool recordOnlyWhenRunning = true;
    mqRecord recorder;
#endif

    //=== Widgets and co. ====================================================//

    /* OpenGL logic for the display window */
    DisplayGlWindow DGW;
    /* OpenGL texture for the mono or RGB display */
    azur::gl::Texture2D *displayTexture;

    /* Message console data where logs are collected */
    RichText::Text ConsoleText;
    /* Message console view showing the data above */
    RichText::View ConsoleView;

    /* Machine-related windows.
       TODO: map<int, GUIWindowSet> + move some of the widgets in */
    // std::map<int, GUIWindowSet> WindowSets;
    GUIWindowSet Windows;

    void Render(mqController *controller);
    void DockWindowsStyle1(GUIWindowSet const &Windows, ImGuiID dock);
    void ResetState();

    //=== Miscellaneous ======================================================//

    /* ... add here ... */
};

#endif /* MQ_UI_GUI_H */
