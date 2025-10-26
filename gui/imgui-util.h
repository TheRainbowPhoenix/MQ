// Dear ImGui utilities

#ifndef MQ_UI_IMGUI_UTIL_H
#define MQ_UI_IMGUI_UTIL_H

#include <mq/defs.h>
#include <azur/gl/gl.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <pthread.h>

/* Number of frames we expect Dear ImGui to need to settle its layout after
   starting for the first time, resizing windows, etc. */
#define IMGUI_SETTLING_FRAMES 5

// TODO: Proper namespacing of fonts
extern ImFont *fontSans;
extern ImFont *fontMono;
extern ImFont *fontBold;

namespace ImGui {

/* A full-line label with a fixed string on the left and a formatted string
   right-aligned on the right side of the current drawing region. */
void TextLR(char const *left, char const *fmt, ...);

/* A full-line centered text with color and bold support 
   (todo) maybe too specific? */
void TextCenteredColor(char const *text, u32 color);

/* Separator text, but disabled. */
void SeparatorTextD(char const *str);

/* Icon button with a tooltip text. */
bool IconButton(int iconID, char const *tooltip, bool disabled=false);

/* button with custom width and enabled/disabled status */
bool ButtonWSized(char const *name, float width, bool disabled);

/* Move cursor by a given amount. */
static inline void MoveCursorScreenPos(ImVec2 diff) {
   ImVec2 pos = ImGui::GetCursorScreenPos();
   ImGui::SetCursorScreenPos(ImVec2(pos.x + diff.x, pos.y + diff.y));
}

/* Text but with the mono font. Note: GCC will not check format strings on
   variadic templates, only variadic functions, so use C-style varargs. */
__attribute__((format(printf, 1, 2)))
void TextMono(char const *fmt, ...);
/* Text but with a red error color. */
__attribute__((format(printf, 1, 2)))
void TextError(char const *fmt, ...);
/* Text vut with the mono font and a red error color. */
__attribute__((format(printf, 1, 2)))
void TextErrorMono(char const *fmt, ...);

/* Checkbox with smaller frame padding. */
template<typename... Args>
bool Checkbox2(Args... args) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
    bool b = ImGui::Checkbox(std::forward<Args>(args)...);
    ImGui::PopStyleVar();
    return b;
}

/* Help mark tooltips */
void HelpMarker(const char *title, const char* desc);

/* anonym combo */
void ComboAnon(int id,
    int *index,
    std::vector<std::string> const &selector,
    bool disabled
);

} /* namespace ImGui */

void ImGui_LoadMQStyle(ImGuiStyle &style);

//=== Customized main menu bar that accepts other widgets ====================//

namespace ImGui {

/* Custom main menu bar. This menu bar has a specific layout with menus on the
   left and a child window on the right for additional buttons/widgets/etc.
   It's a bit hacky style-wise hence this abstraction.

   In order to pop style variables, EndCustomMenuBar() must be called *even* if
   BeginCustomMenuBar() returns false. The function has internal state to
   remember whether to EndMainMenuBar() or not. So you can't nest this (makes
   no sense anyway as this is the main menu). */
bool BeginCustomMenuBar();
void EndCustomMenuBar();

/* Create a top-level menu entry in the custom main menu bar. This is only for
   top-level menus; like for the bar itself, EndCustomMenu() must be called
   *even* if BeginCustomMenu() returns false, and it can't be nested. For
   sub-menus, use BeginMenu() as usual. */
bool BeginCustomMenu(char const *label);
void EndCustomMenu();

/* Start the child window where we can put more items in the menu bar. Like for
   the bar, EndCustomMenuChild() mut be called *even* if BeginCustomMenuChild()
   returns false. */
bool BeginCustomMenuChild(char const *label, ImVec2 size, ImVec2 padding);
void EndCustomMenuChild();

/* Separator within the child window. */
void CustomMenuSeparator();

} /* namespace ImGui */


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
    //=== Data input =========================================================//

    /* Number of bits in address values. Should be a multiple of 4. */
    int AddressBits;
    /* Minimum and maximum address that we can access; both are included so
       that MaxAddress can be specified without risking an overflow. */
    u64 MinAddress = 0;
    u64 MaxAddress = -1;

    /* Input type */
    enum { InputFunction, InputBuffer };
    int InputType;

    /* [Function]: Read function */
    bool (*ReadByte)(u64 address, u8 *value, void *userdata) = nullptr;
    void *ReadByteUserdata = nullptr;

    /* [Buffer]: Input buffer and its size */
    void *BufferPointer = nullptr;
    int BufferSize = 0;
    /* [Buffer]: Base address of the buffer in the address space */
    u64 BufferBaseAddress = 0;

    //=== Layout parameters ==================================================//

    /* Force layout to display a power-of-two number of bytes per line. This
       also forces lines to start on aligned address. */
    bool PowerOfTwoLayout = true;
    /* Extra spacing (pixels) in-between lines. */
    int LineSpacing = 0;
    /* Spacing (pixels) between address, hex, and ASCII columns. */
    int MajorSpacing = 8;
    /* Spacing (pixels) between bytes in hex column. */
    int MinorSpacing = 2;
    /* Extra spacing (pixels) between groups in hex columns, and group size. */
    int GroupSize = 4;
    int GroupSpacingBytes = 4;
    int GroupSpacingAscii = 4;
    /* Center-align horizontally */
    bool AlignXCenter = true;

    //=== Variable data: layout output =======================================//

    int BytesPerLine = 1;
    int VisibleLines = 1;

    void ComputeLayout(int AvailableWidth, int AvailableHeight);

    /* Layout x offsets for the address, bytes and ASCII columns. */
    int LayoutXAddress = 0;
    int LayoutXBytes = 0;
    int LayoutXAscii = 0;
    /* Line height, spacing included. */
    int LayoutLineHeight = 0;

    /* Get the layout X offset for a given byte's hex code and ASCII. */
    int LayoutXByteAt(int Column) const;
    int LayoutXAsciiAt(int Column) const;

    //=== Variable data: viewing state =======================================//

    u64 Cursor;

    /* Minimum and maximum values that the cursor can have. In power-of-two
       layout, the cursor is always line-aligned; if you have 16 bytes per line
       and the min address is 0x28 the minimum cursor will be 0x20 and the
       first byte will be displayed on the middle of the first line. */
    u64 MinCursor() const;
    u64 MaxCursor() const;
    /* Adjust the given cursor to an acceptable value after adding the given
       increment. The addition handles overflow. The adjustment handles both
       alignment (in power-of-two layout) and clamping to min/max values. */
    u64 AdjustCursor(u64 Cursor, i64 Increment=0) const;
};

void AddHexViewer(HexViewer &HV);

} /* namespace ImGui */

//=== Rich text data structure for the console ===============================//

namespace RichText {

/* View guiding how to layout the text. To be independent from Dear Imgui, one
   could abstract the computational part of this (e.g. max width + width of
   text primitive) and leave the ImGui details to the renderer. I've gonna
   leave this here for now. */
struct View {
    //=== User settings ===//

    /* Rendering font. */
    ImFont *font = NULL;
    /* Scrolling position in lines. TODO: Scroll in pixels? */
    ImS64 scroll = 0;

    //=== Values set by the rendering function ===//

    /* Dimensions of the rendering area; only mono fonts for now, so grid. */
    uint columns = 0, rows = 0;

    /* Check that two views are such that layouts computed for one can be
       reused for another. In essence, this reveals what data is essential and
       what data is just graphical options or current widget state. */
    bool isEquivalentTo(View const &other) const {
        return this->columns == other.columns;
    }
};

struct Format {
    u16 position, length;
    /* TODO: More format information */
    u32 color;
};

struct Line {
    /* Format information, range-based */
    // FIXME: Get leaked when destroying the object...
    Format *formats;
    u16 formatCount;
    /* Number of non-NUL bytes */
    u16 size;
    /* Number of render lines occupied */
    u16 renderLines;
    /* Raw NUL-terminated data */
    char data[];

    /* Update the number of render lines. */
    void updateRenderLines(View const &view);

    static Line *make(char const *str, int size=-1);
};

struct Buffer {
    /* A rotating array of `capacity` lines starting at position `start` and
       holding `size` lines. The array is pre-allocated. */
    Line **lines;

    /* Invariants:
       - capacity > 0
       - 0 <= size <= capacity
       - 0 <= start < capacity
       - When size is 0, start is undefined. */
    u16 capacity, start, size;

    /* Total number of rendered lines for the buffer. */
    u16 totalRendered;

    /* To keep track of lines' identity, the rotating array includes an extra
       numbering system. Each line is assigned an *absolute* line number which
       starts at 1 and increases every time a line is added. That number is
       independent of rotation.

       Absolute line number of the next line to be removed. This identifies
       the `start` line, unless the buffer is empty. Regardless, the interval
       [buf->absolute .. buf->absolute + buf->size) always covers exactly the
       set of lines that are held in the buffer. */
    int absolute;

    /* To avoid memory explosion, the rotating array can be set to clean up old
       lines when the total memory consumption is too high. `backlog_size`
       specifies how many bytes of text lines are allowed to hold. */
    int backlogSize;
    /* Total size of current lines, in bytes. */
    int totalSize;

    /* Last absolute line that has been laid out for rendering. Lazy layout
       would start at `absolute_rendered+1`. */
    int absoluteRendered;

    /* Initial state has zero capacity. */
    Buffer();
    ~Buffer();

    /* Initialize by allocating `line_count` lines. The buffer will allow up to
       `backlog_size` bytes of text data and clean up lines past that limit.
       This function does not free pre-existing data in `buf`. */
    bool alloc(int capacity, int backlogSize);

    /* Remove all the lines while keeping resources allocated. */
    void clear();
    /* Free resources and reset the state. */
    void reset();

    /* Absolute line numbers of the "start" and "end" of the buffer. The set of
       lines in the buffer is always [start ... end). If the buffer is empty,
       the interval is empty and neither line number is in the buffer. */
    int absoluteStart() const { return this->absolute; }
    int absoluteEnd() const { return this->absolute + this->size; }

    /* Get a pointer to the line with the given absolute number. */
    Line *getLine(int absoluteNumber) const;

    /* Add a new line to the buffer (recycling an old one if needed). Takes
       ownership of the added line. */
    void addLine(Line *line);

    /* Recycle the `n` oldest lines from the buffer. */
    void recycleOldestLines(int n);

    /* Clean up lines to try and keep the memory footprint of the text under
       `backlog_size` bytes. Always keeps at least the last line. */
    void cleanBacklog();

    /* Update the render width computation for all lines in the buffer. If
       `lazy` is false, all lines are re-laid out. But render width often
       remains the same for many frames, and lines only get added. In this
       case, `lazy` can be set to true, and only lines added or edited since
       the previous render will be re-laid out. */
    void updateRender(View const &view, bool lazy);

private:
    /* Lines in the buffer are identified by their positions within the `lines`
       array, which are integers equipped with modulo arithmetic (we call them
       "indices"). We abstract away the rotation by numbering stored lines from
       0 to buf->size - 1, and we call these numbers "nths". When we want to
       identify lines independent of rotation, we use their absolute line number.

       Such numbers refer to lines stored in the buffer if:
         (index)   0 <= index < size
         (nth)     0 <= nth < size
         (abs)     0 <= abs - buf->absolute < size */

    int nthToIndex(int nth) const;

    /* Get the nth line. */
    Line *getNthLine(int nth) const;

    /* Move `index` by `diff`; assumes |diff| <= this->capacity. */
    int indexAdd(int index, int diff) const;
};

/* Scroll position measured as a number of lines up from the bottom. */
using ScrollPos = int;

struct Text {
    /* A rotating array of lines. Never empty. */
    Buffer lines;
    /* Whether new data has been added and a frame should be rendered. */
    bool renderNeeded;
    /* View parameters from last computeView(). */
    View view;
    /* Total number of render lines (this is view data). */
    i16 renderLines;

    /* Initiale state has zero capacity. */
    Text();
    ~Text();

    /* Multi-threaded locks */
    mutable pthread_mutex_t mutex;
    void lock() const;
    void unlock() const;

    /* Initialize with specified storage limits. */
    // TODO: In principle, could be called multiple times. Not coded yet.
    bool alloc(int backlogSize, int maximumLineCount);

    /* Add a new line. */
    void addLine(Line *line);

    /* Compute a view of the console for rendering and scrolling. */
    void computeView(View const &view);

    /* Clear the text storage. */
    void clear();

    // TODO: Text selection & editing features

private:
    /* Clamp a scrolling position to the range valid of the last computed view. */
    ScrollPos clampScrollPos( ScrollPos pos);
};

} /* namespace RichText */

namespace ImGui {

void AddRichTextFrame(RichText::Text &RT, RichText::View &view);

} /* namespace ImGui */

#endif /* MQ_UI_IMGUI_UTIL_H */
