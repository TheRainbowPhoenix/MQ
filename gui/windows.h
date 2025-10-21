// All windows that we throw around in the main GUI

#ifndef MQ_UI_WINDOWS_H
#define MQ_UI_WINDOWS_H

#include "imgui-util.h"
#include "shader.h"
#include <azur/opengl.h>
#include <mq/machine.h>
#include <mq/memory.h>
#include <optional>

//=== Display window =========================================================//

/* 2D texture shader rendering subrectangles in quads. */

struct ProgramTexture_Attributes {
    glm::vec2 vertex; // Vertex location in OpenGL coordinate space
    glm::vec2 uv;     // Location within texture
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
    glm::vec2 vertex; // Vertex location
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
    void init();
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

    azur::gl::Texture2D &texture() { return m_texture; }

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
    azur::gl::Texture2D m_texture;
};

/* Class of an instanced window along with some automation. */
class GUIWindow
{
public:
    GUIWindow(int instanceId, char const *title, ImGuiWindowFlags flags = 0):
        m_instanceId{instanceId}, m_title{title}, m_flags{flags} {}

    /* Render the contents of the window, without Begin()/End() */
    // TODO: Give windows an [mqMachine const *] to enforce observer semantics
    virtual void renderContents(mqMachine *omach) = 0;
    /* Render entire window, with Begin()/End() and the flags given when
       construced. This handles generating a unique instance-based window ID */
    virtual void render(mqMachine *omach);
    /* Reset state after changing the underlying machine */
    virtual void resetState() {}

    /* Window title */
    char const *title() const { return m_title; }
    void setTitle(char const *title) { m_title = title ? title : ""; }
    std::string uniqueTitle() const {
        return (
            std::string(title()) + "##" +
            std::string(title()) + "." +
            std::to_string(m_instanceId)
        );
    }

private:
    int m_instanceId = -1;
    char const *m_title = "";
    ImGuiWindowFlags m_flags = 0;
};

//=== Control ================================================================//

class ControlWindow: public GUIWindow
{
public:
    ControlWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;

    bool showDemoWindow() const { return m_showDemoWindow; }

private:
    bool m_showDemoWindow = false;
};

//=== Messages ===============================================================//

class MessagesWindow: public GUIWindow
{
public:
    MessagesWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
};

//=== CPU ====================================================================//

class CPUWindow: public GUIWindow
{
public:
    CPUWindow(char const *title):
        GUIWindow(-1, title, ImGuiWindowFlags_HorizontalScrollbar) {}
    void renderContents(mqMachine *omach) override;
};

//=== Memory tree ============================================================//

class MemoryTreeWindow: public GUIWindow
{
public:
    MemoryTreeWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
    void resetState() override;

    // TODO: Action for opening an address in Hex Viewer
    void AddChunkList(mqMemory const *omem);
    void AddPageList(u32 chunkBase, mqChunkPointer chunkPtr);
    void AddMMIOList(u32 addr, mqPagePointer pagePtr);

private:
    int m_selectedChunk = -1;
    int m_selectedPage = -1;
    int m_selectedIO = -1;
};

//=== Memory Buffers window ==================================================//

class MemoryBuffersWindow: public GUIWindow
{
public:
    MemoryBuffersWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
    void resetState() override;

private:
    int m_selectedBuffer = -1;
};

//=== MMU window =============================================================//

class MMUWindow: public GUIWindow
{
public:
    MMUWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
};

//=== Interrupts window ======================================================//

class InterruptsWindow: public GUIWindow
{
public:
    InterruptsWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
};

//=== Hex Viewer window ======================================================//

class HexViewerWindow: public GUIWindow
{
public:
    HexViewerWindow(char const *title, ImGui::HexViewer &HV):
        GUIWindow(-1, title), m_HexViewer {HV} {}
    void renderContents(mqMachine *omach) override;
    void resetState() override;

    /* Switch to a buffer given by name (or "") for full address space. The
       section spanning the given offset/size will be displayed. And the entire
       segment will be shown at the given address. */
    void viewBuffer(std::string bufferName, u32 address, u32 offset, u32 size);

private:
    static bool ReadByte(u64 addr, u8 *result, void *userdata);
    ImGui::HexViewer &m_HexViewer;

    /* If empty, we're viewing the entire memory. Otherwise we're viewing just
       that particular buffer. */
    std::string m_currentBufferName = "";
    /* Offset and size of the buffer section we're looking into. This keeps
       track of whether we're looking at the full buffer or just a subset. */
   int m_currentBufferOffset = 0;
   int m_currentBufferSize = 0;
};

//=== Heap ===================================================================//

class HeapWindow: public GUIWindow
{
public:
    HeapWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
};

//=== Display ================================================================//

class DisplayWindow: public GUIWindow
{
public:
    DisplayWindow(char const *title, DisplayGlWindow &DGW):
        GUIWindow(-1, title), m_DGW{DGW} {}
    void render(mqMachine *omach) override;
    void renderContents(mqMachine *omach) override;

private:
    DisplayGlWindow &m_DGW;
};

//=== Keyboard ===============================================================//

class KeyboardWindow: public GUIWindow
{
public:
    KeyboardWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
};

//=== Record =================================================================//

#include "record.h"
#include <sys/stat.h>

class RecordWindow: public GUIWindow
{
public:
    RecordWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;
    void resetState() override;

    mqRecordRequest request(std::string const &ext);
    uint scale() const {
        return (m_record_scale == 3) ? 8 : (unsigned)m_record_scale + 2;
    }
    std::string &filename(std::string const &ext);
    bool filenameExists() {
        struct stat buffer;
        return (
            stat(filename(".png").c_str(), &buffer) == 0 ||
            stat(filename(".mp4").c_str(), &buffer) == 0
        );
    }

    //move me
    void replace_all(
        std::string &text,
        std::string const &toReplace,
        std::string const &replaceWith
    ) const;

private:
    char m_record_filename_format[128] = "mq_%ADDIN%_%TIME%";
    std::string m_record_filename = "";
    int m_record_scale = 0;
    int m_record_encoder = 0;
    int m_record_start_opt = 0;
    int m_record_reset_opt = 0;
    bool m_dirty = true;
    bool m_record_encoder_is_valid = true;
};

#endif /* MQ_UI_WINDOWS_H */
