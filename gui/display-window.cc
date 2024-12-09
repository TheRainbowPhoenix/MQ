#include "gui.h"

//=== Display window =========================================================//

void DisplayGlWindow::init(Texture const &texture)
{
    shader_texture.init();
    shader_background.init();
    m_texture = &texture;
}

void DisplayGlWindow::cleanup(void)
{
    shader_texture.cleanup();
    shader_background.cleanup();
    m_texture = nullptr;
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
    mat3 tr_world2pixel(
         esc,                       0.0f,                       0.0f,
         0.0f,                      esc,                        0.0f,
         width() / 2 - m_vx * esc,  height() / 2 - m_vy * esc,  1.0f);

    mat3 tr_pixel2gl = windowToOpenGLMatrix();
    mat3 tr_world2gl = tr_pixel2gl * tr_world2pixel;

    glUseProgram(shader_texture.prog);
    shader_texture.set_uniform("u_transform", tr_world2gl);

    glUseProgram(shader_background.prog);
    shader_background.set_uniform("u_pixel2gl", tr_pixel2gl);
    shader_background.set_uniform("u_view", m_vx, m_vy, esc);
}

void DisplayGlWindow::render(ImDrawList const *, ImDrawCmd const *)
{
    if(m_needShaderUniformUpdate > 0) {
        updateShaderUniforms();
        m_needShaderUniformUpdate--;
    }

    shader_texture.vertices.clear();
    shader_background.vertices.clear();

    /* Draw background */
    shader_background.add_background(0, 0, width(), height());
    shader_background.draw();

    if(m_texture && m_texture->isValid()) {
        float sw = m_texture->storageWidth();
        float sh = m_texture->storageHeight();
        float w = m_texture->width();
        float h = m_texture->height();

        /* Go to the negatives to benefit from clamping (on bottom right side
           the texture continues till next power of 2) */
        // FIXME: Not sure why this needs 0.25 and not 0.5 to work well.
        float esc = viewEffectiveScale();
        float u0 = 0.25 / sw / esc;
        float v0 = 0.25 / sh / esc;
        float tw = w / sw;
        float th = h / sh;
        shader_texture.add_subtexture(
            -w/2, -h/2, w, h,
            u0, v0, tw, th, m_texture->format() == GL_RED);

        m_texture->bind();
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

void ProgramTexture::init()
{
    Shader::init();
    glBindVertexArray(this->vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    // TODO: Better way to access Azur's internal shaders
    extern char const *azur_glsl__vs_tex2d;
    extern char const *azur_glsl__fs_tex2d;

    this->prog = azur::gl::loadProgramSources(
        GL_VERTEX_SHADER,   azur_glsl__vs_tex2d,
        GL_FRAGMENT_SHADER, azur_glsl__fs_tex2d,
        0);
}

void ProgramTexture::set_vertex_attributes() const
{
    glVertexAttribPointer(glGetAttribLocation(this->prog, "a_vertex"),
        2, GL_FLOAT, GL_FALSE,
        sizeof(ProgramTexture_Attributes),
        (void *)offsetof(ProgramTexture_Attributes, vertex)
    );
    glVertexAttribPointer(glGetAttribLocation(this->prog, "a_texture_pos"),
        2, GL_FLOAT, GL_FALSE,
        sizeof(ProgramTexture_Attributes),
        (void *)offsetof(ProgramTexture_Attributes, uv)
    );
    glVertexAttribPointer(glGetAttribLocation(this->prog, "a_grayscale"),
        1, GL_INT, GL_FALSE,
        sizeof(ProgramTexture_Attributes),
        (void *)offsetof(ProgramTexture_Attributes, grayscale)
    );
}

void ProgramTexture::add_texture(
    int x, int y, int width, int height, bool grayscale)
{
    return add_subtexture(x, y, width, height, 0.0, 0.0, 1.0, 1.0, grayscale);
}

void ProgramTexture::add_subtexture(int x, int y, int width, int height,
    float u, float v, float tw, float th, bool grayscale)
{
    ProgramTexture_Attributes attr[4] = {
        { vec2(x,       y),        vec2(u, v),       (int)grayscale },
        { vec2(x+width, y),        vec2(u+tw, v),    (int)grayscale },
        { vec2(x,       y+height), vec2(u, v+th),    (int)grayscale },
        { vec2(x+width, y+height), vec2(u+tw, v+th), (int)grayscale },
    };

    this->vertices.push_back(attr[0]);
    this->vertices.push_back(attr[1]);
    this->vertices.push_back(attr[2]);
    this->vertices.push_back(attr[1]);
    this->vertices.push_back(attr[2]);
    this->vertices.push_back(attr[3]);
}

//=== Checkered background shader ============================================//

void ProgramBackground::init()
{
    Shader::init();
    glBindVertexArray(this->vao);
    glEnableVertexAttribArray(0);

    this->prog = azur::gl::loadProgramFiles(
        GL_VERTEX_SHADER,   "gui/glsl/vs_tiles.glsl",
        GL_FRAGMENT_SHADER, "gui/glsl/fs_tiles.glsl",
        0);
}

void ProgramBackground::set_vertex_attributes() const
{
    glVertexAttribPointer(glGetAttribLocation(this->prog, "a_vertex"),
        2, GL_FLOAT, GL_FALSE,
        sizeof(ProgramBackground_Attributes),
        (void *)offsetof(ProgramBackground_Attributes, vertex)
    );
}

void ProgramBackground::add_background(int x, int y, int w, int h)
{
    ProgramBackground_Attributes attr[4] = {
        { vec2(x,   y)   },
        { vec2(x+w, y)   },
        { vec2(x,   y+h) },
        { vec2(x+w, y+h) },
    };

    this->vertices.push_back(attr[0]);
    this->vertices.push_back(attr[1]);
    this->vertices.push_back(attr[2]);
    this->vertices.push_back(attr[1]);
    this->vertices.push_back(attr[2]);
    this->vertices.push_back(attr[3]);
}
