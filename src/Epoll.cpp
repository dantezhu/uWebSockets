#include "Backend.h"

#ifdef USE_EPOLL

namespace uS {

// todo: remove this mutex, have callbacks set at program start
std::recursive_mutex cbMutex;
void (*callbacks[16])(Poll *, int, int);
int cbHead = 0;

void Loop::run() {
    timepoint = std::chrono::system_clock::now();
    // add by dantezhu
    // 直接返回
    return;
    while (numPolls) {
        for (std::pair<Poll *, void (*)(Poll *)> c : closing) {
            numPolls--;

            c.second(c.first);

            if (!numPolls) {
                closing.clear();
                return;
            }
        }
        closing.clear();

        int numFdReady = epoll_wait(epfd, readyEvents, 1024, delay);
        timepoint = std::chrono::system_clock::now();

        if (preCb) {
            preCb(preCbData);
        }

        for (int i = 0; i < numFdReady; i++) {
            Poll *poll = (Poll *) readyEvents[i].data.ptr;
            int status = -bool(readyEvents[i].events & EPOLLERR);
            callbacks[poll->state.cbIndex](poll, status, readyEvents[i].events);
        }

        while (timers.size() && timers[0].timepoint < timepoint) {
            Timer *timer = timers[0].timer;
            cancelledLastTimer = false;
            timers[0].cb(timers[0].timer);

            if (cancelledLastTimer) {
                continue;
            }

            int repeat = timers[0].nextDelay;
            auto cb = timers[0].cb;
            timers.erase(timers.begin());
            if (repeat) {
                timer->start(cb, repeat, repeat);
            }
        }

        if (postCb) {
            postCb(postCbData);
        }
    }
}


// add by dantezhu
void Loop::onEpollEventsBegin() {
    timepoint = std::chrono::system_clock::now();
    if (preCb) {
        preCb(preCbData);
    }
}

// add by dantezhu
void Loop::onEpollEventsEnd() {
    while (timers.size() && timers[0].timepoint < timepoint) {
        Timer *timer = timers[0].timer;
        cancelledLastTimer = false;
        timers[0].cb(timers[0].timer);

        if (cancelledLastTimer) {
            continue;
        }

        int repeat = timers[0].nextDelay;
        auto cb = timers[0].cb;
        timers.erase(timers.begin());
        if (repeat) {
            timer->start(cb, repeat, repeat);
        }
    }

    if (postCb) {
        postCb(postCbData);
    }

    for (std::pair<Poll *, void (*)(Poll *)> c : closing) {
        numPolls--;

        c.second(c.first);

        if (!numPolls) {
            closing.clear();
            return;
        }
    }
    closing.clear();
}

}

#endif
