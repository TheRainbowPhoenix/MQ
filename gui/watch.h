// inotify utilities

#ifndef MQ_UI_WATCH_H
#define MQ_UI_WATCH_H 1

#include <string>

struct WatchInfo {
    int fd;
    int wd;
    bool is_file;
};
enum WatchEvent {
    MQ_WATCH_EVT_NONE           = 0,
    MQ_WATCH_EVT_UPDATED        = 1,
    MQ_WATCH_EVT_DELETED        = 2,
    MQ_WATCH_EVT_DIR_UPDATED    = 3,
};

bool watch_init(struct WatchInfo *info, std::string const &pathname);
enum WatchEvent watch_poll(struct WatchInfo *info);
bool watch_quit(struct WatchInfo *info);

#endif /* MQ_UI_WATCH_H */
