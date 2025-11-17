// All windows that we throw around in the main GUI

#ifndef MQ_UI_WINDOWS_H
#define MQ_UI_WINDOWS_H

#include "imgui-util.h"
#include <azur/opengl.h>
#include <mq/machine.h>
#include <mq/memory.h>
#include <optional>

/* Azur utility; move to Azur later */
namespace azur::gl {

/* A GPU buffer, such as a VBO. */
class Buffer
{
public:
    Buffer(GLuint target): m_target{target} {}
    ~Buffer() { reset(); }

    virtual bool init();
    virtual void reset();
    virtual void update() = 0;

    GLuint target() const { return m_target; }
    GLuint id() const { return m_id; }

    /* Bind the buffer to its set target. */
    void bind() const;

    /* [While bound]
       Allocate the buffer to `size` bytes and initialize it with `data`
       (unless `data` is null, in which case no initialization). `usage` is the
       usage hint to the GPU for (maybe) optimizing allocation strategies. */
    void alloc(void const *data, size_t size, GLenum usage);

    /* [While bound]
       Load `size` bytes from `data` into the buffer at position `offset`. No
       allocation is performed. The designated interval must be in bounds. */
    void load(size_t offset, void const *data, size_t size);

    /* [While bound]
       Assign the contents of the buffer and resize it if needed, according to
       the resize policy. This is similar to alloc() but avoids reallocations
       if within the policy.
       TODO: Configurable policies, currently allows up to 2x the size. */
    void assign(void const *data, size_t size, GLenum usage);

private:
    Resource<GLuint> m_id = 0;
    /* Buffer type */
    GLuint const m_target;
    /* Size of the allocated VBO on the GPU */
    size_t m_gpuSize = 0;
};

/* A VBO backed by a vector on the CPU side. */
template<typename T>
class VectorVBO: public Buffer
{
public:
    VectorVBO(): Buffer(GL_ARRAY_BUFFER) {}
    void reset() override;
    void update() override;
    void clear() { m_vertices.clear(); }

    size_t size() const { return m_vertices.size(); }
    void *data() { return m_vertices.data(); }
    void const *data() const { return m_vertices.data(); }

    void addTriangle(T const *vertices3);
    void addUnfoldedQuad(T const *vertices4);

private:
    /* CPU-side storage */
    std::vector<T> m_vertices;
};

template<typename T>
void VectorVBO<T>::reset()
{
    Buffer::reset();
    m_vertices.clear();
}

template<typename T>
void VectorVBO<T>::update()
{
    assign(m_vertices.data(), m_vertices.size() * sizeof(T), GL_DYNAMIC_DRAW);
}

template<typename T>
void VectorVBO<T>::addTriangle(T const *verts)
{
    m_vertices.push_back(verts[0]);
    m_vertices.push_back(verts[1]);
    m_vertices.push_back(verts[2]);
}

template<typename T>
void VectorVBO<T>::addUnfoldedQuad(T const *verts)
{
    m_vertices.push_back(verts[0]);
    m_vertices.push_back(verts[1]);
    m_vertices.push_back(verts[2]);
    m_vertices.push_back(verts[1]);
    m_vertices.push_back(verts[2]);
    m_vertices.push_back(verts[3]);
}

}

//=== Display window =========================================================//

/* 2D texture shader rendering subrectangles in quads. */

class ShaderTexture: public azur::gl::Program
{
    struct VA {
        glm::vec2 vertex;   // Vertex location in world view space
        glm::vec2 uv;       // Location within texture (normalized)
        float grayscale;    // Grayscale if > 0.5
    };

public:
    bool init() override;
    void reset() override;

    void clear() { m_vbo.clear(); }
    void draw();

    /* Add a full texture of the specified size
       TODO: This always uses texture #0 */
    void addTexture(
        rect<float> screenRect, rect<float> texRect, bool grayscale=false);

private:
    GLuint m_vao = 0;
    azur::gl::VectorVBO<VA> m_vbo;
};

/* Checkered background shader, rendering on full rectangles. */

class ShaderBackground: public azur::gl::Program
{
    struct VA { glm::vec2 vertex; };

public:
    bool init() override;
    void reset() override;

    void clear() { m_vbo.clear(); }
    void draw();

    /* Add the background at specified pixel coordinates */
    void addBackground(rect<int> coord);

private:
    GLuint m_vao = 0;
    azur::gl::VectorVBO<VA> m_vbo;

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
    ShaderTexture shader_texture;
    ShaderBackground shader_background;

    /* Display texture */
    azur::gl::Texture2D m_texture;
};

/* Class of an instanced window along with some automation. */
class GUIWindow
{
public:
    GUIWindow(int instanceId, char const *title, ImGuiWindowFlags flags = 0):
        m_instanceId{instanceId}, m_title{title}, m_flags{flags} {}
    virtual ~GUIWindow() {}

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
    HexViewerWindow(char const *title);
    void renderContents(mqMachine *omach) override;
    void resetState() override;

    /* Switch to a buffer given by name (or "") for full address space. The
       section spanning the given offset/size will be displayed. And the entire
       segment will be shown at the given address. */
    void viewBuffer(std::string bufferName, u32 address, u32 offset, u32 size);

private:
    static bool ReadByte(u64 addr, u8 *result, void *userdata);
    ImGui::HexViewer m_HexViewer;

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

class RecordWindow: public GUIWindow
{
public:
    RecordWindow(char const *title): GUIWindow(-1, title) {}
    void renderContents(mqMachine *omach) override;

    static constexpr auto scaleFactors = {1, 2, 3, 4, 8};
};

#endif /* MQ_UI_WINDOWS_H */
