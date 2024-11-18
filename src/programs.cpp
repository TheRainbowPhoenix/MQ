#include "programs.h"
#include <glm/glm.hpp>

//---
// 2D Texture shader
//---

ProgramTexture::ProgramTexture(): Program()
{
    glBindVertexArray(this->vao);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

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
}

void ProgramTexture::add_texture(int x, int y, int width, int height)
{
    return add_subtexture(x, y, width, height, 0.0, 0.0, 1.0, 1.0);
}

void ProgramTexture::add_subtexture(int x, int y, int width, int height,
    float u, float v, float tw, float th)
{
    ProgramTexture_Attributes attr[4] = {
        { vec2(x,       y),        vec2(u, v) },
        { vec2(x+width, y),        vec2(u+tw, v) },
        { vec2(x,       y+height), vec2(u, v+th) },
        { vec2(x+width, y+height), vec2(u+tw, v+th) },
    };

    this->vertices.push_back(attr[0]);
    this->vertices.push_back(attr[1]);
    this->vertices.push_back(attr[2]);
    this->vertices.push_back(attr[1]);
    this->vertices.push_back(attr[2]);
    this->vertices.push_back(attr[3]);
}

//---
// Spell background shader
//---

ProgramBackground::ProgramBackground(): Program()
{
    glBindVertexArray(this->vao);
    glEnableVertexAttribArray(0);

    this->prog = azur::gl::loadProgramFiles(
        GL_VERTEX_SHADER,   "glsl/vs_tiles.glsl",
        GL_FRAGMENT_SHADER, "glsl/fs_tiles.glsl",
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
