#include "imgui-util.h"
#include <algorithm>
#include <stdio.h>
#include <ctype.h>

namespace ImGui {

void TextLR(char const *left, char const *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *str;
    vasprintf(&str, fmt, args);
    va_end(args);

    char const *right = str ? str : "(nil)";

    ImGui::TextUnformatted(left);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x -
        ImGui::CalcTextSize(right).x);
    ImGui::TextUnformatted(right);

    free(str);
}

void SeparatorTextD(char const *str)
{
    ImGuiStyle const &style = ImGui::GetStyle();
    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    ImGui::SeparatorText(str);
    ImGui::PopStyleColor();
}

} /* namespace ImGui */

void ImGui_LoadMQStyle(ImGuiStyle &st)
{
    st.WindowTitleAlign = ImVec2(0.5, 0.5);
    st.FramePadding = ImVec2(8, 4);
    st.ItemSpacing = ImVec2(10, 5);
    st.ItemInnerSpacing = ImVec2(6, 5);
    st.ScrollbarSize = 10;
    st.GrabMinSize = 8;
    st.FrameBorderSize = 1;
    st.TabBorderSize = 1;
    st.FrameRounding = 2;
    st.GrabRounding = 2;
    st.TabRounding = 2;
    st.TabBarOverlineSize = 0;
    st.SeparatorTextBorderSize = 1;
    st.SeparatorTextPadding = {0,0};

    st.Colors[ImGuiCol_WindowBg]           = {.185, .185, .2,  1  };
    st.Colors[ImGuiCol_FrameBg]            = {.25,  .275, .3,  1  };
    st.Colors[ImGuiCol_FrameBgHovered]     = {.7,   .81,  1,   .2 };
    st.Colors[ImGuiCol_FrameBgActive]      = {.70,  .81,  1,   .36};
    st.Colors[ImGuiCol_TitleBg]            = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_TitleBgActive]      = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_TitleBgCollapsed]   = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_CheckMark]          = {.92,  .95,  .98, 1  };
    st.Colors[ImGuiCol_TabUnfocused]       = {.165, .165, .18, .97};
    st.Colors[ImGuiCol_TabUnfocusedActive] = {.17,  .17,  .19, 1  };
    st.Colors[ImGuiCol_DockingPreview]     = {.25,  .375, .5,  .5 };

    st.Colors[ImGuiCol_Border].w = 0.25;
    st.Colors[ImGuiCol_ScrollbarBg].w = 0.00;
}

//============================================================================//

void ImGuiGlWindow::setPadding(ImVec4 padding)
{
    m_padding = padding;
}

void ImGuiGlWindow::DrawCallback(ImDrawList const *DL, ImDrawCmd const *DC)
{
    ImGuiGlWindow *win = static_cast<ImGuiGlWindow *>(DC->UserCallbackData);

    ImGuiIO& io = ImGui::GetIO();
    int displayHeight = io.DisplaySize.y;

    glEnable(GL_SCISSOR_TEST);
    glScissor(win->x(), displayHeight - win->height() - win->y(), win->width(),
              win->height());

    win->render(DL, DC);

    glDisable(GL_SCISSOR_TEST);
}

void ImGuiGlWindow::AddWindow()
{
    /* InvisibleButton() trick from the custom rendering demo */
    m_screenPos = ImGui::GetCursorScreenPos();
    m_size = ImGui::GetContentRegionAvail();

    /* Skip the first frames in which the size might be zero */
    if(m_size.x < 1 || m_size.y < 1)
        return;

    // FIXME: ImGuiGlWindow: Need unique identifier for the invisible button.
    ImGui::InvisibleButton("ImGuiGlWindowInvisibleButton", m_size,
        ImGuiButtonFlags_MouseButtonLeft |
        ImGuiButtonFlags_MouseButtonRight);
    m_hovered = ImGui::IsItemHovered();
    m_active = ImGui::IsItemActive();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddCallback(ImGuiGlWindow::DrawCallback, this);
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

bool ImGuiGlWindow::inWindow(ImVec2 pos)
{
    return pos.x >= x() && pos.x < x() + width() &&
           pos.y >= y() && pos.y < y() + height();
}

glm::mat3 ImGuiGlWindow::windowToOpenGLMatrix() const
{
    ImGuiIO& io = ImGui::GetIO();
    int w = io.DisplaySize.x;
    int h = io.DisplaySize.y;

    glm::mat3 tr_local2global(
         1.0f,       0.0f,      0.0f,
         0.0f,       1.0f,      0.0f,
         x(),        y(),       1.0f);

    glm::mat3 tr_global2gl(
         2.0f / w,   0.0f,      0.0f,
         0.0f,      -2.0f / h,  0.0f,
        -1.0f,       1.0f,      1.0f);

    return tr_global2gl * tr_local2global;
}

//============================================================================//

namespace ImGui {

/* A reasonably good estimator of text size based on glyph count. Even for mono
   fonts, using the size of a single character appears to lead to significant
   deviations. So we use the size of up to 16. */
static int WidthForGlyphs(uint n)
{
    char const *str = "WWWWWWWWWWWWWWWW";
    int w = ImGui::CalcTextSize(str, str + (n % 16)).x;
    return (n <= 16) ? w : w + (n / 16) * ImGui::CalcTextSize(str).x;
}

static int PreviousPowerOfTwo(uint n)
{
    uint m;
    while((m = (n & (n - 1))))
        n = m;
    return n;
}

static u64 ClampAddress(HexViewer &HV, u64 addr)
{
    if(HV.AddressBits < 64)
        addr &= (1ull << HV.AddressBits) - 1;
    return addr;
}

static void RenderHexViewer(HexViewer &HV)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    char str[32];

    ImVec2 TL = ImGui::GetCursorScreenPos();
    ImVec2 R = ImGui::GetContentRegionAvail();
    ImVec2 BR(TL.x + R.x, TL.y + R.y);

    // drawList->AddRectFilled(TL, BR, 0xff000000);

    int lineHeight = ImGui::GetTextLineHeight();
    int lines = R.y / lineHeight;

    int addressPixels = WidthForGlyphs(HV.AddressBits / 4 + 1);

    /* In each line, we fit address bits, major spacing, byte bits, major
       spacing, and ASCII. Each byte uses two hex glyphs, one minor spacing,
       and one ASCII glyph. The extra minor spacing is to compensate minor
       spacing being counted one too many times. */
    int spaceForBytes =
        R.x - addressPixels - 2 * HV.MajorSpacing + HV.MinorSpacing;
    int bytePixels = WidthForGlyphs(2) + WidthForGlyphs(1) + HV.MinorSpacing;
    HV.BytesPerLine = spaceForBytes / bytePixels;

    if(HV.PowerOfTwoLayout)
        HV.BytesPerLine = PreviousPowerOfTwo(HV.BytesPerLine);

    /* Center the view if we don't use all the space. */
    if(HV.AlignXCenter) {
        int pixelsLeft = spaceForBytes - HV.BytesPerLine * bytePixels;
        TL.x += pixelsLeft / 2;
        R.x -= pixelsLeft;
    }

    u64 addr = HV.Cursor;
    u8 byte;

    ImU32 fg = ImGui::ColorConvertFloat4ToU32(
        ImGui::GetStyle().Colors[ImGuiCol_Text]);

    for(int i = 0; i < lines; i++) {
        ImVec2 p(TL.x, TL.y + i * lineHeight), q = p;

        sprintf(str, "%0*lx:", std::min(HV.AddressBits / 4, 16), addr);
        drawList->AddText(p, fg, str);

        p.x += addressPixels + HV.MajorSpacing;
        q.x = p.x + HV.BytesPerLine * (WidthForGlyphs(2) + HV.MinorSpacing)
              + HV.MajorSpacing - HV.MinorSpacing;

        for(int j = 0; j < HV.BytesPerLine; j++) {
            bool ok = HV.ReadByte && HV.ReadByte(addr, &byte);

            if(ok) {
                sprintf(str, "%02x", byte);
                drawList->AddText(p, fg, str);
                str[0] = isprint(byte) ? byte : '.';
                str[1] = 0;
                drawList->AddText(q, fg, str);
            }
            else {
                drawList->AddText(p, fg, "##");
                drawList->AddText(q, fg, ".");
            }

            p.x += WidthForGlyphs(2) + HV.MinorSpacing;
            q.x += WidthForGlyphs(1);
            addr = ClampAddress(HV, addr + 1);
        }
    }

    HV.VisibleLines = lines;
    ImGui::SetCursorScreenPos(BR);
}

void AddHexViewer(HexViewer &HV)
{
    ImGui::BeginGroup();
    RenderHexViewer(HV);
    ImGui::EndGroup();

    float wheel = ImGui::GetIO().MouseWheel;
    if(ImGui::IsItemHovered() && wheel != 0) {
        int dy = (wheel < 0) ? -1 : +1;
        u64 newCursor = HV.Cursor - dy * HV.BytesPerLine * HV.VisibleLines;
        HV.Cursor = ClampAddress(HV, newCursor);
    }
}

} /* namespace ImGui */
