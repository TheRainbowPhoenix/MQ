#include <azur/defs.h>
#include "watch.h"
#include <mq/mq.h>

#if AZUR_PLATFORM_LINUX

#include <errno.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#define __INOTIFY_EVENT_SIZE \
    (sizeof(struct inotify_event) + NAME_MAX + 1)

bool watch_init(struct WatchInfo *info, std::filesystem::path &pathname)
{
    if(info == nullptr || pathname.empty()) {
        mq_log(MQ_LOG_ERROR, "watch_init: invalid arguments");
        return false;
    }
    if(info->fd < 0) {
        info->fd = inotify_init1(IN_NONBLOCK);
        if (info->fd == -1) {
            mq_log(MQ_LOG_ERROR, "watch_init: inotify_init1 failed!");
            return false;
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
        IN_CLOSE_WRITE | IN_DELETE_SELF);
    if(info->wd == -1) {
        mq_log(MQ_LOG_ERROR, "inotify_add_watch: cannot watch '%s': %s",
            pathname, strerror(errno));
        return false;
    }
    return true;
}

enum WatchEvent watch_poll(struct WatchInfo *info)
{
    static char buff[__INOTIFY_EVENT_SIZE];
    static struct inotify_event *event = NULL;
    static ssize_t size = 0;
    enum WatchEvent watch_event;

    if(!info) {
        mq_log(MQ_LOG_ERROR, "watch_poll: invalid argument");
        return MQ_WATCH_EVT_NONE;
    }
    if(info->fd < 0)
        return MQ_WATCH_EVT_NONE;

    while (true) {
        // process "pending" (see the comment on the `read()` operation)
        // from the previous event fetching
        if(size >= sizeof(struct inotify_event)) {
            if(event->mask & IN_CLOSE) {
                watch_event = MQ_WATCH_EVT_UPDATED;
            } else if(event->mask & IN_DELETE_SELF) {
                watch_event = MQ_WATCH_EVT_DELETED;
            } else if(event->mask & IN_IGNORED) {
                watch_event = MQ_WATCH_EVT_NONE;
            } else {
                mq_log(MQ_LOG_WARNING,
                    "watch_poll: unknown event %x\n", event->mask);
                watch_event = MQ_WATCH_EVT_NONE;
            }
            uintptr_t event_size = sizeof(struct inotify_event) + event->len;
            event = (struct inotify_event *)((uintptr_t)event + event_size);
            size  = size - event_size;
            if (watch_event == MQ_WATCH_EVT_NONE)
                continue;
            return watch_event;
        }
        if(size != 0)
            mq_log(MQ_LOG_WARNING, "watch_poll: non-null event size");

        // Since, the `read()` operation is always blocking until a new
        // event occur, we need to manually check if we have pending data
        // in the "event queue"
        if(ioctl(info->fd, FIONREAD, &size) != 0) {
            mq_log(MQ_LOG_ERROR, "watch_poll: ioctl FIONREAD error: %s",
                strerror(errno));
            break;
        }
        if((unsigned long)size < sizeof(struct inotify_event))
            break;
        // request at least the maximum size of an event as specified in
        // `man inotify.7`::`Reading events from an inotify file descriptor`
        //
        // If you try to read only the size of the event structure, which
        // works almost anytime on simple file monitoring, will fail
        // miserably with directories that, most of the time, contain a
        // file name that exceeds the basic size of the structure. Note that
        // if you try to read an event with a buffer size less than the
        // event data itself, `read()` will return -1 with
        // `Invalid arguments` as`errno` context.
        size = read(info->fd, buff, __INOTIFY_EVENT_SIZE);
        if (size < 0) {
            mq_log(MQ_LOG_ERROR, "watch_poll: broken received event size");
            break;
        }
        event = (struct inotify_event *)buff;
    }

    return MQ_WATCH_EVT_NONE;
}

bool watch_quit(struct WatchInfo *info)
{
    if(!info) {
        mq_log(MQ_LOG_ERROR, "watch_quit: invalid arguments");
        return false;
    }
    if(info->fd >= 0) {
        if(close(info->fd))
            mq_log(MQ_LOG_ERROR, "watch_quit: unable to close inotify");
    }
    info->fd = -1;
    info->wd = -1;
    return true;
}

#else

bool watch_init(struct WatchInfo *info, std::filesystem::path &pathname)
{
    (void)info;
    (void)pathname;
    return false;
}

enum WatchEvent watch_poll(struct WatchInfo *info)
{
    (void)info;
    return MQ_WATCH_EVT_NONE;
}

bool watch_quit(struct WatchInfo *info)
{
    (void)info;
    return true;
}

#endif /* AZUR_PLATFORM_LINUX */
