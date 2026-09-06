#ifdef __ANDROID__
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cerrno>
#include <string>

static int pfd[2];
static pthread_t thr;
static const char *tag = "Perimeter";

static void *thread_func(void*)
{
    // Logcat's 4068-byte payload includes priority, tag and terminators.
    // Stay below it so oversized lines are chunked instead of truncated.
    constexpr size_t maxMessageBytes = 4000;
    std::string pending;
    char buf[1024];
    for (;;) {
        const ssize_t rdsz = read(pfd[0], buf, sizeof(buf));
        if (rdsz < 0 && errno == EINTR) continue;
        if (rdsz <= 0) break;
        pending.append(buf, static_cast<size_t>(rdsz));

        for (;;) {
            const size_t newline = pending.find('\n');
            if (newline <= maxMessageBytes) {
                __android_log_write(ANDROID_LOG_INFO, tag,
                                    pending.substr(0, newline).c_str());
                pending.erase(0, newline + 1);
            } else if (pending.size() > maxMessageBytes) {
                size_t count = maxMessageBytes;
                // Keep UTF-8 characters intact across oversized chunks.
                while (count > 0 &&
                       (static_cast<unsigned char>(pending[count]) & 0xc0) == 0x80) {
                    --count;
                }
                if (count == 0) count = maxMessageBytes; // Malformed UTF-8.
                __android_log_write(ANDROID_LOG_INFO, tag,
                                    pending.substr(0, count).c_str());
                pending.erase(0, count);
            } else {
                break; // Preserve incomplete lines across pipe reads.
            }
        }
    }
    if (!pending.empty()) {
        __android_log_write(ANDROID_LOG_INFO, tag, pending.c_str());
    }
    return nullptr;
}

static __attribute__((constructor)) void start_logger()
{
    // make stdout line-buffered and stderr unbuffered
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // create the pipe and redirect stdout and stderr
    pipe(pfd);
    dup2(pfd[1], STDOUT_FILENO);
    dup2(pfd[1], STDERR_FILENO);

    // spawn the logging thread
    if(pthread_create(&thr, nullptr, thread_func, nullptr) != 0)
        return;
    pthread_detach(thr);
}
#endif
