#include "gui.h"
#include <mq/machine.h>
#include <mq/system/heap.h>
#include <mq/interfaces/display.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <azur/azur.h>
#include <azur/log.h>
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory>

DisplayGlWindow DGW;
Texture displayTexture;
RichText::Text ConsoleText;

/* Refresh request triggered by SDL events and Dear ImGui's initial frames. */
static int render_needed = IMGUI_SETTLING_FRAMES;

static mqMachine *mach = nullptr;

struct DelayedInput {
    bool mq_initialize_addin_fx = false;
    bool mq_initialize_addin_cg = false;
    bool mq_initialize_gravity_duck = false;
    int mq_cycles = 0;
    bool mq_heap_init = false;

    bool ui_pattern_mono = false;
    bool ui_pattern_rgb = false;
};

struct DelayedInput input;

ImFont *fontSans = nullptr;
ImFont *fontMono = nullptr;

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

static void render(void)
{
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
            displayTexture.setFormat(GL_RED, GL_UNSIGNED_BYTE,
                                     d->width, d->height);
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
        mqDisplay_setDirty(d, false);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(width, height));

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
        // ImVec2 p = ImGui::GetCursorScreenPos();
        // ImDrawList* draw_list = ImGui::GetWindowDrawList();
        // draw_list->AddText({p.x+30,p.y+30}, 0xffff00ff, "F1");
        // draw_list->AddText({p.x+60,p.y+30}, 0xffff00ff, "F2");

        char const *names[] = {
            "F1",     "F2",     "F3",     "F4",     "F5",     "F6",
            "SHIFT",  "OTPN",   "VARS",   "MENU",   "◀",      "▲",
            "ALPHA",  "x2",     "^",      "EXIT",   "▼",      "▶",
            "XOT",    "log",    "ln",     "sin",    "cos",    "tan",
            "o/o",    "S<->D",  "(",      ")",      ",",      "→",
            "7",      "8",      "9",      "DEL",    "AC/ON",
            "4",      "5",      "6",      "×",      "÷",
            "1",      "2",      "3",      "+",      "-",
            "0",      ".",      "x10^",   "(-)",    "EXE",
        };
        int i = 0;
        for(int y = 0; y < 9; y++) {
            int rowLength = 6, width = 60;
            if(y >= 5)
                    rowLength = 5, width = 72;

            for(int x = 0; x < rowLength; x++) {
                ImGui::SetCursorPos({20.f + width * x, 30.f + 30 * y});
                ImGui::Button(names[i], {width-5.f, 24});
                i++;
            }
        }
    }
    ImGui::End();

    static bool show_demo_window = false;

    if(ImGui::Begin("Control", nullptr, 0)) {
        struct mallinfo2 mi = mallinfo2();
        /* This info is not available with AddressSanitizer's wrapper's */
        if(mi.arena || mi.hblkhd)
            ImGui::Text("Memory allocated: %.1f MB heap + %.1f MB mmap\n",
                (float)mi.arena / 1e6, (float)mi.hblkhd / 1e6);

        ImGui::Checkbox("Show demo window", &show_demo_window);

        if(ImGui::Button("128x64 mono"))
            input.ui_pattern_mono = true;
        ImGui::SameLine();
        if(ImGui::Button("396x224 16-bit"))
            input.ui_pattern_rgb = true;

        if(ImGui::Button("Reset add-in FX"))
            input.mq_initialize_addin_fx = true;
        ImGui::SameLine();
        if(ImGui::Button("Reset add-in CG"))
            input.mq_initialize_addin_cg = true;

        if(ImGui::Button("Reset and load GravityDuck.g3a"))
            input.mq_initialize_gravity_duck = true;

        if(mach->initialized) {
            ImGui::Text("Cycle:");
            ImGui::SameLine();
            if(ImGui::Button("1"))
                input.mq_cycles = 1;
            ImGui::SameLine();
            if(ImGui::Button("10"))
                input.mq_cycles = 10;
            ImGui::SameLine();
            if(ImGui::Button("100"))
                input.mq_cycles = 100;
            ImGui::SameLine();
            if(ImGui::Button("1000"))
                input.mq_cycles = 1000;
            ImGui::SameLine();
            if(ImGui::Button("1 million"))
                input.mq_cycles = 1000000;
            if(ImGui::Button("Run"))
                input.mq_cycles = -1;
            ImGui::SameLine();
            if(ImGui::Button("Pause"))
                input.mq_cycles = 0;
        }
        else {
            ImGui::Text("Machine is not initialized.");
        }
        if(mach->stuck) {
            ImGui::Text("Machine is stuck!");
            input.mq_cycles = 0;
        }
        if(input.mq_cycles) {
            if(input.mq_cycles > 0)
                ImGui::Text("Cycles pending: %d", input.mq_cycles);
            else
                ImGui::Text("Running...");
        }
    }
    ImGui::End();

    if(ImGui::Begin("Console", nullptr)) {
        static RichText::View view = {
            .font = fontMono,
            .scroll = 0,
        };
        ImGui::Text("WIP...");
        if(ImGui::Button("New line (x10)")) {
            static int i = 0;
            for(int j = 0; j < 10; j++) {
                char str[64];
                sprintf(str, "a pretty long message that will wrap #%d", ++i);
                RichText::Line *l = RichText::Line::make(str);
                ConsoleText.addLine(l);
            }
        }
        ImGui::AddRichTextFrame(ConsoleText, view);
    }
    ImGui::End();

#if 0
if(ImGui::Begin("Text Test"))
{
struct Segment {
    Segment(char const *text, ImU32 col=0, bool underline=false):
        textStart(text),
        textEnd(text + strlen(text)),
        color(col),
        underline(underline) {}

    char const *textStart;
    char const *textEnd;
    ImU32 color;
    bool underline;
};

Segment segs[] = {
    Segment("this is a really super duper long segment that should wrap all on its own "),
    Segment("http://google.com", IM_COL32(127,127,255,255), true),
    Segment(" Short text "),
    Segment("http://github.com", IM_COL32(127,127,255,255), true)
};

ImGui::TextColored(ImColor(0, 255, 0, 255), "Half-manual wrapping");

const float wrapWidth = ImGui::GetContentRegionAvail().x;
for(int i = 0; i < IM_ARRAYSIZE(segs); ++i)
{
    char const *textStart = segs[i].textStart;
    char const *textEnd = segs[i].textEnd ? segs[i].textEnd : textStart + strlen(textStart);

    ImFont *Font = ImGui::GetFont();

    do {
        float widthRemaining = ImGui::CalcWrapWidthForPos(ImGui::GetCursorScreenPos(), 0.0f);
        char const *drawEnd = Font->CalcWordWrapPositionA(1.0f, textStart, textEnd, widthRemaining);
        if(drawEnd == textStart) {
            ImGui::NewLine();
            drawEnd = Font->CalcWordWrapPositionA(1.0f, textStart, textEnd, wrapWidth);
        }

        if(segs[i].color)
            ImGui::PushStyleColor(ImGuiCol_Text, segs[i].color);
        ImGui::TextUnformatted(textStart, drawEnd == textStart ? nullptr : drawEnd);
        if(segs[i].color)
            ImGui::PopStyleColor();

        if(segs[i].underline) {
            ImVec2 lineEnd = ImGui::GetItemRectMax();
            ImVec2 lineStart = lineEnd;
            lineStart.x = ImGui::GetItemRectMin().x;
            ImGui::GetWindowDrawList()->AddLine(lineStart, lineEnd, segs[i].color);

            if(ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
                ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        }

        if(drawEnd == textStart || drawEnd == textEnd) {
            ImGui::SameLine(0.0f, 0.0f);
            break;
        }

        textStart = drawEnd;

        /* Skip spaces around line wrapping spot */
        while(textStart < textEnd) {
            if(ImCharIsBlankA(*textStart)) { textStart++; }
            else if(*textStart == '\n') { textStart++; break; }
            else break;
        }
    } while (true);
}
}
ImGui::End();
#endif

    if(ImGui::Begin("CPU", nullptr,
            ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::PushFont(fontMono);

        ImGui::BeginGroup();
        for(int i = 0; i < 16; i++)
            ImGui::Text("r%d:%s %08x", i, i < 10 ? " " : "", mach->cpu.r[i]);
        ImGui::EndGroup();

        ImGui::SameLine(0, 40);
        ImGui::BeginGroup();
        ImGui::Text("pc:    %08x", mach->cpu.pc);
        ImGui::Text("sr:    %08x", mach->cpu.spRegs[SH_SR]);
        ImGui::Text("gbr:   %08x", mach->cpu.spRegs[SH_GBR]);
        ImGui::Text("mach:  %08x", mach->cpu.spRegs[SH_MACH]);
        ImGui::Text("macl:  %08x", mach->cpu.spRegs[SH_MACL]);
        ImGui::Text("pr:    %08x", mach->cpu.spRegs[SH_PR]);
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

    static ImGui::HexViewer HV = {
        .AddressBits = 32,
        .ReadByte = HexViewer_ReadByte,
        .AlignXCenter = false,
        .Cursor = 0,
    };
    static MemoryWindowState MWS {};
    MemoryWindowAction MWA = AddMemoryWindow(mach, MWS);

    if(MWA.type == MWA.Type::MWA_VIEW_HEX)
        HV.Cursor = MWA.address;

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

    if(ImGui::Begin("Hex Viewer")) {
        ImGui::PushFont(fontMono);
        ImGui::AddHexViewer(HV);
        ImGui::PopFont();
    }
    ImGui::End();

    static bool first_frame = true;
    if(first_frame) {
        auto dock_left_top = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Left, 0.68f, nullptr, &dock);
        auto dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Down, 0.5f, nullptr, &dock_left_top);
        auto dock_left_top_right = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Right, 0.65f, nullptr, &dock_left_top);
        auto dock_left_bottom_right = ImGui::DockBuilderSplitNode(
            dock_left_bottom,
            ImGuiDir_Right, 0.51f, nullptr, &dock_left_bottom);
        auto dock_right_bottom = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Down, 0.6f, nullptr, &dock);

        ImGui::DockBuilderDockWindow("Display", dock);
        ImGui::DockBuilderDockWindow("Keyboard", dock_right_bottom);
        ImGui::DockBuilderDockWindow("Control", dock_left_top);
        ImGui::DockBuilderDockWindow("Console", dock_left_top_right);
        ImGui::DockBuilderDockWindow("CPU", dock_left_top_right);
        ImGui::DockBuilderDockWindow("Memory", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Heap", dock_left_bottom);
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
}

static bool generate_mono_pattern(mqDisplay *display)
{
    if(!mqDisplay_setFormat(display, MQ_DISPLAY_FORMAT_L8, 128, 64))
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

    mqDisplay_setDirty(display, true);
    return true;
}

#define C_RGB(R, G, B) (((R) << 11) + ((G) << 5) + (B))

static bool generate_rgb_pattern(mqDisplay *display)
{
    if(!mqDisplay_setFormat(display, MQ_DISPLAY_FORMAT_RGB565, 396, 224))
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

    mqDisplay_setDirty(display, true);
    return true;
}

//---

static int update(void)
{
    SDL_Event e;
    int cyclesLeft = 0;

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
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_FX);
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_initialize_addin_cg) {
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_CG);
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_initialize_gravity_duck) {
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_CG);
        mq_machine_load_g3a(mach, "GravityDuck.g3a");
        render_needed = std::max(render_needed, 1);
    }
    else if(input.mq_cycles) {
        /* Limit the number of cycles for each GUI frame to not lock the GUI.
           TODO: Proper "real-time" controls, e.g limit to 20 ms.
           I don't think I want threads here, too annoying to sync. */
        int cycles = std::min(input.mq_cycles, 100000);
        cyclesLeft = input.mq_cycles - cycles;
        if(cycles < 0) {
            cycles = 100000;
            cyclesLeft = -1;
        }
        mq_machine_cycle(mach, cycles);
        render_needed = std::max(render_needed, 1);
    }
    if(input.mq_heap_init) {
        mq_mach_initHeap(mach);
    }

    if(input.ui_pattern_mono && mach->display) {
        generate_mono_pattern(mach->display);
        render_needed = std::max(render_needed, 1);
    }
    if(input.ui_pattern_rgb && mach->display) {
        generate_rgb_pattern(mach->display);
        render_needed = std::max(render_needed, 1);
    }

    input = DelayedInput();
    input.mq_cycles = cyclesLeft;

    return 0;
}

int main(void)
{
    printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);

    ConsoleText.alloc(1024, 30);
    mq_log_handler(handle_log);

    mach = mq_machine_create();

    if(azur_init("MQ", 1366, 768) != 0)
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

    SDL_Window *window = azur_sdl_window();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    if(!ImGui_ImplSDL2_InitForOpenGL(window, SDL_GL_GetCurrentContext())) {
        azlog(FATAL, "ImGui_ImplSDL2_InitForOpenGL failed\n");
        return 1;
    }

    #if defined AZUR_GRAPHICS_OPENGL_3_3
    char const *glsl_version = "#version 130";
    #elif defined AZUR_GRAPHICS_OPENGL_ES_2_0
    char const *glsl_version = "#version 100";
    #endif
    if(!ImGui_ImplOpenGL3_Init(glsl_version)) {
        azlog(FATAL, "ImGui_ImplOpenGL3_Init(\"%s\") failed\n", glsl_version);
        return 1;
    }

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = NULL;
    // TODO: Embed assets in native build to dodge workdir requirement
    fontSans = io.Fonts->AddFontFromFileTTF("gui/assets/DejaVuSans.ttf", 13.0f);
    fontMono = io.Fonts->AddFontFromFileTTF("gui/assets/DejaVuSansMono.ttf", 13.0f);
    io.Fonts->AddFontDefault();

    ImGui_LoadMQStyle(ImGui::GetStyle());

    int rc = azur_main_loop(render, 60, update, -1, AZUR_MAIN_LOOP_TIED);

    DGW.cleanup();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    azur_quit();
    if(mach) {
        mq_machine_destroy(mach);
        mach = nullptr;
    }
    return rc;
}
