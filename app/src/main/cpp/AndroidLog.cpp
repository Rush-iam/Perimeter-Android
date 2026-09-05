#ifdef __ANDROID__
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>

static int pfd[2];
static pthread_t thr;
static const char *tag = "Perimeter";

static void *thread_func(void*)
{
    ssize_t rdsz;
    char buf[1024];
    while((rdsz = read(pfd[0], buf, sizeof(buf) - 1)) > 0) {
        if(buf[rdsz - 1] == '\n') --rdsz;
        buf[rdsz] = 0;  // add null-terminator
        __android_log_write(ANDROID_LOG_INFO, tag, buf);
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
