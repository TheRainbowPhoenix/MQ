#include "../3rdparty/portable-file-dialogs/portable-file-dialogs.h"

std::string openFileDialog()
{
    auto paths = pfd::open_file("MQ: Open file", ".",
        {"Add-ins", "*.g1a *.g3a", "All files", "*"}, pfd::opt::none).result();
    return paths.size() ? paths[0] : "";
}
