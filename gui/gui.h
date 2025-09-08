// Main header for all UI definitions that aren't reusable utilities

#ifndef MQ_UI_GUI_H
#define MQ_UI_GUI_H

#include "imgui-util.h"
#include "shader.h"
#include "texture.h"
#include <mq/machine.h>
#include <mq/memory.h>

//=== Display window =========================================================//

/* 2D texture shader rendering subrectangles in quads. */

struct ProgramTexture_Attributes {
    vec2 vertex;      // Vertex location in OpenGL coordinate space
    vec2 uv;          // Location within texture
    float grayscale;  // 1.0 for grayscale mode (red is all channels)
};

struct ProgramTexture: public Shader<ProgramTexture_Attributes>
{
    void init();
    void set_vertex_attributes() const override;

    /* Add a full texture of the specified size
       TODO: This always uses texture #0
       TODO: Specify sub-regions */
    void add_texture(int x, int y, int width, int height, bool grayscale);

    void add_subtexture(int x, int y, int width, int height,
    float u, float v, float tw, float th, bool grayscale);
};

/* Checkered background shader, rendering on full rectangles. */

struct ProgramBackground_Attributes {
    vec2 vertex;    // Vertex location
};

struct ProgramBackground: public Shader<ProgramBackground_Attributes>
{
    void init();
    void set_vertex_attributes() const override;

    /* Add the background at specified pixel coordinates */
    void add_background(int x, int y, int w, int h);
};

class DisplayGlWindow: public ImGuiGlWindow
{
public:
    void init(Texture const &texture);
    void cleanup();

    void render(ImDrawList const *, ImDrawCmd const *) override;
    void AddWindow() override;

    /* Marks the view as dirty when the layout changes or the view is panned/
       zoomed by user interaction. */
    void markViewDirty(bool resettle=false) {
        m_needShaderUniformUpdate += resettle ? IMGUI_SETTLING_FRAMES : 1;
    }

    /* View parameters. */
    float viewX() const { return m_vx; }
    float viewY() const { return m_vy; }
    float viewScale() const { return m_vscale; }
    float viewEffectiveScale() const { return m_vscale * m_inherentScale; }

    /* View location for a global pixel position. */
    ImVec2 viewLocation(int gx, int gy) const;

    /* Get or set inherent scale, a scaling factor tied to the texture contents
       that applies on top of standard view scale. This is to scale the 128x64
       display more than the 396x224 by default without user interaction. */
    void setInherentScale(float scale);
    float inherentScale() const { return m_inherentScale; }

private:
    void updateShaderUniforms();

    /* Flag to mark when the shader uniforms need to be updated. */
    int m_needShaderUniformUpdate = IMGUI_SETTLING_FRAMES;

    /* View settings. vx/vy are texture center relative to window center. */
    float m_vx = 0.0f;
    float m_vy = 0.0f;
    float m_vscale = 1.0f;
    float m_inherentScale = 1.0f;

    /* Shaders used in the window (not shared) */
    ProgramTexture shader_texture;
    ProgramBackground shader_background;

    /* Display texture */
    Texture const *m_texture = nullptr;
};

//=== Memory window ==========================================================//

/* State retained from one frame to the next in the memory window. */
struct MemoryWindowState {
    /* Current selection */
    int selectedChunk = -1;
    int selectedPage = -1;
    int selectedIO = -1;
};
/* Actions emitted from the memory window. */
struct MemoryWindowAction {
};

MemoryWindowAction AddMemoryWindow(mqMachine *mach, MemoryWindowState &state);

MemoryWindowAction AddMemoryWindowContents(
    mqMachine *mach, MemoryWindowState &state);

//=== Memory Buffers window ==================================================//

struct MemoryBuffersWindowState {
    int selectedBuffer = -1;
};
struct MemoryBuffersWindowAction {
    enum class Type { MBWA_NONE, MBWA_VIEW_HEX };
    Type type = Type::MBWA_NONE;
    /* Region of buffer we want to visualize, and matching emulated address */
    mqMemoryBuffer *buffer = NULL;
    int offset = -1;
    int size = -1;
    u32 address = 0;
};

MemoryBuffersWindowAction AddMemoryBuffersWindow(
    mqMachine *mach, MemoryBuffersWindowState &state);

MemoryBuffersWindowAction AddMemoryBuffersWindowContents(
    mqMachine *mach, MemoryBuffersWindowState &state);

//=== MMU window =============================================================//

/* Actions emitted from the MMU window. */
struct MMUWindowAction {
    enum class Type { MMUWA_NONE, MMUWA_UNBIND, MMUWA_BIND };
    Type type = Type::MMUWA_NONE;
};

MMUWindowAction AddMMUWindow(mqMachine *mach);
MMUWindowAction AddMMUWindowContents(mqMachine *mach);

//=== Interrupts window ======================================================//

void AddInterruptsWindow(mqMachine *mach);
void AddInterruptsWindowContents(mqMachine *mach);

//=== Hex Viewer window ======================================================//

struct HexViewerWindowState {
    /* If NULL, we're viewing the entire memory. Otherwise we're viewing just
       that particular buffer. */
    mqMemoryBuffer *currentBuffer = NULL;
    /* Offset and size of the buffer section we're looking into. This keeps
       track of whether we're looking at the full buffer or just a subset. */
   int currentBufferOffset = 0;
   int currentBufferSize = 0;
};
struct HexViewerWindowAction {
};

HexViewerWindowAction AddHexViewerWindow(
    mqMachine *mach, HexViewerWindowState &state, ImGui::HexViewer &HV);
HexViewerWindowAction AddHexViewerWindowContents(
    mqMachine *mach, HexViewerWindowState &state, ImGui::HexViewer &HV);

#endif /* MQ_UI_GUI_H */
