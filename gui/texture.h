// C++ abstraction for a simple OpenGL texture.

#ifndef MQ_UI_TEXTURE_H
#define MQ_UI_TEXTURE_H

#include <azur/gl/gl.h>

class Texture
{
public:
    Texture() = default;
    Texture(Texture const &) = delete;
    Texture(Texture &&) = delete;

    void init(GLuint type, GLuint name=0);
    void cleanup();

    /* Whether this holds a valid, initialized texture. */
    bool isValid() const { return m_name != 0; }

    /* Bind the texture to the resource of the corresponding type. */
    void bind() const;

    /* Set the texture's format and size. This automatically rounds up to a
       power of 2, reallocates the texture's GPU memory if needed and is a
       no-op if the current format and size match the request. The texture must
       be bound for this call. */
    void setFormat(GLuint format, GLuint pixelType, uint width, uint height);

    /* Load data into the texture. If the format might have changed, call
       setFormat() before. The texture must be bound for this call. */
    void setData(void *data);

    /* Get current format and size. */
    GLuint format() const { return m_format; }
    GLuint pixelType() const { return m_pixelType; }
    uint width() const { return m_width; }
    uint height() const { return m_height; }
    /* Get storage size (size rounded up to next power of 2). */
    uint storageWidth() const { return m_width2; }
    uint storageHeight() const { return m_height2; }

private:
    static uint nextPowerOfTwo(uint n);

    GLuint m_name = 0;
    bool m_ownsName = false;

    GLuint m_type = 0;
    GLuint m_format = 0;
    GLuint m_pixelType = 0;

    /* Size as seen by user */
    uint m_width = 0;
    uint m_height = 0;
    /* Storage size obtained by rounding up to the next power of 2 */
    uint m_width2 = 0;
    uint m_height2 = 0;
};

#endif /* MQ_UI_TEXTURE_H */
