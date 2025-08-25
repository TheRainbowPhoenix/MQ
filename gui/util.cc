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
