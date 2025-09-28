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
#include <imgui_internal.h>
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

// GUI one-frame input information
struct GUIInput {
    bool mq_initialize_addin_fx = false;
    bool mq_initialize_addin_cg = false;
    int mq_load_working_folder_addin = -1;

    bool ui_pattern_mono = false;
    bool ui_pattern_rgb = false;

    /* Update the inotify watch on the running add-in */
    bool watch_update = false;

    bool quit = false;
};

struct GUIInput input;
struct GUI gui;

ImFont *fontSans = nullptr;
ImFont *fontMono = nullptr;
ImFont *fontBold = nullptr;

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

static CPUWindow CPUW("CPU");
static InterruptsWindow IW("Interrupts");
static MemoryTreeWindow MTW("Memory tree");
static MemoryBuffersWindow MBW("Memory buffers");
static MMUWindow MMUW("MMU");
static HeapWindow HW("Heap");
static HexViewerWindow HVW("Hex Viewer", gui.HV);
static DisplayWindow DW("Display", gui.DGW);
static KeyboardWindow KW("Keyboard");

static void resetWindowStates(void)
{
    MTW.resetState();
    MBW.resetState();
    HVW.resetState();
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

    /* Generate an observer for the current state of the machine */
    if(omach)
        mq_machine_destroyObserver(omach);
    omach = nullptr;
    if(mach)
        omach = mq_machine_createObserver(mach);

    if(mach && mach->display && mach->display->dirty) {
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

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    bool open = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal);
    input.quit |= ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q,
        ImGuiInputFlags_RouteGlobal);

    if(ImGui::BeginCustomMenuBar()) {
        if(ImGui::BeginCustomMenuChild("##menutitle", {30,0}, {1,4})) {
            ImGui::MoveCursorScreenPos({0, 3});
            ImGui::PushFont(fontBold);
            ImGui::Text("MQ");
            ImGui::PopFont();
        }
        ImGui::EndCustomMenuChild();

        if(ImGui::BeginCustomMenu("File")) {
            open |= ImGui::MenuItem("Open add-in...", "Ctrl+O");
            input.quit |= ImGui::MenuItem("Quit", "Ctrl+Q");
        }
        ImGui::EndCustomMenu();
        if(ImGui::BeginCustomMenu("Machine")) {
            input.mq_initialize_addin_fx |=
                ImGui::MenuItem("Reset to blank FX add-in");
            input.mq_initialize_addin_cg |=
                ImGui::MenuItem("Reset to blank CG add-in");
            input.ui_pattern_mono |=
                ImGui::MenuItem("Generate B&W frame");
            input.ui_pattern_rgb |=
                ImGui::MenuItem("Generate RGB frame");
        }
        ImGui::EndCustomMenu();

        if(ImGui::BeginCustomMenuChild("##menutools", {0,0}, {1,4})) {
            ImGui::CustomMenuSeparator();
            ImGui::SameLine(0, 6);

            ImGui::BeginDisabled(!mach->initialized);
            bool paused = gui.mq_cycles == 0;
            bool stuck = mach->stuck;

            if(paused && ImGui::IconButton(0, "Run"))
                gui.mq_cycles = -1;
            if(!paused && ImGui::IconButton(1, "Pause"))
                gui.mq_cycles = 0;
            ImGui::SameLine(0, 6);

            if(ImGui::IconButton(2, "Step", !paused))
                gui.mq_cycles = 1;
            ImGui::SameLine(0, 6);
            ImGui::EndDisabled();

            ImGui::MoveCursorScreenPos({0, 3});
            if(!mach->initialized)
                ImGui::TextDisabled("Not initialized");
            else if(stuck)
                ImGui::TextColored({1,.3,.3,1}, "Stuck!");
            else
                ImGui::Text(paused ? "Paused" : "Running...");
        }
        ImGui::EndCustomMenuChild();
    }
    ImGui::EndCustomMenuBar();

    /* On native builds tihs fills inputFile instantly, while on emscripten
       this fills it asynchronously and we'll get it in a future frame */
    if(open)
        openFileDialog(&gui.inputFile);

    auto dock = ImGui::DockSpaceOverViewport();

    DW.render(omach);
    KW.render(omach);

    static bool show_demo_window = false;

    if(ImGui::Begin("Control", nullptr, 0)) {
#ifndef AZUR_PLATFORM_EMSCRIPTEN
        struct mallinfo2 mi = mallinfo2();
        /* This info is not available with AddressSanitizer's wrapper's */
        if(mi.arena || mi.hblkhd)
            ImGui::Text("Memory allocated: %.1f MB heap + %.1f MB mmap\n",
                (float)mi.arena / 1e6, (float)mi.hblkhd / 1e6);

        // TODO: Better alternative on Linux:
        // 1. Open /proc/self/status
        // 2. Parse for VmRSS (Resident Set Size) and VmSwap (in swap)
        // 3. VmRSS is divided in RssAnon (± heap), RssFile (fixed), RssShmem
        // 4. For users, we're interested in VmRSS (+ VmSwap)
        // 5. For debugging, we're interested in RssAnon (≈ mi.arena)
        // Reference:
        //   top(1), "Linux Memory Types"
        // Or, for the proper programmatic interface:
        // 1. Open /proc/self/statm
        // 2. Read all numbers, multiplied by sysconf(_SC_PAGESIZE)
        // 3. [size, resident, shared, text, _, data/stack, _]
        //    * size is VmSize -> useless
        //    * shared won't be used
        //    * text is fixed
        //    * data/stack counts unmapped, non-resident pages -> useless
        // 4. Keep using mallinfo() for memory stats
        // Reference:
        //   proc_pid_statm(5)
#endif

        if(omach->initialized) {
            ImGui::Text("Cycle:");
            ImGui::SameLine();
            if(ImGui::Button("1"))
                gui.mq_cycles = 1;
            ImGui::SameLine();
            if(ImGui::Button("10"))
                gui.mq_cycles = 10;
            ImGui::SameLine();
            if(ImGui::Button("100"))
                gui.mq_cycles = 100;
            ImGui::SameLine();
            if(ImGui::Button("1000"))
                gui.mq_cycles = 1000;
            ImGui::SameLine();
            if(ImGui::Button("10k"))
                gui.mq_cycles = 10000;
        }
        else {
            ImGui::Text("Machine is not initialized.");
        }
        if(omach->stuck) {
            ImGui::Text("Machine is stuck!");
            gui.mq_cycles = 0;
        }
        if(gui.mq_cycles) {
            if(gui.mq_cycles > 0)
                ImGui::Text("Cycles pending: %d", gui.mq_cycles);
            else
                ImGui::Text("Running...");
        }

        char watch_str[256];
        if(gui.watch_info.fd < 0)
            strcpy(watch_str, "Watch input file");
        else
            snprintf(watch_str, sizeof watch_str,
                "Watching input file: %s",
                gui.watch_info.addin_path.c_str());

        if(ImGui::Checkbox2(watch_str, &gui.watch_enabled))
            input.watch_update = gui.watch_enabled;

        if(gui.workingFolderAddins.size() == 0)
            ImGui::Text("(No add-ins in working folder)");
        else
            ImGui::Text("Reset and load:");

        int spaceLeft = 0;
        for(uint i = 0; i < gui.workingFolderAddins.size(); i++) {
            char const *addin = gui.workingFolderAddins[i].c_str();
            /* Check if we have enough space (32 for button + spacing) */
            int spaceNeeded = ImGui::CalcTextSize(addin).x + 32;
            if(spaceLeft < spaceNeeded)
                spaceLeft = ImGui::GetContentRegionAvail().x;
            else
                ImGui::SameLine();

            if(ImGui::Button(addin))
                input.mq_load_working_folder_addin = i;
            spaceLeft -= spaceNeeded;
        }

        ImGui::Checkbox2("Show demo window", &show_demo_window);
    }
    ImGui::End();

    if(ImGui::Begin("Messages", nullptr)) {
        static RichText::View view = {
            .font = fontMono,
            .scroll = 0,
        };
        if(ImGui::Button("Clear"))
            gui.ConsoleText.clear();
        ImGui::AddRichTextFrame(gui.ConsoleText, view);
    }
    ImGui::End();

    CPUW.render(omach);
    IW.render(omach);
    MTW.render(omach);
    MBW.render(omach);
    HW.render(omach);
    MMUW.render(omach);
    HVW.render(omach);

    static bool first_frame = true;
    if(first_frame) {
        auto dock_left_top = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Left, 0.70, nullptr, &dock);
        auto dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Down, 0.5f, nullptr, &dock_left_top);
        auto dock_left_top_right = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Right, 0.65f, nullptr, &dock_left_top);
        auto dock_left_bottom_right = ImGui::DockBuilderSplitNode(
            dock_left_bottom,
            ImGuiDir_Right, 0.48f, nullptr, &dock_left_bottom);
        auto dock_right_bottom = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Down, 0.6f, nullptr, &dock);

        ImGui::DockBuilderDockWindow(DW.title(), dock);
        ImGui::DockBuilderDockWindow(KW.title(), dock_right_bottom);
        ImGui::DockBuilderDockWindow("Control", dock_left_top);
        ImGui::DockBuilderDockWindow("Messages", dock_left_top_right);
        ImGui::DockBuilderDockWindow(CPUW.title(), dock_left_top_right);
        ImGui::DockBuilderDockWindow(IW.title(), dock_left_top_right);
        ImGui::DockBuilderDockWindow(MTW.title(), dock_left_bottom);
        ImGui::DockBuilderDockWindow(MBW.title(), dock_left_bottom);
        ImGui::DockBuilderDockWindow(HW.title(), dock_left_bottom);
        ImGui::DockBuilderDockWindow(MMUW.title(), dock_left_bottom);
        ImGui::DockBuilderDockWindow(HVW.title(), dock_left_bottom_right);
        ImGui::DockBuilderFinish(dock);
        first_frame = false;
    }

    if(show_demo_window)
        ImGui::ShowDemoWindow();

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

    if(input.quit)
        return 1;

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

    if(input.mq_initialize_addin_fx) {
        resetWindowStates();
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_FX);
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_initialize_addin_cg) {
        resetWindowStates();
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_CG);
        render_needed = std::max(render_needed, 1);
    }
    fs::path path = "";
    if(!gui.start_path.empty()) {
        path = gui.start_path;
        gui.start_path = "";
        gui.mq_cycles = -1;
    }
    if(gui.watch_enabled) {
        enum WatchEvent event;
        while((event = watch_poll(&gui.watch_info)) != MQ_WATCH_EVT_NONE) {
            if(event == MQ_WATCH_EVT_DELETED)
                mq_log(MQ_LOG_WARNING, "watch: addin has been removed");
            if(event == MQ_WATCH_EVT_UPDATED) {
                mq_log(MQ_LOG_DEBUG, "watch: addin has been updated");
                path = gui.watch_info.addin_path;
                gui.mq_cycles = -1;
            }
        }
    }
    if(input.mq_load_working_folder_addin >= 0)
        path = gui.workingFolderAddins[input.mq_load_working_folder_addin];
    if(!path.empty()) {
        long size;
        void *data = openAndReadFile(path.c_str(), &size);
        if(data) {
            gui.inputFile.path = path;
            gui.inputFile.data = data;
            gui.inputFile.size = size;
        }
        else
            path = "";
    }

    /* Intentional re-check */
    if(gui.inputFile.data) {
        open_addin(
            gui.inputFile.path,
            gui.inputFile.data,
            gui.inputFile.size);
        free(gui.inputFile.data);
        gui.current_program_path = gui.inputFile.path;
        gui.inputFile = OpenFileBuffer();
        render_needed = std::max(render_needed, 1);
    }
    else if(gui.mq_cycles) {
        ZoneScopedN("update mq");

        /* Cycle until we reach 12 milliseconds */
        struct timespec ts_start;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        // printf("ts_start=%ld\n", ts_start.tv_nsec);

        while(gui.mq_cycles != 0 /* negative is infinity */) {
            struct timespec ts_current;
            clock_gettime(CLOCK_MONOTONIC, &ts_current);
            int64_t ns_elapsed = (ts_current.tv_nsec - ts_start.tv_nsec);
            int64_t s_elapsed = (ts_current.tv_sec - ts_start.tv_sec);
            ns_elapsed += 1'000'000'000ull * s_elapsed;
            // printf("ts_current=%ld ns_elapsed=%ld\n", ts_current.tv_nsec, ns_elapsed);
            if(ns_elapsed >= 12'000'000)
                break;

            int cycles = std::min(gui.mq_cycles, 100000);
            if(cycles < 0) {
                cycles = 100000;
                // printf("gui.mq_cycles=%d, cycles=%d\n", gui.mq_cycles, cycles);
            }
            else {
                // printf("gui.mq_cycles=%d, cycles=%d\n", gui.mq_cycles, cycles);
                gui.mq_cycles -= cycles;
            }
            mq_machine_cycle(mach, cycles);
        }
        render_needed = std::max(render_needed, 1);
    }
    if(HW.actionInitialize())
        mq_casiowin_initHeap(mach);

    bool may_enable_watch = false;
    /* Update the watch if we're loading a new program */
    if(!path.empty())
        may_enable_watch = true;
    /* Update the watch if we're clicking on the checkbox and there is a
       program running */
    if(input.watch_update && !gui.current_program_path.empty())
        may_enable_watch = true;

    if(gui.watch_enabled && may_enable_watch) {
        if(!watch_init(&gui.watch_info, gui.current_program_path))
            mq_log(MQ_LOG_ERROR, "unable to watch the file o(x_x)o");
    }

    if(input.ui_pattern_mono && mach->display) {
        generateMonoFrame(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(input.ui_pattern_rgb && mach->display) {
        generateRGBFrame(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(mach && MMUW.actionBind()) {
        mq_mmu_bind(mach);
        render_needed = std::max(render_needed, 1);
    }
    if(mach && MMUW.actionUnbind()) {
        mq_mmu_unbind(mach);
        render_needed = std::max(render_needed, 1);
    }

    if(auto a = MBW.actionViewHex()) {
        std::string bufferName = a.buffer ? a.buffer->name : "";
        HVW.viewBuffer(bufferName, a.address, a.offset, a.size);
    }

    input = GUIInput();
    return 0;
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
            if(!gui.start_path.empty()) {
                mq_log(
                    MQ_LOG_WARNING,
                    "dropping previous addin request '%s'",
                    gui.start_path.c_str());
            }
            gui.start_path = argv[i];
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

    gui.DGW.init(gui.displayTexture);

    /* Generate an example display for a texture */
    gui.displayTexture.init(GL_TEXTURE_2D);
    gui.displayTexture.bind();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glGenerateMipmap(GL_TEXTURE_2D);

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
