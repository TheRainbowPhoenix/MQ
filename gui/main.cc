#include "gui.h"
#include <mq/machine.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <azur/azur.h>
#include <azur/log.h>
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory>

DisplayGlWindow DGW;
Texture displayTexture;

/* Refresh request triggered by SDL events and Dear ImGui's initial frames. */
static int render_needed = IMGUI_SETTLING_FRAMES;

static mqMachine *mach = nullptr;

struct DelayedInput {
    bool mq_initialize_addin_fx = false;
    bool mq_initialize_addin_cg = false;
    bool mq_initialize_gravity_duck = false;
    int mq_cycles = 0;

    bool ui_pattern_mono = false;
    bool ui_pattern_rgb = false;
};

struct DelayedInput input;

static ImFont *fontSans = nullptr;
static ImFont *fontMono = nullptr;

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

        if(ImGui::Button("Cycle"))
            input.mq_cycles = 1;
        if(ImGui::Button("10"))
            input.mq_cycles = 10;
        if(ImGui::Button("100"))
            input.mq_cycles = 100;
        if(ImGui::Button("1000"))
            input.mq_cycles = 1000;
        if(mach->stuck) {
            ImGui::SameLine();
            ImGui::Text("Machine is stuck!");
        }
    }
    ImGui::End();

    if(ImGui::Begin("Inspector", nullptr,
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

    if(ImGui::Begin("Memory", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {
        static ImGui::HexViewer HV = {
            .AddressBits = 32,
            .ReadByte = HexViewer_ReadByte,
            .Cursor = 0,
        };

        // TODO: Avoid recomputation of memory stats every frame?!
        struct mqMemory_Stats s = mq_memory_stats(mach->memory);

        ImGui::Text("1 MB Chunks: %d (%d buffer, %d detailed including %d "
                    "pure MMIO)",
                    s.totalChunks, s.bufferChunks, s.detailedChunks,
                    s.pureMMIOChunks);
        ImGui::Text("4 kB Pages: %d mapped", s.bufferPages);

        mqMemory const *mem = mach->memory;
        for(int i = 0; i < 0x1000; i++) {
            char id[16];
            sprintf(id, "memchunk%d", i);
            if(mem->chunks[i] && ImGui::TreeNode(id, "%08x", i << 20)) {
                // TODO: Nodes are clicked only when closed. Leaves?
                if(ImGui::IsItemClicked()) {
                    HV.Cursor = i << 20;
                }
                ImGui::TreePop();
            }
        }

        ImGuiStyle const &style = ImGui::GetStyle();
        ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
        ImGui::SeparatorText("Hex Viewer");
        ImGui::PopStyleColor();
        ImGui::PushFont(fontMono);
        ImGui::AddHexViewer(HV);
        ImGui::PopFont();
    }
    ImGui::End();

    static bool first_frame = true;
    if(first_frame) {
        auto dock_left_top = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Left, 0.6f, nullptr, &dock);
        auto dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Down, 0.5f, nullptr, &dock_left_top);
        auto dock_left_top_right = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Right, 0.65f, nullptr, &dock_left_top);
        auto dock_right_bottom = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Down, 0.6f, nullptr, &dock);

        ImGui::DockBuilderDockWindow("Display", dock);
        ImGui::DockBuilderDockWindow("Keyboard", dock_right_bottom);
        ImGui::DockBuilderDockWindow("Control", dock_left_top);
        ImGui::DockBuilderDockWindow("Memory", dock_left_top_right);
        ImGui::DockBuilderDockWindow("Inspector", dock_left_bottom);
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

// TODO - Plug in data from the display emulation
enum DisplayFormat { NONE, MONO, RGB565 };
struct DisplayData {
    enum DisplayFormat format = NONE;
    uint width = 0;
    uint height = 0;
    void *data = NULL;
    uint size = 0;
};
struct DisplayData display;

static uint display_size_for(enum DisplayFormat format, uint w, uint h)
{
    if(format == MONO)
        return w * h;
    else if(format == RGB565)
        return (w * 2) * h;
    assert(false && "display_realloc: bad format");
}

// static uint display_size(struct DisplayData const *d)
// {
//     return display_size_for(d->format, d->width, d->height);
// }

static bool display_realloc(
    struct DisplayData *d, enum DisplayFormat format, uint w, uint h)
{
    uint size = display_size_for(format, w, h);

    if(d->format == format && d->width == w && d->height == h) {
        memset(d->data, 0x00, size);
        return true;
    }

    void *newData = malloc(size);
    if(!newData)
        return false;

    free(d->data);

    d->format = format;
    d->width = w;
    d->height = h;
    d->data = newData;
    d->size = size;
    return true;
}

static bool generate_mono_pattern(struct DisplayData *display)
{
    if(!display_realloc(display, MONO, 128, 64))
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
    return true;
}

#define C_RGB(R, G, B) (((R) << 11) + ((G) << 5) + (B))

static bool generate_rgb_pattern(struct DisplayData *display)
{
    if(!display_realloc(display, RGB565, 396, 224))
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

    return true;
}

//---

static int update(void)
{
    SDL_Event e;

    while(SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        render_needed = std::max(render_needed, 2);

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
    if(input.mq_cycles) {
        mq_machine_cycle(mach, input.mq_cycles);
        render_needed = std::max(render_needed, 1);
    }

    if(input.ui_pattern_mono) {
        generate_mono_pattern(&display);
        displayTexture.bind();
        displayTexture.setFormat(GL_RED, GL_UNSIGNED_BYTE,
                                 display.width, display.height);
        displayTexture.setData(display.data);
        DGW.setInherentScale(3);
        render_needed = std::max(render_needed, 1);
    }
    if(input.ui_pattern_rgb) {
        generate_rgb_pattern(&display);
        displayTexture.bind();
        displayTexture.setFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                                 display.width, display.height);
        displayTexture.setData(display.data);
        DGW.setInherentScale(1);
        render_needed = std::max(render_needed, 1);
    }

    input = DelayedInput();

    return 0;
}

int main(void)
{
    printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);

    mach = mq_machine_alloc();

    if(azur_init("MQ", 1366, 768) != 0)
        return 1;

    srand(clock());

    DGW.init(displayTexture);

    generate_rgb_pattern(&display);

    /* Generate an example display for a texture */
    displayTexture.init(GL_TEXTURE_2D);
    displayTexture.bind();
    displayTexture.setFormat(GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                             display.width, display.height);
    displayTexture.setData(display.data);
    DGW.setInherentScale(1);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glGenerateMipmap(GL_TEXTURE_2D);

    // Texture tex_display(GL_TEXTURE_2D);

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
        mq_machine_free(mach);
        mach = nullptr;
    }
    return rc;
}
