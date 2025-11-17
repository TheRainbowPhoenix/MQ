#include "windows.h"
#include <azur/resources.h>
#include <azur/log.h>

namespace azur::gl {

bool Buffer::init()
{
    glGenBuffers(1, m_id.get());
    return true;
}

void Buffer::reset()
{
    glDeleteBuffers(1, m_id.get());
    m_id = 0;
}

void Buffer::bind() const
{
    glBindBuffer(m_target, m_id);
}

void Buffer::alloc(void const *data, size_t size, GLenum usage)
{
    glBufferData(m_target, size, data, usage);
    m_gpuSize = size;
}

void Buffer::load(size_t offset, void const *data, size_t size)
{
    if(offset + size > m_gpuSize) {
        azlog(ERROR, "Loading %zu bytes at offset %zu into a buffer of "
            "size %zu (%zu + %zu > %zu)",
            size, offset, m_gpuSize, offset, size, m_gpuSize);
    }
    glBufferSubData(m_target, offset, size, data);
}

void Buffer::assign(void const *data, size_t size, GLenum usage)
{
    /* If the size of the VBO is too small or much larger than needed, resize
       it; otherwise, simply swap the data without reallocating */
    if(m_gpuSize < size || m_gpuSize > size * 2)
        alloc(data, size, usage);
    else
        load(0, data, size);
}

} /* namespace azur::gl */

//=== Display window =========================================================//

void DisplayGlWindow::init()
{
    shader_texture.init();
    shader_background.init();
    setPadding({0, 0, 0, 25});

    /* Set texture parameters */
    m_texture.generateName();
    m_texture.bind();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glGenerateMipmap(GL_TEXTURE_2D);
}

void DisplayGlWindow::cleanup(void)
{
    shader_texture.reset();
    shader_background.reset();
    m_texture.reset();
}

ImVec2 DisplayGlWindow::viewLocation(int gx, int gy) const
{
    float esc = viewEffectiveScale();

    /* Cursor location relative to window center, which is the view x/y. */
    int crx = gx - (x() + width() / 2);
    int cry = gy - (y() + height() / 2);

    /* Determine the invariant quantity, which is the world location of
       the point under the cursor, then scale around that */
    float cx = m_vx + (float)crx / esc;
    float cy = m_vy + (float)cry / esc;

    return {cx, cy};
}

void DisplayGlWindow::setInherentScale(float scale)
{
    m_inherentScale = scale;
    markViewDirty();
}

void DisplayGlWindow::updateShaderUniforms()
{
    float esc = viewEffectiveScale();
    glm::mat3 tr_world2pixel(
         esc,                       0.0f,                       0.0f,
         0.0f,                      esc,                        0.0f,
         width() / 2 - m_vx * esc,  height() / 2 - m_vy * esc,  1.0f);

    glm::mat3 tr_pixel2gl = windowToOpenGLMatrix();
    glm::mat3 tr_world2gl = tr_pixel2gl * tr_world2pixel;

    shader_texture.useProgram();
    shader_texture.setUniform("u_transform", tr_world2gl);

    shader_background.useProgram();
    shader_background.setUniform("u_pixel2gl", tr_pixel2gl);

    shader_background.setUniform("u_view",
        glm::vec3(m_vx - width() / 2 / esc,
                  m_vy - height() / 2 / esc, esc));
}

void DisplayGlWindow::render(ImDrawList const *, ImDrawCmd const *)
{
    // TODO: shader uniforms just before rendering for less context swtiches?
    if(m_needShaderUniformUpdate > 0) {
        updateShaderUniforms();
        m_needShaderUniformUpdate--;
    }

    shader_texture.clear();
    shader_background.clear();

    /* Draw background */
    shader_background.addBackground({0, 0, width(), height()});
    shader_background.useProgram();
    shader_background.draw();

    if(m_texture.isValid()) {
        float sw = m_texture.storageSize().x;
        float sh = m_texture.storageSize().y;
        float w = m_texture.width();
        float h = m_texture.height();

        /* Go to the negatives to benefit from clamping (on bottom right side
           the texture continues till next power of 2) */
        // FIXME: Not sure why this needs 0.25 and not 0.5 to work well.
        float esc = viewEffectiveScale();
        float u0 = 0.25 / sw / esc;
        float v0 = 0.25 / sh / esc;
        float tw = w / sw;
        float th = h / sh;
        shader_texture.addTexture(
            {-w/2, -h/2, w, h},
            {u0, v0, tw, th},
            m_texture.format() == GL_R8);

        shader_texture.useProgram();
        m_texture.bind();
        shader_texture.draw();
    }
}

void DisplayGlWindow::AddWindow()
{
    ImGuiGlWindow::AddWindow();
    float esc = viewEffectiveScale();

    ImGuiIO &io = ImGui::GetIO();
    if(isActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        m_vx -= (float)io.MouseDelta.x / esc;
        m_vy -= (float)io.MouseDelta.y / esc;
        markViewDirty();
    }

    float wheel = ImGui::GetIO().MouseWheel;
    if(isHovered() && wheel != 0) {
        /* Only accept +1/-1 for scroll as browser specify wildly
           inconsistent units, which find their way into emscripten.
           <https://github.com/emscripten-core/emscripten/issues/6283> */
        int dy = (wheel < 0) ? -1 : +1;

        /* Cursor location relative to window center, which is the view x/y. */
        int crx = io.MousePos.x - (x() + width() / 2);
        int cry = io.MousePos.y - (y() + height() / 2);

        /* Determine the invariant quantity, which is the world location of
           the point under the cursor, then scale around that */
        float cx = m_vx + (float)crx / esc;
        float cy = m_vy + (float)cry / esc;

        if(m_vscale + dy != 0)
            m_vscale += dy; // *= powf(1.2f, dy);
        esc = viewEffectiveScale();

        m_vx = cx - (float)crx / esc;
        m_vy = cy - (float)cry / esc;

        if(m_vscale == 1) {
            /* Avoid clipping due to non-integral bounds */
            m_vx = round(m_vx);
            m_vy = round(m_vy);
        }
        markViewDirty();
    }
}

//=== 2D texture shader ======================================================//

bool ShaderTexture::init()
{
    if(!azur::gl::Program::init() || !m_vbo.init())
        return false;

    glGenVertexArrays(1, &m_vao);

    setName("texture");
    addPrelude(GL_VERTEX_SHADER);
    addSourceFile(GL_VERTEX_SHADER, "@azur:glsl/vs_tex2d.glsl");
    addPrelude(GL_FRAGMENT_SHADER);
    addSourceFile(GL_FRAGMENT_SHADER, "@azur:glsl/fs_tex2d.glsl");
    /* Attribute locations are already given in GLSL code */
    if(!compile() || !link())
        return false;

    glBindVertexArray(m_vao);
    m_vbo.bind();
    bindVertexAttribute(&VA::vertex, 0);
    bindVertexAttribute(&VA::uv, 1);
    bindVertexAttribute(&VA::grayscale, 2);
    return true;
}

void ShaderTexture::reset()
{
    glDeleteVertexArrays(1, &m_vao);
    m_vbo.reset();
    azur::gl::Program::reset();
}

void ShaderTexture::draw()
{
    m_vbo.bind();
    m_vbo.update();
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vbo.size());
}

void ShaderTexture::addTexture(rect<float> dst, rect<float> tex, bool grayscale)
{
    if(tex.width() == 0 && tex.height() == 0)
        tex = rect<float>(0.f, 0.f, 1.f, 1.f);
    float gr = (float)(int)grayscale;

    VA attr[4] = {
        { {dst.left(),  dst.top()},    {tex.left(),  tex.top()},    gr },
        { {dst.right(), dst.top()},    {tex.right(), tex.top()},    gr },
        { {dst.left(),  dst.bottom()}, {tex.left(),  tex.bottom()}, gr },
        { {dst.right(), dst.bottom()}, {tex.right(), tex.bottom()}, gr },
    };
    m_vbo.addUnfoldedQuad(attr);
}

//=== Checkered background shader ============================================//

bool ShaderBackground::init()
{
    if(!azur::gl::Program::init() || !m_vbo.init())
        return false;

    glGenVertexArrays(1, &m_vao);

    setName("checkerboard");
    addPrelude(GL_VERTEX_SHADER);
    addSourceFile(GL_VERTEX_SHADER, "@mqgui:glsl/vs_tiles.glsl");
    addPrelude(GL_FRAGMENT_SHADER);
    addSourceFile(GL_FRAGMENT_SHADER, "@mqgui:glsl/fs_tiles.glsl");
    /* Attribute locations are already given in GLSL code */
    if(!compile() || !link())
        return false;

    glBindVertexArray(m_vao);
    m_vbo.bind();
    bindVertexAttribute(&VA::vertex, 0);
    return true;
}

void ShaderBackground::reset()
{
    glDeleteVertexArrays(1, &m_vao);
    m_vbo.reset();
    azur::gl::Program::reset();
}

void ShaderBackground::draw()
{
    m_vbo.bind();
    m_vbo.update();
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vbo.size());
}

void ShaderBackground::addBackground(rect<int> coord)
{
    VA attr[4] = {
        { {coord.left(),  coord.top()},    },
        { {coord.right(), coord.top()},    },
        { {coord.left(),  coord.bottom()}, },
        { {coord.right(), coord.bottom()}, },
    };
    m_vbo.addUnfoldedQuad(attr);
}
