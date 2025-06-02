#include "texture.h"

void Texture::init(GLuint type, GLuint name)
{
    m_type = type;
        m_name = name;
    if(!m_name) {
        glGenTextures(1, &m_name);
        m_ownsName = true;
    }
}

void Texture::cleanup()
{
    if(m_ownsName)
        glDeleteTextures(1, &m_name);
}

void Texture::bind() const
{
    glBindTexture(m_type, m_name);
}

uint Texture::nextPowerOfTwo(uint n)
{
    if(!n)
        return 0;
    uint m = 1;
    while(m < n)
        m <<= 1;
    return m;
}

void Texture::setFormat(
    GLuint format, GLuint pixelType, uint width, uint height)
{
    uint width2 = nextPowerOfTwo(width);
    uint height2 = nextPowerOfTwo(height);

    if(format != m_format || pixelType != m_pixelType || width2 != m_width2 ||
       height2 != m_height2) {
        glTexImage2D(m_type, 0, format, width2, height2, 0, format, pixelType,
                     NULL);
        m_format = format;
        m_pixelType = pixelType;
        m_width2 = width2;
        m_height2 = height2;
    }

    m_width = width;
    m_height = height;
}

void Texture::setData(void *data)
{
    glTexSubImage2D(m_type, 0, 0, 0, m_width, m_height, m_format, m_pixelType,
                    data);
}
