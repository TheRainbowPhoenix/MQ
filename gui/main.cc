#include "gui.h"
#include "windows.h"
#include "watch.h"

#include <mq/mq.h>
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

/* Refresh request triggered by SDL events and Dear ImGui's initial frames. */
static int render_needed = IMGUI_SETTLING_FRAMES;

/* Main machine on which we're running the program. This is used in the
   emulation thread and can't be accessed randomly by GUI. */
static mqMachine *mach = nullptr;
/* Observer machine containing snapshots of the main machine's state, used by
   the GUI and (mostly) independent from the emulation thread */
static mqMachine *omach = nullptr;

struct GUI gui;

ImFont *fontSans = nullptr;
ImFont *fontMono = nullptr;
ImFont *fontBold = nullptr;

void update_machine(mqMachine *mach, bool startRunning);

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
    gui.ConsoleText.addLine(line);
}

static void resetWindowStates(void)
{
    gui.Windows.resetState();
    gui.HV.Cursor = 0;
    gui.HV.MinAddress = 0;
    gui.HV.MaxAddress = (u32)-1;
}

static void render(void)
{
    ZoneScopedN("render");

    if(!render_needed)
        return;
    render_needed--;

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

    if(omach)
        mq_machine_destroyObserver(omach);
    omach = nullptr;

    /*** Wait to acquire access to the machine so we can generate an observer
         and update the display texture.
         TODO: Put a separate lock on the display ***/
    if(mach) {
        mq_machine_lock(mach);
        omach = mq_machine_createObserver(mach);

        if(mach->display && mach->display->dirty) {
            mqDisplay *d = mach->display;
            gui.displayTexture.bind();
            if(d->format == MQ_DISPLAY_FORMAT_L8) {
#if AZUR_GRAPHICS_OPENGL_ES_2_0 || AZUR_GRAPHICS_OPENGL_ES_3_0
                gui.displayTexture.setFormat(GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                         d->width, d->height);
#elif AZUR_GRAPHICS_OPENGL_3_3
                gui.displayTexture.setFormat(GL_RED, GL_UNSIGNED_BYTE,
                                         d->width, d->height);
#endif
                gui.displayTexture.setData(d->data);
            }
            else if(d->format == MQ_DISPLAY_FORMAT_RGB565) {
                gui.displayTexture.setFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                                         d->width, d->height);
                gui.displayTexture.setData(d->data);
            }
            else
                printf("warning: display not updated, unknown format!\n");

            gui.DGW.setInherentScale(d->width <= 128 ? 3 : 1);
            mq_display_setDirty(d, false);
        }

        mq_machine_unlock(mach);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    gui.Render(omach);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    /* Present results to user */
    SDL_GL_SwapWindow(window);
    previous_time = time;

    FrameMark;
}

//---

static void open_addin(std::string const &path, void *data, long size)
{
    if(path.ends_with(".g1a") || path.ends_with(".G1A")) {
        resetWindowStates();
        watch_quit(&gui.watch_info);
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_FX);
        mq_machine_load_g1a(mach, data, size);
    }
    else if(path.ends_with(".g3a") || path.ends_with(".G3A")) {
        resetWindowStates();
        watch_quit(&gui.watch_info);
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_CG);
        mq_machine_load_g3a(mach, data, size);
    }
    else {
        azlog(ERROR, "unrecognized add-in type for %s", path.c_str());
    }
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

    if(gui.actions.appClearConsole)
        gui.ConsoleText.clear();

    fs::path loadPath = "";

    if(gui.watch_enabled) {
        enum WatchEvent event;
        while((event = watch_poll(&gui.watch_info)) != MQ_WATCH_EVT_NONE) {
            if(event == MQ_WATCH_EVT_DELETED)
                mq_log(MQ_LOG_WARNING, "watch: addin has been removed");
            if(event == MQ_WATCH_EVT_UPDATED) {
                mq_log(MQ_LOG_DEBUG, "watch: addin has been updated");
                loadPath = gui.current_program_path;
                startRunning = true;
            }
        }
    }
    if(auto p = gui.actions.fileLoadPath)
        loadPath = *p;
    if(!loadPath.empty()) {
        long size;
        void *data = openAndReadFile(loadPath.c_str(), &size);
        if(data) {
            gui.inputFile.path = loadPath;
            gui.inputFile.data = data;
            gui.inputFile.size = size;
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
    mq_machine_lock(mach);
    update_machine(mach, startRunning);
    mq_machine_unlock(mach);

    gui.actions = GUIActions();
    return 0;
}

void update_machine(mqMachine *mach, bool startRunning)
{
    bool may_enable_watch = false;

    if(auto i = gui.actions.machineInitialize) {
        resetWindowStates();
        mq_machine_initialize(mach, *i);
        render_needed = std::max(render_needed, 1);
    }

    if(auto c = gui.actions.machineSetPendingCycles)
        mach->cyclesPending = *c;

    /* Intentional re-check */
    if(gui.inputFile.data) {
        open_addin(
            gui.inputFile.path,
            gui.inputFile.data,
            gui.inputFile.size);
        free(gui.inputFile.data);
        /* Update the watch if we're loading a new program */
        may_enable_watch = true;
        if(startRunning)
            mach->cyclesPending = -1;
        gui.current_program_path = gui.inputFile.path;
        gui.inputFile = OpenFileBuffer();
        render_needed = std::max(render_needed, 1);
    }
    else if(mach->cyclesPending) {
        TracyCZoneN(ctx, "cycle machine", true)

        /* Cycle until we reach 12 milliseconds */
        struct timespec ts_start;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        // printf("ts_start=%ld\n", ts_start.tv_nsec);

        int totalCycles = 0;

        while(mach->cyclesPending != 0 /* negative is infinity */) {
            struct timespec ts_current;
            clock_gettime(CLOCK_MONOTONIC, &ts_current);
            int64_t ns_elapsed = (ts_current.tv_nsec - ts_start.tv_nsec);
            int64_t s_elapsed = (ts_current.tv_sec - ts_start.tv_sec);
            ns_elapsed += 1'000'000'000ull * s_elapsed;
            // printf("ts_current=%ld ns_elapsed=%ld\n", ts_current.tv_nsec, ns_elapsed);
            if(ns_elapsed >= 12'000'000)
                break;

            int cycles = std::min(mach->cyclesPending, 20000);
            if(cycles < 0)
                cycles = 20000;

            totalCycles += cycles;

            mq_machine_cycle(mach, cycles);
        }

        TracyCZoneEnd(ctx)

        // Can vary widely based on what the host nest is!
        // printf("cycles this round! %d\n", totalCycles);
        render_needed = std::max(render_needed, 1);
    }

    if(gui.actions.machineSystemHeapInitialize)
        mq_casiowin_initHeap(mach);

    /* Update the watch if we're clicking on the checkbox and there is a
       program running */
    if(gui.actions.fileUpdateWatch && !gui.current_program_path.empty())
        may_enable_watch = true;

    if(gui.watch_enabled && may_enable_watch) {
        if(!watch_init(&gui.watch_info, gui.current_program_path))
            mq_log(MQ_LOG_ERROR, "unable to watch the file o(x_x)o");
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

static void find_cwd_addins(std::vector<std::string> &addins)
{
    for(auto const &entry: fs::directory_iterator(".")) {
        if(!entry.is_regular_file())
            continue;

        std::string ext = entry.path().extension();
        if(ext == ".g1a" || ext == ".G1A" || ext == ".g3a" || ext == ".G3A")
            addins.push_back(entry.path().filename());
    }

    std::sort(addins.begin(), addins.end());
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
            gui.watch_enabled = true;
        else if(!strcmp("--version", argv[i])) {
            printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,
                AZUR_VERSION_MINOR);
            return false;
        }
        else {
            if(gui.actions.fileLoadPath) {
                mq_log(
                    MQ_LOG_WARNING,
                    "dropping previous addin request '%s'",
                    gui.actions.fileLoadPath->c_str());
            }
            gui.actions.fileLoadPath = argv[i];
            gui.actions.machineSetPendingCycles = -1;
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "C.UTF-8");
    printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);

    gui.HV.AddressBits = 32;
    gui.HV.MinAddress = 0;
    gui.HV.MaxAddress = 0xffffffff;
    gui.HV.InputType = ImGui::HexViewer::InputFunction;
    gui.HV.LineSpacing = 2;
    gui.HV.AlignXCenter = false;
    gui.HV.Cursor = 0;

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
    gui.Windows.HexViewer =
        std::make_unique<HexViewerWindow>("Hex Viewer", gui.HV);
    gui.Windows.Display = std::make_unique<DisplayWindow>("Display", gui.DGW);
    gui.Windows.Keyboard = std::make_unique<KeyboardWindow>("Keyboard");

    mq_log_handler(handle_log);

    if(!parse_cli_args(argc, argv))
        return 0;

    mq_init();

    mach = mq_machine_create();

    if(azur_init("MQ", 1500, 850) != 0)
        return 1;
    if(!azur_init_imgui())
        return 1;

    srand(clock());

    gui.displayTexture.init(GL_TEXTURE_2D);
    gui.DGW.init(gui.displayTexture);

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

    /* Provide options for loading add-ins in the current folder */
    find_cwd_addins(gui.workingFolderAddins);

    int rc = azur_main_loop(render, 60, update, -1, AZUR_MAIN_LOOP_TIED);

    gui.DGW.cleanup();

    watch_quit(&gui.watch_info);

    azur_quit();
    if(mach) {
        mq_machine_destroy(mach);
        mach = nullptr;
    }
    mq_quit();
    return rc;
}
