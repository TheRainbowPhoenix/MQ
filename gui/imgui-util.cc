#include "imgui-util.h"
#include <stdio.h>

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

} /* namespace ImGui */

void ImGui_LoadMQStyle(ImGuiStyle &st)
{
    st.WindowTitleAlign = ImVec2(0.5, 0.5);
    st.FramePadding = ImVec2(8, 4);
    st.ItemSpacing = ImVec2(10, 5);
    st.ItemInnerSpacing = ImVec2(6, 5);
    st.ScrollbarSize = 12;
    st.GrabMinSize = 8;
    st.FrameBorderSize = 1;
    st.TabBorderSize = 1;
    st.FrameRounding = 2;
    st.GrabRounding = 2;
    st.TabRounding = 2;

    st.Colors[ImGuiCol_WindowBg]           = {.185, .185, .2,  1  };
    st.Colors[ImGuiCol_FrameBg]            = {.25,  .275, .3,  1  };
    st.Colors[ImGuiCol_FrameBgHovered]     = {.7,   .81,  1,   .2 };
    st.Colors[ImGuiCol_FrameBgActive]      = {.70,  .81,  1,   .36};
    st.Colors[ImGuiCol_TitleBg]            = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_TitleBgActive]      = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_CheckMark]          = {.92,  .95,  .98, 1  };
    st.Colors[ImGuiCol_TabUnfocused]       = {.165, .165, .18, .97};
    st.Colors[ImGuiCol_TabUnfocusedActive] = {.17,  .17,  .19, 1  };

    st.Colors[ImGuiCol_Border].w = 0.125;
    // st.Colors[ImGuiCol_ScrollbarBg].w = 0.25;
}

//=== Dockable windows rendered with custom OpenGL code ======================//

void ImGuiGlWindow::DrawCallback(ImDrawList const *DL, ImDrawCmd const *DC)
{
    ImGuiGlWindow *win = static_cast<ImGuiGlWindow *>(DC->UserCallbackData);

    ImGuiIO& io = ImGui::GetIO();
    int displayWidth = io.DisplaySize.x;
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

    ImGuiIO& io = ImGui::GetIO();
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
