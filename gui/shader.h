// C++ abstraction for an OpenGL shader with a linked program, a VAO and a VBO.

#ifndef MQ_UI_SHADER_H
#define MQ_UI_SHADER_H

#include <azur/gl/gl.h>
#include <vector>
#include "util.h"

template<typename VertexAttr>
struct Shader
{
    /* Since this holds non-trivial OpenGL objects, disable copy/move */
    Shader() = default;
    virtual ~Shader() = default;
    Shader(Shader const &other) = delete;
    Shader(Shader &&other) = delete;

    void init();
    void cleanup();

    /* Adds a vertex to the vertex data buffer */
    void add_vertex(VertexAttr const &attributes);

    /* Set vertex attributes when the buffer is loaded */
    virtual void set_vertex_attributes() const = 0;

    /* Update the VBO and draw */
    void prepare_draw();
    void draw();

    /* Utilities to set uniforms */
    void set_uniform(char const *name, float f);
    void set_uniform(char const *name, float f1, float f2);
    void set_uniform(char const *name, float f1, float f2, float f3);
    void set_uniform(char const *name, glm::vec2 const &v2);
    void set_uniform(char const *name, glm::vec3 const &v3);
    void set_uniform(char const *name, glm::vec4 const &v4);
    void set_uniform(char const *name, mat2 const &m2);
    void set_uniform(char const *name, mat3 const &m3);
    void set_uniform(char const *name, mat4 const &m4);

    /* Program ID */
    GLuint prog = 0;
    /* Vertex Array Object and Vertex Buffer Object with parameters */
    GLuint vao = 0, vbo = 0;

    /* Vertex attributes to be loaded in the VBO */
    std::vector<VertexAttr> vertices;
    /* Size of the VBO, in number of vertices */
    /* Number of spots currently used in [vertex_data] */
    size_t vbo_size = 0;
};

template<typename T>
void Shader<T>::init()
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
}

template<typename T>
void Shader<T>::cleanup()
{
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

template<typename T>
void Shader<T>::add_vertex(T const &attributes)
{
    vertices.push_back(attributes);
}

template<typename T>
void Shader<T>::prepare_draw()
{
    if(!vertices.size()) return;

    glBindVertexArray(vao);
    glUseProgram(prog);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    /* If the size of the VBO is too small or much larger than needed, resize
       it; otherwise, simply swap the data without reallocating */
    if(vbo_size < vertices.size() || vbo_size > vertices.size() * 4) {
        glBufferData(GL_ARRAY_BUFFER, sizeof(T) * vertices.size(),
            vertices.data(), GL_DYNAMIC_DRAW);
    }
    else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(T) * vertices.size(),
            vertices.data());
    }

    this->set_vertex_attributes();
}

template<typename T>
void Shader<T>::draw()
{
    prepare_draw();

    // TODO: Do not hardcode the specific operation GL_TRIANGLES
    glDrawArrays(GL_TRIANGLES, 0, vertices.size());
}

template<typename T>
void Shader<T>::set_uniform(char const *name, float f)
{
    glUniform1f(glGetUniformLocation(this->prog, name), f);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, float f1, float f2)
{
    glUniform2f(glGetUniformLocation(this->prog, name), f1, f2);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, float f1, float f2, float f3)
{
    glUniform3f(glGetUniformLocation(this->prog, name), f1, f2, f3);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, glm::vec2 const &v2)
{
    glUniform2fv(glGetUniformLocation(this->prog, name), 1, &v2[0]);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, glm::vec3 const &v3)
{
    glUniform3fv(glGetUniformLocation(this->prog, name), 1, &v3[0]);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, glm::vec4 const &v4)
{
    glUniform4fv(glGetUniformLocation(this->prog, name), 1, &v4[0]);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, mat2 const &m2)
{
    glUniformMatrix2fv(glGetUniformLocation(this->prog, name), 1, GL_FALSE,
        &m2[0][0]);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, mat3 const &m3)
{
    glUniformMatrix3fv(glGetUniformLocation(this->prog, name), 1, GL_FALSE,
        &m3[0][0]);
}

template<typename T>
void Shader<T>::set_uniform(char const *name, mat4 const &m4)
{
    glUniformMatrix4fv(glGetUniformLocation(this->prog, name), 1, GL_FALSE,
        &m4[0][0]);
}

#endif /* MQ_UI_SHADER_H */
