// inotify utilities

#ifndef MQ_UI_WATCH_H
#define MQ_UI_WATCH_H 1

struct WatchInfo {
    int fd;
    int wd;
};
enum WatchEvent {
    MQ_WATCH_EVT_NONE    = 0,
    MQ_WATCH_EVT_UPDATED = 1,
    MQ_WATCH_EVT_DELETED = 2,
};

int watch_init(struct WatchInfo *info, char const *pathname);
enum WatchEvent watch_poll(struct WatchInfo *info);

#endif /* MQ_UI_WATCH_H */
