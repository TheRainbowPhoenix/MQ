#include "imgui-util.h"
#include <imgui_internal.h>
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
    st.Colors[ImGuiCol_FrameBg]            = {.22,  .22,  .24, 1  };
    st.Colors[ImGuiCol_FrameBgHovered]     = {.7,   .81,  1,   .2 };
    st.Colors[ImGuiCol_FrameBgActive]      = {.70,  .81,  1,   .36};
    st.Colors[ImGuiCol_TitleBg]            = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_TitleBgActive]      = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_TitleBgCollapsed]   = {.14,  .14,  .14, 1  };
    st.Colors[ImGuiCol_Header]             = {.26,  .59,  .98, .28};
    st.Colors[ImGuiCol_HeaderHovered]      = {.26,  .59,  .98, .47};
    st.Colors[ImGuiCol_HeaderActive]       = {.26,  .59,  .98, .59};
    st.Colors[ImGuiCol_CheckMark]          = {.92,  .95,  .98, 1  };
    st.Colors[ImGuiCol_TabUnfocused]       = {.165, .165, .18, .97};
    st.Colors[ImGuiCol_TabUnfocusedActive] = {.17,  .17,  .19, 1  };
    st.Colors[ImGuiCol_DockingPreview]     = {.25,  .375, .5,  .5 };
    st.Colors[ImGuiCol_TabDimmedSelected]  = st.Colors[ImGuiCol_WindowBg];
    st.Colors[ImGuiCol_Tab]                = st.Colors[ImGuiCol_TabDimmed];

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

/* Same in reverse */
static int GlyphsInWidth(int pixels)
{
    char const *str = "WWWWWWWWWWWWWWWW";
    int w = ImGui::CalcTextSize(str, str + 16).x;
    return (16 * pixels / w);
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

//============================================================================//

namespace RichText {

Line *Line::make(char const *str, int size)
{
    if(size < 0)
        size = strlen(str);

    Line *l = (Line *)malloc(sizeof(Line) + size + 1);
    l->formats = NULL;
    l->formatCount = 0;
    l->size = strlen(str);
    l->renderLines = 0;

    memcpy(l->data, str, size);
    l->data[size] = 0;
    return l;
}

void Line::updateRenderLines(View const &view)
{
    // TODO: Currently assumes one column per byte, which is... not great!
    this->renderLines = (this->size + view.columns - 1) / view.columns;
    if(this->renderLines <= 0)
        this->renderLines = 1;
}

Buffer::Buffer()
{
    this->lines = nullptr;
    this->capacity = 0;
    this->start = 0;
    this->size = 0;
    this->totalRendered = 0;
    this->absolute = 0;
    this->backlogSize = 0;
    this->totalSize = 0;
    this->absoluteRendered = 0;
}

bool Buffer::alloc(int capacity, int backlogSize)
{
    if(capacity <= 0)
        return false;

    this->lines = new Line *[capacity];
    if(!this->lines)
        return false;
    memset(this->lines, 0, capacity * sizeof(Line *));

    this->capacity = capacity;
    this->start = 0;
    this->size = 0;
    this->totalRendered = 0;
    this->absolute = 1;
    this->backlogSize = backlogSize;
    this->totalSize = 0;
    this->absoluteRendered = 0;
    return true;
}

Buffer::~Buffer()
{
    reset();
}

void Buffer::reset()
{
    for(int i = 0; i < this->capacity; i++)
        free(this->lines[i]);
    delete[] this->lines;
    memset(this, 0, sizeof *this);
}

Line *Buffer::getNthLine(int nth) const
{
    if((uint)nth >= this->size)
        return NULL;
    return this->lines[nthToIndex(nth)];
}

int Buffer::indexAdd(int index, int diff) const
{
    return (index + diff + this->capacity) % this->capacity;
}

int Buffer::nthToIndex(int nth) const
{
    return (this->start + nth) % this->capacity;
}

Line *Buffer::getLine(int abs) const
{
    return getNthLine(abs - this->absolute);
}

void Buffer::addLine(Line *line)
{
    /* Make space if the buffer is full */
    recycleOldestLines(this->size - this->capacity + 1);

    this->size++;
    int last_nth = nthToIndex(this->size - 1);

    this->lines[last_nth] = line;
}

void Buffer::recycleOldestLines(int count)
{
    count = std::min(count, (int)this->size);
    if(count <= 0)
        return;

    for(int nth = 0; nth < count; nth++) {
        Line *L = getNthLine(nth);
        this->totalRendered -= L->renderLines;
        this->totalSize -= L->size;
        this->lines[nthToIndex(nth)] = NULL;
        free(L);
    }

    this->start = indexAdd(this->start, count);
    this->size -= count;
    this->absolute += count;
}

void Buffer::cleanBacklog()
{
    if(this->size <= 0)
        return;

    int remove = 0;
    int n = this->totalSize;

    while(remove < this->size - 1 && n > this->backlogSize) {
        n -= getNthLine(remove)->size;
        remove++;
    }

    recycleOldestLines(remove);
}

void Buffer::updateRender(View const &view, bool lazy)
{
    int start = absoluteStart();
    int end = absoluteEnd();
    if(lazy)
        start = std::max(start, this->absoluteRendered + 1);

    for(int abs = start; abs < end; abs++) {
        Line *L = getNthLine(abs - this->absolute);
        this->totalRendered -= L->renderLines;
        L->updateRenderLines(view);
        this->totalRendered += L->renderLines;
    }

    // FIXME: Why -2?
    this->absoluteRendered = std::max(this->absoluteRendered, end - 2);
}

Text::Text(): lines {}
{
    this->renderNeeded = false;
    this->renderWidth = 0;
    this->renderLines = 0;
}

bool Text::alloc(int backlogSize, int maximumLineCount)
{
    backlogSize = std::max(backlogSize, 1);
    maximumLineCount = std::max(maximumLineCount, 1);

    if(!lines.alloc(maximumLineCount, backlogSize))
        return false;

    this->renderNeeded = true;
    this->view = View {};
    this->renderWidth = 0;
    this->renderLines = 0;

    // FIXME: Compared to original, no newline initially.
    return true;
}

void Text::addLine(Line *line)
{
    if(!line)
        return;

    this->lines.addLine(line);
    this->renderNeeded = true;

    /* This is a good time to clean up the backlog. */
    // TODO: This might actually be a performance bottleneck, because we do a
    // long loop only to find out that the total size is still reasonable!
    lines.cleanBacklog();
}

void Text::computeView(View const &view)
{
    /* If a view with the same width was previously computed, do a lazy update:
       recompute only the last lines. */
    bool lazy = view.isEquivalentTo(this->view);
    this->view = view;
    this->renderWidth = view.columns;
    this->renderLines = view.rows;

    this->lines.updateRender(view, lazy);
}

ScrollPos Text::clampScrollPos(ScrollPos pos)
{
    /* No scrolling case */
    if(this->lines.totalRendered < this->renderLines)
        return 0;

    return std::max(0, std::min(pos,
        this->lines.totalRendered - this->renderLines));
}

} /* namespace RichText */

namespace ImGui {

static float RenderLine(float x, float y, RichText::Line *L,
    RichText::View const &view, float dy, int show_from, int show_until)
{
    char const *p = L->data;
    char const *endline = p + L->size;
    int line_offset = 0;
    int line_number = 0;

    while(p < endline) {
        char const *endscreen = p + std::min(view.columns, (uint)strlen(p));
        // char const *endscreen = view.font->CalcWordWrapPositionA(1.0f,
        // textStart, textEnd, widthRemaining);
        int len = endscreen - p;

        if(line_number >= show_from && line_number < show_until) {
            ImGui::SetCursorScreenPos({x, y});
            ImGui::TextUnformatted(p, p + len);
            y += dy;
        }

        p += len;
        line_offset += len;
        line_number++;
    }

    // printf("[%s] %d/%d render lines\n", L->data, L->renderLines, line_number);

    return y;
}

void AddRichTextFrame(RichText::Text &RT, RichText::View &view)
{
    ImGui::PushFont(fontMono);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,
        ImGui::GetStyle().Colors[ImGuiCol_TitleBg]);

    // ImGui::Text("scroll:%lld pos:%d totalLines:%d", scroll, pos, total_lines);
    // ImGui::Text("cols:%u rows:%u", view.columns, view.rows);

    //---

    ImGui::BeginChild("##console", {}, ImGuiChildFlags_FrameStyle);

    /* Figure out the geometry of the frame */
    ImVec2 frameSize = ImGui::GetContentRegionAvail();
    /* Subtract scrollbar width */
    float scrollbarWidth = ImGui::GetStyle().ScrollbarSize;
    frameSize.x -= scrollbarWidth;

    view.columns = (uint)GlyphsInWidth(frameSize.x);
    view.rows = (uint)(frameSize.y / ImGui::GetTextLineHeight());
    RT.computeView(view);

    int totalLines = RT.lines.totalRendered;
    int visibleLines = RT.renderLines;

    RichText::ScrollPos pos =
        std::max((ImS64)0, totalLines - visibleLines - view.scroll);

    float x = ImGui::GetCursorScreenPos().x;
    float y0 = ImGui::GetCursorScreenPos().y;
    float y = y0;
    float dy = ImGui::GetTextLineHeight();

    /* Show only visible lines. We want to avoid counting all the lines in the
       console, and instead start from the end. */
    int line_y = visibleLines + pos;
    int L_start = RT.lines.absoluteStart();
    int L_end = RT.lines.absoluteEnd();
    int i = L_end;

    while(i > L_start && line_y > 0)
        line_y -= RT.lines.getLine(--i)->renderLines;

    /* If there isn't enough content to fill the view, start at the top. */
    line_y = std::min(line_y, pos);

    while(i < L_end && line_y < visibleLines) {
        RichText::Line *L = RT.lines.getLine(i);

        // TODO: Handle formats etc.
        y = RenderLine(x, y, L, view, dy, -line_y, visibleLines - line_y);
        line_y += L->renderLines;
        i++;
    }

    /* Manual scrollbar, based on
       https://github.com/ocornut/imgui/issues/8215#issuecomment-2527561330 */

    if(totalLines > visibleLines) {
        ImGuiWindow *win = ImGui::GetCurrentWindow();
        ImS64 scroll_max = totalLines;
        ImS64 scroll_visible_size = visibleLines;

        // Hack to use GetWindowScrollbarRect()
        win->ScrollbarSizes.x = win->ScrollbarSizes.y = scrollbarWidth;

        ImRect scrollbarRect = ImGui::GetWindowScrollbarRect(win, ImGuiAxis_Y);
        ImGuiID scrollbarID = ImGui::GetWindowScrollbarID(win, ImGuiAxis_Y);
        ImGui::ScrollbarEx(scrollbarRect, scrollbarID, ImGuiAxis_Y,
            &view.scroll, scroll_visible_size, scroll_max, ImDrawFlags_None);

        win->ScrollbarSizes.x = win->ScrollbarSizes.y = 0.0f;
    }

    //---

    ImGui::EndChild();

    float wheel = ImGui::GetIO().MouseWheel;
    if(ImGui::IsItemHovered() && wheel != 0) {
        int dy = (wheel < 0) ? -3 : +3;
        view.scroll = std::max(0, std::min(totalLines - visibleLines,
            (int)view.scroll - dy));
    }

    ImGui::PopStyleColor();
    ImGui::PopFont();
}

} /* namespace ImGui */
