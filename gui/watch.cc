#include <errno.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "watch.h"
#include <mq/mq.h>

int watch_init(struct WatchInfo *info, std::filesystem::path &pathname)
{
    if(info == nullptr || pathname.empty()) {
        mq_log(MQ_LOG_ERROR, "watch_init: invalid arguments");
        return -99;
    }
    if(info->fd < 0) {
        info->fd = inotify_init1(IN_NONBLOCK);
        if (info->fd == -1) {
            mq_log(MQ_LOG_ERROR, "inotify_init1 failed!");
            return -1;
        }
    } else {
        if(info->wd >= 0) {
            inotify_rm_watch(info->fd, info->wd);
            info->wd = -1;
        }
    }
    info->wd = inotify_add_watch(
        info->fd,
        pathname.c_str(),
        IN_CLOSE_WRITE | IN_DELETE_SELF
    );
    if (info->wd == -1) {
        mq_log(
            MQ_LOG_ERROR,
            "inotify_add_watch: cannot watch '%s': %s",
            pathname, strerror(errno)
        );
        return -2;
    }
    info->addin_path = pathname;
    return 0;
}

enum WatchEvent watch_poll(struct WatchInfo *info)
{
    struct inotify_event event;
    int size;

    if(info == nullptr) {
        mq_log(MQ_LOG_ERROR, "watch_poll: invalid argument");
        return MQ_WATCH_EVT_NONE;
    }
    if(info->fd < 0)
        return MQ_WATCH_EVT_NONE;
    while (true) {
        if(ioctl(info->fd, FIONREAD, &size) != 0) {
            mq_log(
                MQ_LOG_ERROR,
                "watch_poll: ioctl FIONREAD error: %s",
                strerror(errno)
            );
            break;
        }
        if((unsigned long)size < sizeof(event))
            break;
        if(read(info->fd, &event, sizeof(event)) != sizeof(event)) {
            mq_log(MQ_LOG_ERROR, "watch_poll: broken received event size");
            break;
        }
        if(event.mask & IN_CLOSE)
            return MQ_WATCH_EVT_UPDATED;
        if(event.mask & IN_DELETE_SELF)
            return MQ_WATCH_EVT_DELETED;
        if(event.mask & IN_IGNORED)
            continue;
        mq_log(MQ_LOG_WARNING, "watch_poll: unknown event %x\n", event.mask);
    }
    return MQ_WATCH_EVT_NONE;
}

int watch_quit(struct WatchInfo *info)
{
    if(info == nullptr) {
        mq_log(MQ_LOG_ERROR, "watch_quit: invalid arguments");
        return -99;
    }
    if(info->fd >= 0) {
        if(close(info->fd) != 0)
            mq_log(MQ_LOG_ERROR, "watch_quit: unable to close inotify");
    }
    info->fd = -1;
    info->wd = -1;
    info->addin_path = "";
    return 0;
}
