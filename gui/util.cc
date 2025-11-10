#include <azur/config.h>
#include <azur/log.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

void *openAndReadFile(char const *path, long *size_ptr)
{
    FILE *fp;
    void *data = NULL;
    long size;

    *size_ptr = 0;

    fp = fopen(path, "r");
    if(!fp) goto err;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Allocate a non-NULL pointer even if file is empty */
    data = malloc(size + (size == 0));
    if(!data) goto err;

    if(fread(data, size, 1, fp) != 1) goto err;
    fclose(fp);

    *size_ptr = size;
    return data;

err:
    azlog(ERROR, "openAndReadFile: cannot open '%s': %s\n",
        path, strerror(errno));
    if(fp)
        fclose(fp);
    if(data)
        free(data);
    return NULL;
}

#if AZUR_PLATFORM_EMSCRIPTEN

#include "../3rdparty/emscripten-browser-file/emscripten_browser_file.h"

void handle_upload_file(
    std::string const &filename, std::string const &mime_type,
    std::string_view buffer, void *ofb0)
{
    OpenFileBuffer *ofb = static_cast<OpenFileBuffer *>(ofb0);
    (void)mime_type;

    /* The buffer will expire when the callback returns, so copy it. */
    size_t size = buffer.size();
    void *data = malloc(size);
    if(!data) {
        azlog(ERROR, "Open '%s' failed!\n", filename.c_str());
        return;
    }

    memcpy(data, buffer.data(), size);
    ofb->path = filename;
    ofb->data = data;
    ofb->size = size;
    azlog(WARN, "Open '%s' (%zu bytes)!\n", filename.c_str(), size);
}

void openFileDialog(OpenFileBuffer *ofb)
{
    emscripten_browser_file::upload(".g1a,.g3a", handle_upload_file, ofb);
}

#else /* Not emscripten */

#include "../3rdparty/portable-file-dialogs/portable-file-dialogs.h"

void openFileDialog(OpenFileBuffer *ofb)
{
    auto paths = pfd::open_file("MQ: Open file", ".",
        {"Add-ins", "*.g1a *.g3a", "All files", "*"}, pfd::opt::none).result();
    if(!paths.size())
        return;

    fs::path path = paths[0];
    long size;
    void *data = openAndReadFile(path.c_str(), &size);
    if(!data)
        return;

    ofb->path = path;
    ofb->data = data;
    ofb->size = size;
}

#endif

std::string memorySizeString(uint size, bool shortSuffix)
{
    char *str = NULL;

    if(size == 0)
        asprintf(&str, "0");
    else if(((size >> 20) << 20) == size)
        asprintf(&str, "%u%s", size >> 20, shortSuffix ? "M" : " MiB");
    else if(((size >> 10) << 10) == size)
        asprintf(&str, "%u%s", size >> 10, shortSuffix ? "k" : " kiB");
    else
        asprintf(&str, "%u", size);

    std::string ret(str);
    free(str);
    return ret;
}

bool generateMonoFrame(mqDisplay *display)
{
    if(!mq_display_setFormat(display, MQ_DISPLAY_FORMAT_L8, 128, 64))
        return false;

    int r0 = rand() % 2 + 1;
    int r1 = rand() % 3 + 1;
    int r2 = rand() % 2;
    int r3 = rand() % 4 + 2;
    u8 palette[4] = { 0x00, 0x55, 0xaa, 0xff };
    for(uint y = 0; y < display->height; y++)
    for(uint x = 0; x < display->width; x++) {
        int c1 = x ^ y;
        int c2 = (x >> r3) ^ r0 * ((x - r2*y) >> 1);
        int c3 = (y >> 1) ^ ((x+y) >> r1);
        int c = c1 ^ c2 ^ c3;
        ((u8 *)display->data)[display->width * y + x] = palette[c & 3];
    }

    for(uint y = 0; y < display->height; y++) {
        ((u8 *)display->data)[display->width * y + 0] = 0xff;
        ((u8 *)display->data)[display->width * (y+1) - 1] = 0xff;
    }
    for(uint x = 0; x < display->width; x++) {
        ((u8 *)display->data)[display->width * 0 + x] = 0xff;
        ((u8 *)display->data)[display->width * (display->height-1) + x] = 0xff;
    }

    mq_display_setDirty(display, true);
    return true;
}

#define C_RGB(R, G, B) (((R) << 11) + ((G) << 5) + (B))

bool generateRGBFrame(mqDisplay *display)
{
    if(!mq_display_setFormat(display, MQ_DISPLAY_FORMAT_RGB565, 396, 224))
        return false;

    u16 palette[16];

    /* Generate a cool looking image pattern */
    for(int i = 0; i < 16; i++) {
        int top = rand() & 31;
        int bot = rand() & top;
        int which = rand() % 3;
        if(which == 0)
            palette[i] = C_RGB(top, bot, bot);
        else if(which == 1)
            palette[i] = C_RGB(bot, top, bot);
        else
            palette[i] = C_RGB(bot, bot, top);
    }

    for(uint y = 0; y < display->height; y++)
    for(uint x = 0; x < display->width; x++) {
        int c1 = x ^ y;
        int c2 = (x >> 5) ^ (x >> 1) ^ (x >> 6);
        int c3 = (y >> 1) ^ (y >> 4) ^ (y >> 5);
        int c = c1 ^ c2 ^ c3;
        ((u16 *)display->data)[display->width * y + x] = palette[c & 15];
    }

    for(uint y = 0; y < display->height; y++) {
        ((u16 *)display->data)[display->width * y + 0] = 0xffff;
        ((u16 *)display->data)[display->width * (y+1) - 1] = 0xffff;
    }
    for(uint x = 0; x < display->width; x++) {
        ((u16 *)display->data)[display->width * 0 + x] = 0xffff;
        ((u16 *)display->data)[display->width * (display->height-1) + x] = 0xffff;
    }

    mq_display_setDirty(display, true);
    return true;
}

std::string replaceSubstring(
   std::string const &text, std::string const &pat, std::string const &repl)
{
    std::string buf;
    size_t pos = 0;
    size_t prevPos;

    buf.reserve(text.size());
    while(true) {
        prevPos = pos;
        pos = text.find(pat, pos);
        if(pos == std::string::npos)
            break;
        buf.append(text, prevPos, pos - prevPos);
        buf += repl;
        pos += pat.size();
    }
    buf.append(text, prevPos, text.size() - prevPos);
    return buf;
}

std::string strftimeCurrentTime(char const *fmt)
{
    auto t = std::time(nullptr);
    auto tm = std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(tm, fmt);
    return oss.str();
}

/* Set the dirty flag only if needed to avoid resolving paths repeatedly. */
void OutputPathPattern::setPattern(std::string const &pattern)
{
    if(m_pattern != pattern) {
        m_pattern = pattern;
        m_dirty = true;
    }
}
void OutputPathPattern::setOverwrite(bool overwrite)
{
    m_dirty = (overwrite != m_overwrite);
    m_overwrite = overwrite;
}

bool OutputPathPattern::fileExists(std::string path)
{
#if AZUR_PLATFORM_EMSCRIPTEN
    /* The browser handles naming downloaded files. */
    (void)path;
    return false;
#else
    struct stat buffer;
    // TODO[OutputPathPattern]: Might need a better existence check.
    return stat(path.c_str(), &buffer) == 0;
#endif
}

std::string OutputPathPattern::makePathWithSuffix(
    std::string path, int uniqueID)
{
    /* Use <filesystem> to decompose the path */
    fs::path p = path;
    std::string newFilename =
        p.stem().string() + std::to_string(-uniqueID) + p.extension().string();
    p.replace_filename(newFilename);
    printf("%s, %d -> %s\n", path.c_str(), uniqueID, p.c_str());
    return p;
}

void OutputPathPattern::resolve(bool force) const
{
    if(!m_dirty && !force)
        return;
    if(m_pattern == "") {
        m_resolvedPath = "";
        m_resolvedPathExists = false;
        return;
    }

    m_substMap = m_subst();

    std::string substitutedPath = m_pattern;
    for(auto const &[key, value_desc]: m_substMap) {
        auto const &[value, desc] = value_desc;
        substitutedPath = replaceSubstring(substitutedPath, key, value);
    }

    bool exists = fileExists(substitutedPath);

    if(m_overwrite || !exists) {
        m_resolvedPath = substitutedPath;
        m_resolvedPathExists = exists;
    }
    else {
        int uniqueID = 0;
        std::string uniquePath = "";

        /* Give up after 999 tries to not lag out, just in case... */
        while(uniqueID <= 999 && exists) {
            uniqueID++;
            uniquePath = makePathWithSuffix(substitutedPath, uniqueID);
            exists = fileExists(uniquePath);
        }

        m_resolvedPath = uniquePath;
        m_resolvedPathExists = exists;
    }

    m_dirty = false;
}
