// Dear ImGui utilities

#ifndef MQ_UI_IMGUI_UTIL_H
#define MQ_UI_IMGUI_UTIL_H

#include <azur/gl/gl.h>
#include <imgui.h>
#include <glm/glm.hpp>

/* Number of frames we expect Dear ImGui to need to settle its layout after
   starting for the first time, resizing windows, etc. */
#define IMGUI_SETTLING_FRAMES 5

namespace ImGui {

/* A full-line label with a fixed string on the left and a formatted string
   right-aligned on the right side of the current drawing region. */
void TextLR(char const *left, char const *fmt, ...);

/* Separator text, but disabled. */
void SeparatorTextD(char const *str);


} /* namespace ImGui */

void ImGui_LoadMQStyle(ImGuiStyle &style);

//=== Dockable windows rendered with custom OpenGL code ======================//

class ImGuiGlWindow
{
public:
    /* Rendering function. This is called during rendering and allows arbitrary
       OpenGL draw calls. A scissor test corresponding to the window rectangle
       is setup before this is invoked. */
    virtual void render(ImDrawList const *, ImDrawCmd const *) = 0;

    /* Add the window to the draw list. To be used after ImGui::Begin(). */
    virtual void AddWindow();

    /* Get or set the padding around the OpenGL view in pixels. */
    ImVec4 padding() const { return m_padding; }
    void setPadding(ImVec4 padding);

    /* Accessors for whether the window is hovered or active. Available after
       the AddWindow() call. Uses the invisible button trick internally. */
    bool isHovered() const { return m_hovered; }
    bool isActive() const { return m_active; }

    /* Accessors for location and dimensions; updated by AddWindow(). */
    int x() const { return m_screenPos.x; }
    int y() const { return m_screenPos.y; }
    int width() const { return m_size.x - m_padding.x - m_padding.z; }
    int height() const { return m_size.y - m_padding.y - m_padding.w; }

    /* Check if a cursor position lands within the window. */
    bool inWindow(ImVec2 position);

    /* Coordinate change matrix from Dear ImGui window coordinates (0,0 being
       top left) to OpenGL coordinates. */
    glm::mat3 windowToOpenGLMatrix() const;

private:
    static void DrawCallback(ImDrawList const *, ImDrawCmd const *);

    bool m_hovered = false;
    bool m_active = false;

    ImVec2 m_screenPos {};
    ImVec2 m_size {};

    ImVec4 m_padding {};
};

//=== Hex viewer widget for memory ===========================================//

namespace ImGui {

struct HexViewer {
    //=== Data input ===//

    /* Number of bits in address values. Should be a multiple of 4. */
    int AddressBits;
    /* Read function */
    bool (*ReadByte)(u64 address, u8 *value);

    //=== Layout parameters ===//

    /* Force layout to display a power-of-two number of bytes per line. */
    bool PowerOfTwoLayout = true;
    /* Spacing (pixels) between address, hex, and ASCII columns. */
    int MajorSpacing = 10;
    /* Spacing (pixels) between bytes in hex column. */
    int MinorSpacing = 2;

    //=== Variable data ===//

    u64 Cursor;
    int BytesPerLine = 1;
    int VisibleLines = 1;
};

void AddHexViewer(HexViewer &HV);

} /* namespace ImGui */

#endif /* MQ_UI_IMGUI_UTIL_H */
