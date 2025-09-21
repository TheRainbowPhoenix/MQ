#include "gui.h"
#include <mq/mq.h>
#include <mq/machine.h>
#include <mq/system/casiowin.h>
#include <mq/system/heap.h>
#include <mq/interfaces/display.h>
#include <mq/interfaces/keyboard.h>
#include <mq/modules/mmu.h>
#include "watch.h"

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

DisplayGlWindow DGW;
Texture displayTexture;
RichText::Text ConsoleText;

/* Refresh request triggered by SDL events and Dear ImGui's initial frames. */
static int render_needed = IMGUI_SETTLING_FRAMES;

static mqMachine *mach = nullptr;

// GUI one-frame input information
struct GUIInput {
    bool mq_initialize_addin_fx = false;
    bool mq_initialize_addin_cg = false;
    int mq_load_working_folder_addin = -1;
    bool mq_heap_init = false;
    bool mq_mmu_unbind = false;
    bool mq_mmu_bind = false;

    bool ui_pattern_mono = false;
    bool ui_pattern_rgb = false;

    bool quit = false;
};

// GUI information that must survive multiple frames
struct GUIState {
    // Filled asynchronously by file input dialog
    struct OpenFileBuffer inputFile;
    std::vector<std::string> workingFolderAddins;
    int mq_cycles = 0;

    bool watch_enabled = true;
    struct WatchInfo watch_info = {
        .fd = -1,
        .wd = -1,
        .addin_path = "",
    };
};

struct GUIInput input;
struct GUIState state;

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
    ConsoleText.addLine(line);
}

static bool HexViewer_ReadByte(u64 addr, u8 *result)
{
    if(!mach || !mach->memory)
        return false;
    u32 v;
    bool b = mq_memory_read_pure(mach->memory, addr, 1, &v);
    if(b)
        *result = v;
    return b;
}

static ImGui::HexViewer HV = {
    .AddressBits = 32,
    .MinAddress = 0,
    .MaxAddress = 0xffffffff,
    .InputType = ImGui::HexViewer::InputFunction,
    .ReadByte = HexViewer_ReadByte,
    .LineSpacing = 2,
    .AlignXCenter = false,
    .Cursor = 0,
};
static HexViewerWindowState HVWS {};
static MemoryWindowState MWS {};
static MemoryBuffersWindowState MBWS {};

static void resetWindowStates(void)
{
    MWS = MemoryWindowState();
    MBWS = MemoryBuffersWindowState();
    HVWS = HexViewerWindowState();
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

    if(mach && mach->display && mach->display->dirty) {
        mqDisplay *d = mach->display;
        displayTexture.bind();
        if(d->format == MQ_DISPLAY_FORMAT_L8) {
#if AZUR_GRAPHICS_OPENGL_ES_2_0 || AZUR_GRAPHICS_OPENGL_ES_3_0
            displayTexture.setFormat(GL_LUMINANCE, GL_UNSIGNED_BYTE,
                                     d->width, d->height);
#elif AZUR_GRAPHICS_OPENGL_3_3
            displayTexture.setFormat(GL_RED, GL_UNSIGNED_BYTE,
                                     d->width, d->height);
#endif
            displayTexture.setData(d->data);
        }
        else if(d->format == MQ_DISPLAY_FORMAT_RGB565) {
            displayTexture.setFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                                     d->width, d->height);
            displayTexture.setData(d->data);
        }
        else
            printf("warning: display not updated, unknown format!\n");

        DGW.setInherentScale(d->width <= 128 ? 3 : 1);
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
            bool paused = state.mq_cycles == 0;
            bool stuck = mach->stuck;

            if(paused && ImGui::IconButton(0, "Run"))
                state.mq_cycles = -1;
            if(!paused && ImGui::IconButton(1, "Pause"))
                state.mq_cycles = 0;
            ImGui::SameLine(0, 6);

            if(ImGui::IconButton(2, "Step", !paused))
                state.mq_cycles = 1;
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
        openFileDialog(&state.inputFile);

    auto dock = ImGui::DockSpaceOverViewport();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if(ImGui::Begin("Display", nullptr, 0)) {
        DGW.AddWindow();
        ImVec2 TL(DGW.x(), DGW.y() + DGW.height());
        ImVec2 BR(TL.x + DGW.width(), TL.y + DGW.padding().w);
        ImGui::PushClipRect(TL, BR, false);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(TL, BR, 0xff1c1712);

        ImGui::SetCursorScreenPos({TL.x + 4, TL.y + 4});
        ImGui::Text("%dx%d - center (%.1f,%.1f) - zoom %d%%",
            DGW.width(), DGW.height(), DGW.viewX(), DGW.viewY(),
            (int)(DGW.viewScale() * 100));

        ImVec2 cursor = ImGui::GetIO().MousePos;
        if(DGW.inWindow(cursor)) {
            ImVec2 pointing = DGW.viewLocation(cursor.x, cursor.y);
            ImGui::SameLine(0);
            ImGui::Text("- pointing at (%.1f,%.1f)", pointing.x, pointing.y);
        }

        ImGui::PopClipRect();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if(ImGui::Begin("Keyboard", nullptr, 0)) {
        mqKeyboard *kbd = mach->keyboard;
        if(kbd) {
            for(uint i = 0; i < kbd->keyCount; i++) {
                mqKeyboardKey *key = &kbd->keyInfo[i];
                float x = key->geometry.x, y = key->geometry.y;
                float w = key->geometry.w, h = key->geometry.h;
                char str[64];
                snprintf(str, sizeof str, "%s##key%d", key->name, i);
                ImGui::SetCursorPos({x, y});
                if(mq_keyboard_isKeyPressed(kbd, i)) {
                    // TODO: Visual effect for keyboard-based key presses
                    // (or add a shortcut to the button-not sure what's best)
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5);
                    ImGui::Button(str, {w, h});
                    ImGui::PopStyleVar();
                }
                else
                    ImGui::Button(str, {w, h});
                mq_keyboard_setKeyPressed(kbd, i, ImGui::IsItemActive());
            }

            if(kbd && ImGui::IsWindowFocused()) {
                ImGui::SetNextFrameWantCaptureKeyboard(true);
                if(ImGui::IsKeyDown(ImGuiKey_LeftArrow))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_LEFT, true);
                if(ImGui::IsKeyDown(ImGuiKey_UpArrow))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_UP, true);
                if(ImGui::IsKeyDown(ImGuiKey_DownArrow))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_DOWN, true);
                if(ImGui::IsKeyDown(ImGuiKey_RightArrow))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_RIGHT, true);
                if(ImGui::IsKeyDown(ImGuiKey_LeftShift))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_SHIFT, true);
                if(ImGui::IsKeyDown(ImGuiKey_Enter))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_EXE, true);
                if(ImGui::IsKeyDown(ImGuiKey_Escape))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_EXIT, true);
                if(ImGui::IsKeyDown(ImGuiKey_F1))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_F1, true);
                if(ImGui::IsKeyDown(ImGuiKey_F2))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_F2, true);
                if(ImGui::IsKeyDown(ImGuiKey_F3))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_F3, true);
                if(ImGui::IsKeyDown(ImGuiKey_F4))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_F4, true);
                if(ImGui::IsKeyDown(ImGuiKey_F5))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_F5, true);
                if(ImGui::IsKeyDown(ImGuiKey_F6))
                    mq_keyboard_setKeycodePressed(kbd, MQ_KEY_F6, true);
            }
        }
    }
    ImGui::End();

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

        ImGui::Checkbox2("Show demo window", &show_demo_window);

        if(state.workingFolderAddins.size() == 0)
            ImGui::Text("(No add-ins in working folder)");
        else
            ImGui::Text("Reset and load:");

        int spaceLeft = 0;
        for(uint i = 0; i < state.workingFolderAddins.size(); i++) {
            char const *addin = state.workingFolderAddins[i].c_str();
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

        if(mach->initialized) {
            ImGui::Text("Cycle:");
            ImGui::SameLine();
            if(ImGui::Button("1"))
                state.mq_cycles = 1;
            ImGui::SameLine();
            if(ImGui::Button("10"))
                state.mq_cycles = 10;
            ImGui::SameLine();
            if(ImGui::Button("100"))
                state.mq_cycles = 100;
            ImGui::SameLine();
            if(ImGui::Button("1000"))
                state.mq_cycles = 1000;
            ImGui::SameLine();
            if(ImGui::Button("10k"))
                state.mq_cycles = 10000;
        }
        else {
            ImGui::Text("Machine is not initialized.");
        }
        if(mach->stuck) {
            ImGui::Text("Machine is stuck!");
            state.mq_cycles = 0;
        }
        if(state.mq_cycles) {
            if(state.mq_cycles > 0)
                ImGui::Text("Cycles pending: %d", state.mq_cycles);
            else
                ImGui::Text("Running...");
        }
    }
    ImGui::End();

    if(ImGui::Begin("Messages", nullptr)) {
        static RichText::View view = {
            .font = fontMono,
            .scroll = 0,
        };
        if(ImGui::Button("Clear"))
            ConsoleText.clear();
        ImGui::AddRichTextFrame(ConsoleText, view);
    }
    ImGui::End();

    if(ImGui::Begin("CPU", nullptr,
            ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Sleeping: %d", (int)mach->cpu.sleeping);
        ImGui::PushFont(fontMono);

        ImGui::BeginGroup();
        for(int i = 0; i < 16; i++)
            ImGui::Text("r%d:%s %08x", i, i < 10 ? " " : "", mach->cpu.r[i]);
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::Text("pc:    %08x", mach->cpu.pc);
        ImGui::Text("gbr:   %08x", mach->cpu.spRegs[SH_GBR]);
        ImGui::Text("mach:  %08x", mach->cpu.spRegs[SH_MACH]);
        ImGui::Text("macl:  %08x", mach->cpu.spRegs[SH_MACL]);
        ImGui::Text("pr:    %08x", mach->cpu.spRegs[SH_PR]);

        u32 SR = mach->cpu.spRegs[SH_SR];
        ImGui::Text("sr:    %08x", mach->cpu.spRegs[SH_SR]);
        ImGui::Text(" MD=%d RB=%d BL=%d",
            (SR >> 30) & 1, (SR >> 29 & 1), (SR >> 28) & 1);
        ImGui::Text(" IMASK=%d",
            (SR >> 4) & 0xf);
        ImGui::Text(" RC=%d",
            (SR >> 16) & 0xfff);
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::Text("vbr:     %08x", mach->cpu.spRegs[SH_VBR]);
        ImGui::Text("ssr:     %08x", mach->cpu.spRegs[SH_SSR]);
        ImGui::Text("spc:     %08x", mach->cpu.spRegs[SH_SPC]);
        ImGui::Text("sgr:     %08x", mach->cpu.spRegs[SH_SGR]);
        ImGui::Text("dbr:     %08x", mach->cpu.spRegs[SH_DBR]);
        ImGui::Text("dsr:     %08x", mach->cpu.spRegs[SH_DSR]);
        for(int i = 0; i < 8; i++)
            ImGui::Text("r%d_bank: %08x", i, mach->cpu.spRegs[SH_RnBANK + i]);
        // ImGui::Text();
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::Text("[DSP]");
        ImGui::Text("a0:  %02x.%08x",
            mach->cpu.spRegs[SH_A0G], mach->cpu.spRegs[SH_A0]);
        ImGui::Text("a1:  %02x.%08x",
            mach->cpu.spRegs[SH_A1G], mach->cpu.spRegs[SH_A1]);
        ImGui::Text("x0:     %08x", mach->cpu.spRegs[SH_X0]);
        ImGui::Text("x1:     %08x", mach->cpu.spRegs[SH_X1]);
        ImGui::Text("y0:     %08x", mach->cpu.spRegs[SH_Y0]);
        ImGui::Text("y1:     %08x", mach->cpu.spRegs[SH_Y1]);
        ImGui::Text("m0:     %08x", mach->cpu.spRegs[SH_M0]);
        ImGui::Text("m1:     %08x", mach->cpu.spRegs[SH_M1]);
        ImGui::Text("mod:    %08x", mach->cpu.spRegs[SH_MOD]);
        ImGui::Text("rs:     %08x", mach->cpu.spRegs[SH_RS]);
        ImGui::Text("re:     %08x", mach->cpu.spRegs[SH_RE]);
        // ImGui::Text();
        ImGui::EndGroup();

        ImGui::PopFont();
    }
    ImGui::End();

    AddInterruptsWindow(mach);

    MemoryWindowAction MWA = AddMemoryWindow(mach, MWS);
    (void)MWA;

    MemoryBuffersWindowAction MBWA = AddMemoryBuffersWindow(mach, MBWS);
    if(MBWA.type == MemoryBuffersWindowAction::Type::MBWA_VIEW_HEX) {
        HV.Cursor = MBWA.address;
        HV.MinAddress = MBWA.address;
        HV.MaxAddress = MBWA.address + (MBWA.size - 1);
        HVWS.currentBuffer = MBWA.buffer;
        HVWS.currentBufferOffset = MBWA.offset;
        HVWS.currentBufferSize = MBWA.size;
    }

    if(ImGui::Begin("Heap")) {
        u32 heapStart, heapEnd;
        bool initialized = mq_heap_isInitialized(&heapStart, &heapEnd);
        if(initialized) {
            ImGui::Text("Heap from %08x to %08x", heapStart, heapEnd);

            mq_heap_debug_t dbg = mq_heap_debuginfo();
            #define X(NAME) \
                ImGui::Text(#NAME ":"); \
                ImGui::SameLine(); \
                ImGui::Text(dbg.NAME ? "true" : "false");
            X(sequence_covers)
            X(sequence_terminator)
            X(sequence_coherent_used)
            X(sequence_footer_size)
            X(sequence_merged_free)
            X(list_structure)
            X(index_covers)
            X(index_class_separation)
            #undef X
        }
        else {
            ImGui::Text("System heap is not initialized");
            if(ImGui::Button("Initialize"))
                input.mq_heap_init = true;
        }
    }
    ImGui::End();

    MMUWindowAction MMUWA = AddMMUWindow(mach);
    if(MMUWA.type == MMUWindowAction::Type::MMUWA_BIND)
        input.mq_mmu_bind = true;
    if(MMUWA.type == MMUWindowAction::Type::MMUWA_UNBIND)
        input.mq_mmu_unbind = true;

    HexViewerWindowAction HVWA = AddHexViewerWindow(mach, HVWS, HV);
    (void)HVWA;

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

        ImGui::DockBuilderDockWindow("Display", dock);
        ImGui::DockBuilderDockWindow("Keyboard", dock_right_bottom);
        ImGui::DockBuilderDockWindow("Control", dock_left_top);
        ImGui::DockBuilderDockWindow("Messages", dock_left_top_right);
        ImGui::DockBuilderDockWindow("CPU", dock_left_top_right);
        ImGui::DockBuilderDockWindow("Interrupts", dock_left_top_right);
        ImGui::DockBuilderDockWindow("Memory tree", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Memory buffers", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Heap", dock_left_bottom);
        ImGui::DockBuilderDockWindow("MMU", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Hex Viewer", dock_left_bottom_right);
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

static bool generate_mono_pattern(mqDisplay *display)
{
    if(!mq_display_setFormat(display, MQ_DISPLAY_FORMAT_L8, 128, 64))
        return false;

    int r0 = rand() % 2 + 1;
    int r1 = rand() % 3 + 1;
    int r2 = rand() % 2;
    int r3 = rand() % 4 + 2;
    u8 palette[4] = { 0x00, 0x55, 0xaa, 0xff };
    for(uint y = 0; y < display->height; y++)
    for(uint x = 0; x < display->width; x++) {
        int c1 = x ^ y;
        int c2 = (x >> r3) ^ r0 * ((x - r2*y) >> 1);
        int c3 = (y >> 1) ^ ((x+y) >> r1);
        int c = c1 ^ c2 ^ c3;
        ((u8 *)display->data)[display->width * y + x] = palette[c & 3];
    }

    for(uint y = 0; y < display->height; y++) {
        ((u8 *)display->data)[display->width * y + 0] = 0xff;
        ((u8 *)display->data)[display->width * (y+1) - 1] = 0xff;
    }
    for(uint x = 0; x < display->width; x++) {
        ((u8 *)display->data)[display->width * 0 + x] = 0xff;
        ((u8 *)display->data)[display->width * (display->height-1) + x] = 0xff;
    }

    mq_display_setDirty(display, true);
    return true;
}

#define C_RGB(R, G, B) (((R) << 11) + ((G) << 5) + (B))

static bool generate_rgb_pattern(mqDisplay *display)
{
    if(!mq_display_setFormat(display, MQ_DISPLAY_FORMAT_RGB565, 396, 224))
        return false;

    u16 palette[16];

    /* Generate a cool looking image pattern */
    for(int i = 0; i < 16; i++) {
        int top = rand() & 31;
        int bot = rand() & top;
        int which = rand() % 3;
        if(which == 0)
            palette[i] = C_RGB(top, bot, bot);
        else if(which == 1)
            palette[i] = C_RGB(bot, top, bot);
        else
            palette[i] = C_RGB(bot, bot, top);
    }

    for(uint y = 0; y < display->height; y++)
    for(uint x = 0; x < display->width; x++) {
        int c1 = x ^ y;
        int c2 = (x >> 5) ^ (x >> 1) ^ (x >> 6);
        int c3 = (y >> 1) ^ (y >> 4) ^ (y >> 5);
        int c = c1 ^ c2 ^ c3;
        ((u16 *)display->data)[display->width * y + x] = palette[c & 15];
    }

    for(uint y = 0; y < display->height; y++) {
        ((u16 *)display->data)[display->width * y + 0] = 0xffff;
        ((u16 *)display->data)[display->width * (y+1) - 1] = 0xffff;
    }
    for(uint x = 0; x < display->width; x++) {
        ((u16 *)display->data)[display->width * 0 + x] = 0xffff;
        ((u16 *)display->data)[display->width * (display->height-1) + x] = 0xffff;
    }

    mq_display_setDirty(display, true);
    return true;
}

//---

static void open_addin(std::string const &path, void *data, long size)
{
    if(path.ends_with(".g1a") || path.ends_with(".G1A")) {
        resetWindowStates();
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_FX);
        mq_machine_load_g1a(mach, data, size);
    }
    else if(path.ends_with(".g3a") || path.ends_with(".G3A")) {
        resetWindowStates();
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
            DGW.markViewDirty(true);
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
    if(state.watch_enabled) {
        enum WatchEvent event;
        while(true) {
            event = watch_poll(&state.watch_info);
            if(event == MQ_WATCH_EVT_NONE)
                break;
            if(event == MQ_WATCH_EVT_DELETED)
                mq_log(MQ_LOG_WARNING, "- addin has been removed");
            if(event == MQ_WATCH_EVT_UPDATED) {
                mq_log(MQ_LOG_DEBUG, "watch: addin has been updated");
                path = state.watch_info.addin_path;
            }
        }
    }
    if(input.mq_load_working_folder_addin >= 0)
        path = state.workingFolderAddins[input.mq_load_working_folder_addin];
    if(!path.empty()) {
        long size;
        void *data = openAndReadFile(path.c_str(), &size);
        if(data) {
            state.inputFile.path = path;
            state.inputFile.data = data;
            state.inputFile.size = size;
        }
        if(state.watch_enabled) {
            if(watch_init(&state.watch_info, path) != 0)
                mq_log(MQ_LOG_ERROR, "unable to watch the file o(x_x)o");
        }
    }

    /* Intentional re-check */
    if(state.inputFile.data) {
        open_addin(
            state.inputFile.path,
            state.inputFile.data,
            state.inputFile.size
        );
        free(state.inputFile.data);
        state.inputFile = OpenFileBuffer();
        render_needed = std::max(render_needed, 1);
    }
    else if(state.mq_cycles) {
        ZoneScopedN("update mq");

        /* Cycle until we reach 12 milliseconds */
        struct timespec ts_start;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);
        // printf("ts_start=%ld\n", ts_start.tv_nsec);

        while(state.mq_cycles != 0 /* negative is infinity */) {
            struct timespec ts_current;
            clock_gettime(CLOCK_MONOTONIC, &ts_current);
            int64_t ns_elapsed = (ts_current.tv_nsec - ts_start.tv_nsec);
            int64_t s_elapsed = (ts_current.tv_sec - ts_start.tv_sec);
            ns_elapsed += 1'000'000'000ull * s_elapsed;
            // printf("ts_current=%ld ns_elapsed=%ld\n", ts_current.tv_nsec, ns_elapsed);
            if(ns_elapsed >= 12'000'000)
                break;

            int cycles = std::min(state.mq_cycles, 100000);
            if(cycles < 0) {
                cycles = 100000;
                // printf("state.mq_cycles=%d, cycles=%d\n", state.mq_cycles, cycles);
            }
            else {
                // printf("state.mq_cycles=%d, cycles=%d\n", state.mq_cycles, cycles);
                state.mq_cycles -= cycles;
            }
            mq_machine_cycle(mach, cycles);
        }
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_heap_init) {
        mq_casiowin_initHeap(mach);
    }

    if(input.ui_pattern_mono && mach->display) {
        generate_mono_pattern(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(input.ui_pattern_rgb && mach->display) {
        generate_rgb_pattern(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_mmu_bind && mach) {
        mq_mmu_bind(mach);
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_mmu_unbind && mach) {
        mq_mmu_unbind(mach);
        render_needed = std::max(render_needed, 1);
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

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "C.UTF-8");
    printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);

    ConsoleText.alloc(65536, 256);
    mq_log_handler(handle_log);

    //todo: better arg handling
    int i = 1;
    while(i < argc) {
        if(strcmp("--watch", argv[i]) == 0) {
            mq_log(MQ_LOG_DEBUG, "activate watch feature");
            state.watch_enabled = true;
            i += 1;
            continue;
        }
        if(strcmp("--version", argv[i]) == 0) {
            printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);
            return 0;
        }
        mq_log(MQ_LOG_ERROR, "unknown argument '%s'", argv[i]);
        return 1;
    }

    mq_init();

    mach = mq_machine_create();

    if(azur_init("MQ", 1500, 850) != 0)
        return 1;
    if(!azur_init_imgui())
        return 1;

    srand(clock());

    DGW.init(displayTexture);

    if(mach->display)
        generate_rgb_pattern(mach->display);

    /* Generate an example display for a texture */
    displayTexture.init(GL_TEXTURE_2D);
    displayTexture.bind();

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
    find_cwd_addins(state.workingFolderAddins);

    int rc = azur_main_loop(render, 60, update, -1, AZUR_MAIN_LOOP_TIED);

    DGW.cleanup();

    azur_quit();
    if(mach) {
        mq_machine_destroy(mach);
        mach = nullptr;
    }
    mq_quit();
    return rc;
}
