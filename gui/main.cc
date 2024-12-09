#include <azur/azur.h>
#include <azur/log.h>
#include <azur/gl/gl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <memory>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>

#include <mq/machine.h>

#include "util.h"
#include "programs.h"

static std::unique_ptr<ProgramTexture> shader_texture;
static std::unique_ptr<ProgramBackground> shader_background;

/* A pixel art texture for tests/debugging */
static GLuint tex_image;

/* Refresh request triggered by SDL events (used to avoid useless renders). For
   some reason Dear ImGui needs a couple of frames to initialize */
static int render_needed = 5;

/* Transformation settings for the camera */
static float view_x=0.0f, view_y=0.0f;
static float view_scale=1.0f;
/* Spell location in pixels (set by Dear ImGui code each frame) */
static int spell_x=0, spell_y=0, spell_w=0, spell_h=0;
/* Whether view needs update based on window movement */
static int spell_needs_update = true;

static bool view_needs_update = true;

static mqMachine *mach = nullptr;

struct input {
    bool mq_initialize_addin_fx = false;
    bool mq_initialize_addin_cg = false;

    bool ui_pattern_mono = false;
    bool ui_pattern_rgb = false;
};

struct input delayed_input;

static ImFont *fontSans = nullptr;
static ImFont *fontMono = nullptr;

static void view_update(void)
{
    SDL_Window *window = azur_sdl_window();
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    /* Make sure we align on unit boundaries */
    float vx = roundf(view_x);
    float vy = roundf(view_y);

    /* We have 4 coordinate systems:
       * world:  The spell world (1.0f = view_scale pixels)
       * pixel:  Pixel coordinates in the Dear ImGui spell window (0 is center)
       * screen: Pixel coordinates within the program's window
       * gl:     Normalized GL coordinates of the program's window */

    mat3 tr_world2pixel(
         view_scale,                  0.0f,                       0.0f,
         0.0f,                        view_scale,                 0.0f,
         -vx * view_scale,            -vy * view_scale,           1.0f);

    mat3 tr_pixel2screen(
         1.0f,                        0.0f,                       0.0f,
         0.0f,                        1.0f,                       0.0f,
         spell_x + spell_w / 2.0f,    spell_y + spell_h / 2.0f,   1.0f);

    mat3 tr_screen2gl(
         2.0f / width,                0.0f,                       0.0f,
         0.0f,                       -2.0f / height,              0.0f,
        -1.0f,                        1.0f,                       1.0f);

    mat3 tr_world2gl = tr_screen2gl * tr_pixel2screen * tr_world2pixel;
    mat3 tr_pixel2gl = tr_screen2gl * tr_pixel2screen;

    glUseProgram(shader_texture->prog);
    shader_texture->set_uniform("u_transform", tr_world2gl);

    glUseProgram(shader_background->prog);
    shader_background->set_uniform("u_pixel2gl", tr_pixel2gl);
    shader_background->set_uniform("u_view", view_x, view_y, view_scale);
}

static vec2 cursor_location(void)
{
    ImGuiIO &io = ImGui::GetIO();
    int x = io.MousePos.x - spell_x - spell_w / 2;
    int y = io.MousePos.y - spell_y - spell_h / 2;

    float cx = view_x + (float)x / view_scale;
    float cy = view_y + (float)y / view_scale;
    return vec2(cx, cy);
}

void display_callback(ImDrawList const *, ImDrawCmd const *)
{
    SDL_Window *window = azur_sdl_window();
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    if(spell_needs_update)
        view_update();

    shader_texture->vertices.clear();
    shader_background->vertices.clear();

    /* Draw background */
    shader_background->add_background(-spell_w / 2.0f, -spell_h / 2.0f,
        spell_w, spell_h);

    if(tex_image) {
        /* Go to the negatives to benefit from clamping (on bottom right side
           the texture continues till next power of 2) */
        // FIXME: Not sure why this needs 0.25 and not 0.5 to work well.
        float u0 = 0.25 / 512 / view_scale;
        float v0 = 0.25 / 256 / view_scale;
        float tw = 396. / 512;
        float th = 224. / 256;
        shader_texture->add_subtexture(-396/2, -224/2, 396, 224,
            u0, v0, tw, th);
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(spell_x, height - spell_h - spell_y, spell_w, spell_h);

    shader_background->draw();

    glBindTexture(GL_TEXTURE_2D, tex_image);
    shader_texture->draw();

    glDisable(GL_SCISSOR_TEST);
}

static void TextLR(char const *left, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *str;
    vasprintf(&str, fmt, args);
    va_end(args);

    char const *right = str ? str : "(nil)";

    ImGui::TextUnformatted(left);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x -
        ImGui::CalcTextSize(right).x);
    ImGui::TextUnformatted(right);

    free(str);
}

static void render(void)
{
    if(!render_needed) return;

    if(view_needs_update) {
        view_update();
        view_needs_update = false;
    }

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
        /* InvisibleButton() trick from the custom rendering demo */
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 size = ImGui::GetContentRegionAvail();
        if(size.x < 50) size.x = 50;
        if(size.y < 50) size.y = 50;

        spell_x = p0.x;
        spell_y = p0.y;
        spell_w = size.x;
        spell_h = size.y;

        ImGui::InvisibleButton("SpellInvisibleButton", size,
            ImGuiButtonFlags_MouseButtonLeft |
            ImGuiButtonFlags_MouseButtonRight);
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddCallback(display_callback, nullptr);
        draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

        if(active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            view_x -= (float)io.MouseDelta.x / view_scale;
            view_y -= (float)io.MouseDelta.y / view_scale;
            view_update();
        }

        float wheel = ImGui::GetIO().MouseWheel;
        if(hovered && wheel != 0) {
            /* Only accept +1/-1 for scroll as browser specify wildly
               inconsistent units, which find their way into emscripten.
               <https://github.com/emscripten-core/emscripten/issues/6283> */
            int dy = (wheel < 0) ? -1 : +1;

            /* Zoom around the location of the cursor */
            int x = io.MousePos.x - spell_x - spell_w / 2;
            int y = io.MousePos.y - spell_y - spell_h / 2;

            /* Determine the invariant quantity (which is the world location of
               the point under the cursor) then rotate around that */
            float cx = view_x + (float)x / view_scale;
            float cy = view_y + (float)y / view_scale;

            if(view_scale + dy != 0)
                view_scale += dy; // *= powf(1.2f, dy);
            view_x = cx - (float)x / view_scale;
            view_y = cy - (float)y / view_scale;
            view_update();
        }
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
            "SHIFT",  "OTPN",   "VARS",   "MENU",   "←",      "↑",
            "ALPHA",  "x2",     "^",      "EXIT",   "↓",      "→",
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
    ImVec2 cursor = ImGui::GetIO().MousePos;
    vec2 pointing = cursor_location();

    if(ImGui::Begin("Control", nullptr, 0)) {
        TextLR("Window size", "%dx%d", width, height);
        TextLR("Display window size", "%dx%d", spell_w, spell_h);
        TextLR("Display view center", "(%.6f, %.6f)", view_x, view_y);
        TextLR("View top-left", "(%.6f, %.6f)",
            view_x - spell_w/2/view_scale, view_y - spell_h/2/view_scale);
        TextLR("View bottom-right", "(%.6f, %.6f)",
            view_x + spell_w/2/view_scale, view_y + spell_h/2/view_scale);
        TextLR("Zoom", "%d%%", (int)(view_scale * 100));

        if(cursor.x >= spell_x && cursor.x < spell_x + spell_w &&
           cursor.y >= spell_y && cursor.y < spell_y + spell_h) {
            TextLR("Pointing at", "%.1f, %.1f", pointing.x, pointing.y);
        }
        else {
            TextLR("Pointing at", "-");
        }
        ImGui::Checkbox("Show demo window", &show_demo_window);

        if(ImGui::Button("B&W pattern"))
            delayed_input.ui_pattern_mono = true;
        if(ImGui::Button("RGB pattern"))
            delayed_input.ui_pattern_rgb = true;
    }
    ImGui::End();

    if(ImGui::Begin("Inspector", nullptr, 0)) {
        ImGui::PushFont(fontMono);

        ImGui::BeginGroup();
        for(int i = 0; i < 16; i++)
            ImGui::Text("r%d:%s %08x", i, i < 10 ? " " : "", mach->cpu.gpRegs[i]);
        ImGui::EndGroup();

        ImGui::SameLine(0, 25);
        ImGui::BeginGroup();
        for(uint i = 0; i < SH_NUM_SPECIAL_REGS; i++) {
            char const *name = mq_cpu_spreg_name(i);
            if(name)
                ImGui::Text("%s:%*s %08x", name, 7-(int)strlen(name), "",
                    mach->cpu.spRegs[i]);

            if(i == 15) {
                ImGui::EndGroup();
                ImGui::SameLine(0, 25);
                ImGui::BeginGroup();
            }
        }
        ImGui::EndGroup();

        ImGui::PopFont();
    }
    ImGui::End();

    static bool first_frame = true;
    if(first_frame) {
        auto dock_left_top = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Left, 0.65f, nullptr, &dock);
        auto dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left_top,
            ImGuiDir_Down, 0.5f, nullptr, &dock_left_top);
        auto dock_right_bottom = ImGui::DockBuilderSplitNode(dock,
            ImGuiDir_Down, 0.6f, nullptr, &dock);

        ImGui::DockBuilderDockWindow("Display", dock);
        ImGui::DockBuilderDockWindow("Keyboard", dock_right_bottom);
        ImGui::DockBuilderDockWindow("Control", dock_left_top);
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

    render_needed--;
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
        return ((w + 7) / 8) * h;
    else if(format == RGB565)
        return (w * 2) * h;
    assert(false && "display_realloc: bad format");
}

static uint display_size(struct DisplayData const *d)
{
    return display_size_for(d->format, d->width, d->height);
}

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

    for(uint i = 0; i < display->size; i++)
        ((u8 *)display->data)[i] = rand();
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
        render_needed = std::max(render_needed, 1);

        if(e.type == SDL_QUIT)
            return 1;
        if(e.type == SDL_WINDOWEVENT &&
                e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            view_needs_update = true;
        }
    }

    if(delayed_input.mq_initialize_addin_fx)
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_FX);
    if(delayed_input.mq_initialize_addin_cg)
        mq_machine_initialize(mach, MQ_MACHINE_INITIALIZE_ADDIN_FX);

    if(delayed_input.ui_pattern_mono)
        puts("B&W pattern clicked!");
    if(delayed_input.ui_pattern_rgb) {
        generate_rgb_pattern(&display);
        glTexSubImage2D(GL_TEXTURE_2D, 0, /* x */ 0, /* y */ 0, display.width,
            display.height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, display.data);
        render_needed = std::max(render_needed, 1);
    }

    delayed_input = input();

    return 0;
}

//---
struct Texture
{
    Texture(GLuint type, GLuint name = 0);
    Texture(Texture const &) = delete;
    Texture(Texture &&);

    void bind() const;

private:
    GLuint m_type;
    GLuint m_name;
};

Texture::Texture(GLuint type, GLuint name):
    m_type {type}, m_name {name}
{
    if(!m_name)
        glGenTextures(1, &m_name);
}

void Texture::bind() const
{
    glBindTexture(m_type, m_name);
}
//---

int main(void)
{
    printf("MQ on Azur %d.%d\n", AZUR_VERSION_MAJOR,AZUR_VERSION_MINOR);

    mach = mq_machine_alloc();

    if(azur_init("MQ", 1366, 768) != 0)
        return 1;

    srand(clock());

    shader_texture = std::make_unique<ProgramTexture>();
    shader_background = std::make_unique<ProgramBackground>();

    view_update();

    generate_rgb_pattern(&display);

    /* Generate an example display for a texture */
    glGenTextures(1, &tex_image);
    glBindTexture(GL_TEXTURE_2D, tex_image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 256,
        0, GL_RGB,  GL_UNSIGNED_SHORT_5_6_5, NULL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, /* x */ 0, /* y */ 0, display.width,
        display.height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, display.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenerateMipmap(GL_TEXTURE_2D);

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

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowTitleAlign = ImVec2(0.5, 0.5);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(10, 5);
    style.ItemInnerSpacing = ImVec2(6, 5);
    style.ScrollbarSize = 12;
    style.GrabMinSize = 8;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.FrameRounding = 2;
    style.GrabRounding = 2;
    style.TabRounding = 2;

    style.Colors[ImGuiCol_WindowBg]           = ImVec4(0.2, 0.2, 0.2, 1.0);
    style.Colors[ImGuiCol_FrameBg]            = ImVec4(0.25, 0.275, 0.3, 1.0);
    style.Colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.7, 0.81, 1.0, 0.2);
    style.Colors[ImGuiCol_FrameBgActive]      = ImVec4(0.70f, 0.81f, 1.00f, 0.36f);
    style.Colors[ImGuiCol_TitleBg]            = ImVec4(0.14, 0.14, 0.14, 1.0);
    style.Colors[ImGuiCol_TitleBgActive]      = ImVec4(0.14, 0.14, 0.14, 1.0);
    style.Colors[ImGuiCol_CheckMark]          = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]       = ImVec4(0.18f, 0.18f, 0.19f, 0.97f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.19f, 0.20f, 0.21f, 1.00f);

    style.Colors[ImGuiCol_Border].w = 0.125;
    style.Colors[ImGuiCol_ScrollbarBg].w = 0.25;

    int rc = azur_main_loop(render, 60, update, -1, AZUR_MAIN_LOOP_TIED);

    shader_texture.reset();
    shader_background.reset();

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
