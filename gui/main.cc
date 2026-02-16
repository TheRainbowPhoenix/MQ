#include "record.h"
#include "gui.h"
#include "windows.h"
#include "watch.h"

#include <mq/mq.h>
#include <mq/controller.h>
#include <mq/machine.h>
#include <mq/system/casiowin.h>
#include <mq/system/heap.h>
#include <mq/interfaces/display.h>
#include <mq/interfaces/keyboard.h>
#include <mq/modules/mmu.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <azur/azur.h>
#include <azur/resources.h>
#include <azur/log.h>
#include <SDL2/SDL.h>
#include <stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <locale.h>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct GUI gui;
/* Refresh request triggered by SDL events and Dear ImGui's initial frames. */
static int render_needed = IMGUI_SETTLING_FRAMES;
/* Globally-loaded fonts */
ImFont *fontSans = nullptr;
ImFont *fontMono = nullptr;
ImFont *fontBold = nullptr;

mqController *emu0 = NULL;

//============================================================================//

static void find_cwd_programs(
    std::string const &search_prefix,
    std::vector<std::string> &programs
) {
    programs.clear();
    for(auto const &entry: fs::directory_iterator(search_prefix)) {
        if(!entry.is_regular_file())
            continue;
        std::string ext = entry.path().extension();
        if(ext == ".g1a" || ext == ".G1A" || ext == ".g3a" || ext == ".G3A")
            programs.push_back(entry.path().filename());
    }
    std::sort(programs.begin(), programs.end());
}

//============================================================================//

void update_machine(
    mqController *controller, mqMachine *mach, bool startRunning);

static void handle_log(enum mq_log_priority priority, char *str)
{
    /* Print to terminal */
    mq_log_default_handler(priority, str);
    /* Also print to internal console */
    RichText::Line *line;
    if(priority == MQ_LOG_DEBUG)
        line = RichText::Line::make((std::string("debug: ") + str).c_str());
    else if(priority == MQ_LOG_WARNING)
        line = RichText::Line::make((std::string("warning: ") + str).c_str());
    else if(priority == MQ_LOG_ERROR)
        line = RichText::Line::make((std::string("error: ") + str).c_str());
    else
        line = RichText::Line::make(str);

    gui.ConsoleText.lock();
    gui.ConsoleText.addLine(line);
    gui.ConsoleText.unlock();
}

static void render(void)
{
    ZoneScopedN("render");

    SDL_Window *window = azur_sdl_window();
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    /* TODO: Get global time */
    double time = 0.0;

    static double previous_time = 0.0;
    double dt = time - previous_time;
    (void)dt;

    /* Don't accumulate work when the window is not focused! */
    Uint32 flags = SDL_GetWindowFlags(window);
    if(previous_time != 0.0 && !(flags & SDL_WINDOW_INPUT_FOCUS)) return;

    if(emu0->omach)
        mq_machine_destroyObserver(emu0->omach);
    emu0->omach = nullptr;

    /*** Wait to acquire access to the machine so we can generate an observer
         and update the display texture.
         TODO: Put a separate lock on the display ***/
    mqMachine *mach = emu0->mach;
    if(mach) {
        ZoneScopedN("render lock");
        // printf("[Main] Locking machine for render\n");
        mq_machine_lock(mach);
        // printf("[Main] Locked machine for render\n");
        emu0->omach = mq_machine_createObserver(mach);
        // printf("[Main] Unlocking machine after render\n");
        mq_machine_unlock(mach);
        // printf("[Main] Unlocked machine after render\n");

        /* Now that we have a local copy of the framebuffer, upload it to the
           GPU (while the machine can keep working) */
        if(gui.lastDisplayFrameNew) {
            mqDisplay const *d = &gui.lastDisplayFrame;
            gui.displayTexture->bind();
            if(d->format == MQ_DISPLAY_FORMAT_L8) {
                gui.displayTexture->setFormat(GL_R8, d->width, d->height);
                gui.displayTexture->loadData(
                    d->data, GL_RED, GL_UNSIGNED_BYTE, d->width, 0);
            }
            else if(d->format == MQ_DISPLAY_FORMAT_RGB565) {
                gui.displayTexture->setFormat(GL_RGB565, d->width, d->height);
                gui.displayTexture->loadData(
                    d->data, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, d->width, 0);
            }
            else
                printf("warning: display not updated: unknown format!\n");

            gui.DGW.setInherentScale(d->width <= 128 ? 3 : 1);
            gui.lastDisplayFrameNew = false;
            render_needed = std::max(render_needed, 1);
        }
    }

    static int frames_skipped = 0;
    if(!render_needed && frames_skipped < 30) {
        frames_skipped++;
        FrameMark;
        return;
    }
    render_needed -= (render_needed > 0);
    frames_skipped = 0;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    gui.Render(emu0);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    /* Present results to user */
    SDL_GL_SwapWindow(window);
    previous_time = time;

    FrameMark;
}

//---

static void open_program(std::string const &path, void *data, long size)
{
    if(path.ends_with(".g1a") || path.ends_with(".G1A")) {
        gui.ResetState();
        watch_quit(&gui.programFolderWatcherInfo);
        mq_machine_setupHardware(emu0->mach, MQ_MACHINE_HARDWARE_VIRT_ADDIN_FX);
        mq_machine_initialize(emu0->mach, MQ_MACHINE_INITIALIZE_ADDIN);
        mq_machine_load_g1a(emu0->mach, data, size);
        //todo: move me ?
        // gui.perfThrottleProfile = gui.GUI_THROTTLE_PROFILE_25FPS;
    }
    else if(path.ends_with(".g3a") || path.ends_with(".G3A")) {
        gui.ResetState();
        watch_quit(&gui.programFolderWatcherInfo);
        mq_machine_setupHardware(emu0->mach, MQ_MACHINE_HARDWARE_VIRT_ADDIN_CG);
        mq_machine_initialize(emu0->mach, MQ_MACHINE_INITIALIZE_ADDIN);
        mq_machine_load_g3a(emu0->mach, data, size);
        // gui.perfThrottleProfile = gui.GUI_THROTTLE_PROFILE_60FPS;
    }
    else {
        azlog(ERROR, "unrecognized add-in type for %s", path.c_str());
    }

    //todo: move me?
    mq_machine_setFilesystemRoot(emu0->mach, gui.programFolderPrefix.c_str());
}

static int update(void)
{
    ZoneScopedN("update");

    SDL_Event e;

    if(gui.actions.appQuit)
        return 1;

    bool startRunning = false;

    while(SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        render_needed = std::max(render_needed, 3);

        if(e.type == SDL_QUIT)
            return 1;
        if(e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            gui.DGW.markViewDirty(true);
        }
    }

    if(gui.actions.appClearConsole) {
        gui.ConsoleText.lock();
        gui.ConsoleText.clear();
        gui.ConsoleText.unlock();
    }

    // program folder refresh
    enum WatchEvent event;
    bool need_refresh = false;
    if(gui.actions.programFolderPrefixUpdate) {
        if(!gui.programFolderPrefix.empty())
            watch_quit(&gui.programFolderWatcherInfo);
        gui.programFolderPrefix = *gui.actions.programFolderPrefixUpdate;
        if(!watch_init(&gui.programFolderWatcherInfo, gui.programFolderPrefix))
            mq_log(MQ_LOG_ERROR, "unable to watch the program folder");
        need_refresh = true;
    } else {
        while(true) {
            event = watch_poll(&gui.programFolderWatcherInfo);
            if(event == MQ_WATCH_EVT_NONE)
                break;
            if(event != MQ_WATCH_EVT_DIR_UPDATED)
                continue;
            need_refresh = true;
            mq_log(MQ_LOG_DEBUG, "refresh program folder");
        }
    }
    if(need_refresh)
        find_cwd_programs(gui.programFolderPrefix, gui.programFolderListName);

    fs::path loadPath = "";
    if(gui.programFileWatchEnabled) {
        while(true) {
            event = watch_poll(&gui.programFileWatchInfo);
            if(event == MQ_WATCH_EVT_NONE)
                break;
            if(event == MQ_WATCH_EVT_DELETED)
                mq_log(MQ_LOG_WARNING, "watch: program has been removed");
            if(event == MQ_WATCH_EVT_UPDATED) {
                mq_log(MQ_LOG_DEBUG, "watch: program has been updated");
                loadPath = gui.programFilePath;
                startRunning = true;
            }
        }
    }
    if(gui.actions.programFileLoadByName)
        loadPath = fs::path(gui.programFolderPrefix) /
                   *gui.actions.programFileLoadByName;
    else if(gui.actions.programFileLoadByPath)
        loadPath = *gui.actions.programFileLoadByPath;
    else if(gui.actions.programFileReload)
        loadPath = gui.programFilePath;
    if(!loadPath.empty()) {
        long size;
        void *data = openAndReadFile(loadPath.c_str(), &size);
        if(data) {
            gui.programFileInfo.path = loadPath;
            gui.programFileInfo.data = data;
            gui.programFileInfo.size = size;
            // Auto-start the first program that gets loaded
            if(gui.programFilePath.empty())
                gui.actions.machineSetPendingCycles = -1;
        }
        else
            loadPath = "";
    }

    if(auto a = gui.actions.viewHex) {
        std::string bufferName = a->buffer ? a->buffer->name : "";
        gui.Windows.HexViewer->viewBuffer(
            bufferName, a->address, a->offset, a->size);
    }

    /* Now acquire a lock to the machine so we can run complex actions. */
    // printf("[Main] Locking machine for update\n");
    mq_machine_lock(emu0->mach);
    // printf("[Main] Locked machine for update\n");
    update_machine(emu0, emu0->mach, startRunning);
    // printf("[Main] Unlocking machine after update\n");
    mq_machine_unlock(emu0->mach);
    // printf("[Main] Unlocked machine after update\n");

    if(gui.actions.recordScreenshot) {
        gui.imageOutputPath.resolve(true);
        printf("-> '%s'\n", gui.imageOutputPath.resolvedPath().c_str());
        gui.imageError = screenshotPNG(&gui.lastDisplayFrame, gui.imageScale,
            gui.imageOutputPath.resolvedPath());
        /* Resolve again to update the warning */
        gui.imageOutputPath.resolve(true);
    }

#if MQ_VIDEO_FFMPEG
    /* render status update */
    if(gui.actions.recordStart) {
        gui.videoOutputPath.resolve(true);
        gui.recorder.start(
            &gui.lastDisplayFrame,
            gui.videoOutputPath.resolvedPath(),
            60, false);
    }
    if(gui.actions.recordPause) {
        if(!gui.recorder.pause(&gui.lastDisplayFrame))
            mq_log(MQ_LOG_ERROR, "%s", gui.recorder.lasterror().c_str());
    }
    if(gui.actions.recordUnpause) {
        if(!gui.recorder.unpause(&gui.lastDisplayFrame))
            mq_log(MQ_LOG_ERROR, "%s", gui.recorder.lasterror().c_str());
    }
    if(gui.actions.recordStop) {
        if(!gui.recorder.stop(&gui.lastDisplayFrame))
            mq_log(MQ_LOG_ERROR, "%s", gui.recorder.lasterror().c_str());
    }

    /* add frame to the video if needed */
    if(gui.recorder.status() == MQ_RECORD_STATUS_RECORDING) {
        bool want_frame =
            (emu0->mach->cyclesPending != 0) || !gui.recordOnlyWhenRunning;
        if(want_frame && !gui.recorder.frame_add(&gui.lastDisplayFrame))
            mq_log(MQ_LOG_ERROR, "%s", gui.recorder.lasterror().c_str());
    }
#endif

    gui.actions = GUIActions();
    return 0;
}

/* The machine is locked during this function. */
void update_machine(
    mqController *controller, mqMachine *mach, bool startRunning)
{
    bool may_enable_watch = false;

    if(auto i = gui.actions.machineInitialize) {
        gui.ResetState();
        mq_machine_setupHardware(emu0->mach, *i);
        mq_machine_initialize(emu0->mach, MQ_MACHINE_INITIALIZE_ADDIN);
        render_needed = std::max(render_needed, 1);
    }

    if(auto c = gui.actions.machineSetPendingCycles)
        mq_machine_setCyclesPending(mach, *c);
    if(auto c = gui.actions.machineSetPendingFrames)
        mq_machine_setFramesPending(mach, *c);

    /* Intentional re-check for folder prefix switch request */
    if(gui.actions.programFolderPrefixUpdate && mach->fs)
        mq_machine_setFilesystemRoot(mach, gui.programFolderPrefix.c_str());

    /* Intentional re-check */
    if(gui.programFileInfo.data) {
        open_program(
            gui.programFileInfo.path,
            gui.programFileInfo.data,
            gui.programFileInfo.size);
        free(gui.programFileInfo.data);
        /* Update the watch if we're loading a new program */
        may_enable_watch = true;
        if(startRunning)
            mq_machine_setCyclesPending(mach, -1);
        gui.programFilePath = gui.programFileInfo.path;
        gui.programFileInfo = OpenFileBuffer();
        render_needed = std::max(render_needed, 1);
    }

    if(gui.actions.machineSystemHeapInitialize)
        mq_casiowin_initHeap(mach);

    /* Update the watch if we're clicking on the checkbox and there is a
       program running */
    if(gui.actions.programFileWatchToggle && !gui.programFilePath.empty())
        may_enable_watch = true;

    if(gui.programFileWatchEnabled && may_enable_watch) {
        if(!watch_init(&gui.programFileWatchInfo, gui.programFilePath))
            mq_log(MQ_LOG_ERROR, "unable to watch the file o(x_x)o");
    }

    /* Make a copy of the current frame (after resetting the machine in case of
       a reset, so we don't grab a frame with the wrong format) */
    if(mach->display && mach->display->pixelsChanged) {
        mqDisplay *d = mach->display;
        gui.lastDisplayFrame.format = d->format;
        gui.lastDisplayFrame.width  = d->width;
        gui.lastDisplayFrame.height = d->height;
        gui.lastDisplayFrame.data =
            memdup(d->data, mq_display_framebufferSize(d));
        /* NOTE: New "frames" are counted for the recorder whenever any pixels
           change. This is not the same notion of "frame" as the one exposed by
           the display and controller. */
        /* The frame is new for the recorder */
        gui.lastDisplayFrame.pixelsChanged = true;
        /* The frame is new for the GUI */
        gui.lastDisplayFrameNew = true;

        mq_display_setPixelsChanged(d, false);
    }

    /* fetch throttle statistics */
    if(controller->lastFrame.dirty) {
        if(controller->lastFrame.timeDelta > 0) {
            int idx = gui.perfThrottleStatsIdx;
            int pause = controller->lastFrame.timePause;
            int raw = (1000 * 1000000) / (controller->lastFrame.timeDelta);
            int fps =
                (1000 * 1000000) /
                (controller->lastFrame.timeRef -
                 controller->lastFrame.previousTimeRef);
            if(pause < 0) {
                fps = raw;
                pause = 0;
            }
            gui.perfThrottleStatsPause[idx] = pause / 1000000;
            gui.perfThrottleStatsRaw[idx] = raw;
            gui.perfThrottleStatsFps[idx] = fps;
            gui.perfThrottleStatsIdx = (idx + 1) % (5 * 60);
            gui.perfThrottleLastPause = pause / 1000000;
            gui.perfThrottleLastFps = fps;
            gui.perfThrottleLastRaw = raw;
        }
        if(mach->framesPending > 0) {
            if(--mach->framesPending <= 0)
                mach->cyclesPending = 0;
        }
        controller->lastFrame.dirty = false;
    }

    /* force-update the throttle request */
    controller->requestFps = gui.perfThrottleRequestFps;
    if(controller->requestFps == 0) {
        mq_log(MQ_LOG_ERROR, "throttle.requestFps == 0!!");
        controller->requestFps = 60;
    }

    if(gui.actions.machineGenerateMonoFrame && mach->display) {
        generateMonoFrame(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(gui.actions.machineGenerateRGBFrame && mach->display) {
        generateRGBFrame(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(gui.actions.machineMMUBind && mach) {
        mq_mmu_bind(mach);
        render_needed = std::max(render_needed, 1);
    }
    if(gui.actions.machineMMUUnbind && mach) {
        mq_mmu_unbind(mach);
        render_needed = std::max(render_needed, 1);
    }

    if(gui.actions.machineToggleProfilingCycles && mach) {
        mach->profilingCycles = !mach->profilingCycles;
        render_needed = std::max(render_needed, 1);
    }

    if(mach->keyboard) {
        for(auto [key, pressed]: gui.actions.physicalKeysAssigned)
            mq_keyboard_setKeyPressed(mach->keyboard, key, pressed);
        for(auto [keycode, pressed]: gui.actions.logicalKeysAssigned)
            mq_keyboard_setKeycodePressed(mach->keyboard, keycode, pressed);
    }
}

int *icon_rect_ids = NULL;

static void load_icons(char const *rid)
{
    ImGuiIO &io = ImGui::GetIO();
    ImFont *font = fontSans;
    u32 PUA = 0xe000; // UTF-8 Private Use Area (from BMP)

    /* Load the image and find out how many icons there are */
    int icon_w, icon_h, icon_n;

    size_t size;
    void const *data = azur::getResource(rid, &size);
    if(!data)
        return;

    u8 *icon_px = stbi_load_from_memory(
        (stbi_uc const *)data, size, &icon_w, &icon_h, &icon_n, 4);
    if(!icon_px) {
        azlog(ERROR, "stbi_load(\"%s\") failed\n", rid);
        return;
    }
    int icon_count = icon_w / 16;

    /* Allocate glyphs and build the font atlas */
    icon_rect_ids = (int *)malloc(icon_count * sizeof *icon_rect_ids);

    for(int i = 0; i < icon_count; i++) {
        icon_rect_ids[i] = io.Fonts->AddCustomRectFontGlyph(
            font, PUA+i, 16, 16, 16, ImVec2(0,-1));
    }

    io.Fonts->TexPixelsUseColors = true;
    io.Fonts->Build();

    /* Retrieve atlas texture in RGBA format */
    u8 *atlas_px = nullptr;
    int atlas_w, atlas_h;
    io.Fonts->GetTexDataAsRGBA32(&atlas_px, &atlas_w, &atlas_h);

    /* Copy the icons into the atlas */
    for(int i = 0; i < icon_count; i++) {
        int id = icon_rect_ids[i];
        ImFontAtlasCustomRect const *rect = io.Fonts->GetCustomRectByIndex(id);
        if(!rect)
            continue;

        for(int y = 0; y < rect->Height; y++) {
            u32 *dst = (u32 *)atlas_px + (rect->Y + y) * atlas_w + (rect->X);
            for(int x = 0; x < rect->Width; x++) {
                u8 *src = icon_px + (y * icon_w + (16 * i + x)) * 4;
                *dst++ = IM_COL32(src[0], src[1], src[2], src[3]);
            }
        }
    }

    stbi_image_free(icon_px);
}

static ImFont *ImGui_AddFontFromResource(char const *rid, float pointSize)
{
    ImGuiIO &io = ImGui::GetIO();
    size_t size;
    void const *ptr = azur::getResource(rid, &size);
    if(!ptr) {
        azlog(ERROR, "No such resource '%s'\n", rid);
        return nullptr;
    }

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    return io.Fonts->AddFontFromMemoryTTF(
        (void *)ptr, size, pointSize, &fontConfig);
}

bool parse_cli_args(int argc, char **argv)
{
    for(int i = 1; i < argc; i++) {
        if(!strcmp("--watch", argv[i]))
            gui.programFileWatchEnabled = true;
        else if(!strcmp("--version", argv[i])) {
            printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,
                AZUR_VERSION_MINOR);
            return false;
        }
        else if(!strcmp("--dir", argv[i])) {
            if(gui.actions.programFolderPrefixUpdate) {
                mq_log(
                    MQ_LOG_WARNING,
                    "dropping previous program directory request '%s'",
                    gui.actions.programFolderPrefixUpdate.value().c_str());
            }
            if(++i >= argc) {
                mq_log(MQ_LOG_ERROR, "missing directory information");
                return false;
            }
            gui.actions.programFolderPrefixUpdate = argv[i];
        }
        else if(!strcmp("--program", argv[i])) {
            if(gui.actions.programFileLoadByName) {
                mq_log(MQ_LOG_WARNING,
                    "dropping previous program directory request '%s'",
                    gui.actions.programFileLoadByName.value().c_str());
            }
            if(++i >= argc) {
                mq_log(MQ_LOG_ERROR, "missing directory information");
                return false;
            }
            gui.actions.programFileLoadByName = argv[i];
        }
        else {
            if(gui.actions.programFileLoadByPath) {
                mq_log(MQ_LOG_WARNING,
                    "dropping previous program request '%s'",
                    gui.actions.programFileLoadByPath.value().c_str());
            }
            gui.actions.programFileLoadByPath = argv[i];
            gui.actions.machineSetPendingCycles = -1;
        }
    }
    if(!gui.actions.programFolderPrefixUpdate)
        gui.actions.programFolderPrefixUpdate = ".";
    if(gui.actions.programFileLoadByPath
            && gui.actions.programFileLoadByName) {
        mq_log(MQ_LOG_WARNING,
            "You have specified an program name and an program path! "
            "Only the path will be kept");
        gui.actions.programFileLoadByName.reset();
    }
    return true;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "C.UTF-8");
    printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);

    // azur::log::setMinimumLevelFilter(azur::log::level::DEBUG);

    gui.ConsoleText.alloc(65536, 256);

    gui.ConsoleView.font = fontMono;
    gui.ConsoleView.scroll = 0;

    gui.Windows.Control = std::make_unique<ControlWindow>("Control");
    gui.Windows.Messages = std::make_unique<MessagesWindow>("Messages");
    gui.Windows.CPU = std::make_unique<CPUWindow>("CPU");
    gui.Windows.Interrupts = std::make_unique<InterruptsWindow>("Interrupts");
    gui.Windows.MemoryTree = std::make_unique<MemoryTreeWindow>("Memory tree");
    gui.Windows.MemoryBuffers =
        std::make_unique<MemoryBuffersWindow>("Memory buffers");
    gui.Windows.MMU = std::make_unique<MMUWindow>("MMU");
    gui.Windows.Heap = std::make_unique<HeapWindow>("Heap");
    gui.Windows.HexViewer = std::make_unique<HexViewerWindow>("Hex Viewer");
    gui.Windows.Display = std::make_unique<DisplayWindow>("Display", gui.DGW);
    gui.Windows.Keyboard = std::make_unique<KeyboardWindow>("Keyboard");
    gui.Windows.Record = std::make_unique<RecordWindow>("Record");

    mq_log_handler(handle_log);

    if(!parse_cli_args(argc, argv))
        return 0;

    mq_init();

    mqMachine *mach = mq_machine_create();
    emu0 = mq_controller_create();
    if(!emu0)
        return 1;

    mq_controller_startThread(emu0, mach);

    if(azur_init("MQ", 1500, 850) != 0)
        return 1;
    if(!azur_init_imgui())
        return 1;

    srand(clock());

    gui.DGW.init();
    gui.displayTexture = &gui.DGW.texture();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = NULL;
    fontSans = ImGui_AddFontFromResource(
        "@mqgui:assets/DejaVuSans.subset.ttf", 13.0f);
    fontMono = ImGui_AddFontFromResource(
        "@mqgui:assets/DejaVuSansMono.subset.ttf", 13.0f);
    fontBold = ImGui_AddFontFromResource(
        "@mqgui:assets/DejaVuSans-Bold.subset.ttf", 13.0f);
    io.Fonts->AddFontDefault();

    load_icons("@mqgui:assets/icons.png");

    ImGui_LoadMQStyle(ImGui::GetStyle());

    int rc = azur_main_loop(render, 60, update, -1, AZUR_MAIN_LOOP_TIED);
    mq_controller_stopThread(emu0);

    gui.DGW.cleanup();

#if MQ_VIDEO_FFMPEG
    gui.recorder.stop(&gui.lastDisplayFrame);
#endif
    watch_quit(&gui.programFileWatchInfo);

    azur_quit();
    mq_controller_destroy(emu0);

    mq_quit();
    return rc;
}
