#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "misc.h"
#include "log.h"

#define LOG_TAG "THREAD_MISC"

void androidSetThreadName(const char* name)
{
#if defined(__linux__)
    // Mac OS doesn't have this, and we build libutil for the host too
    int hasAt = 0;
    int hasDot = 0;
    const char *s = name;
    while (*s) {
        if (*s == '.') hasDot = 1;
        else if (*s == '@') hasAt = 1;
        s++;
    }
    int len = s - name;
    if (len < 15 || hasAt || !hasDot) {
        s = name;
    } else {
        s = name + len - 15;
    }
    prctl(PR_SET_NAME, (unsigned long) s, 0, 0, 0);
#endif
}

int androidCreateRawThreadEtc(android_thread_func_t entryFunction,
                               void *userData,
                               const char* threadName __android_unused,
                               int32_t threadPriority,
                               size_t threadStackSize,
                               android_thread_id_t *threadId)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

#ifdef HAVE_ANDROID_OS  /* valgrind is rejecting RT-priority create reqs */
    if (threadPriority != PRIORITY_DEFAULT || threadName != NULL) {
        // Now that the pthread_t has a method to find the associated
        // android_thread_id_t (pid) from pthread_t, it would be possible to avoid
        // this trampoline in some cases as the parent could set the properties
        // for the child.  However, there would be a race condition because the
        // child becomes ready immediately, and it doesn't work for the name.
        // prctl(PR_SET_NAME) only works for self; prctl(PR_SET_THREAD_NAME) was
        // proposed but not yet accepted.
        thread_data_t* t = new thread_data_t;
        t->priority = threadPriority;
        t->threadName = threadName ? strdup(threadName) : NULL;
        t->entryFunction = entryFunction;
        t->userData = userData;
        entryFunction = (android_thread_func_t)&thread_data_t::trampoline;
        userData = t;
    }
#endif

    if (threadStackSize) {
        pthread_attr_setstacksize(&attr, threadStackSize);
    }

    errno = 0;
    pthread_t thread;
    int result = pthread_create(&thread, &attr,
                    (android_pthread_entry)entryFunction, userData);
    pthread_attr_destroy(&attr);
    if (result != 0) {
        ALOGE("androidCreateRawThreadEtc failed (entry=%p, res=%d, errno=%d)\n"
             "(android threadPriority=%d)",
            entryFunction, result, errno, threadPriority);
        return 0;
    }

    // Note that *threadID is directly available to the parent only, as it is
    // assigned after the child starts.  Use memory barrier / lock if the child
    // or other threads also need access.
    if (threadId != NULL) {
        *threadId = (android_thread_id_t)thread; // XXX: this is not portable
    }
    return 1;
}

#ifdef HAVE_ANDROID_OS
static pthread_t android_thread_id_t_to_pthread(android_thread_id_t thread)
{
    return (pthread_t) thread;
}
#endif

android_thread_id_t androidGetThreadId()
{
    return (android_thread_id_t)pthread_self();
}

// ----------------------------------------------------------------------------
int androidCreateThread(android_thread_func_t fn, void* arg)
{
    return createThreadEtc(fn, arg);
}

int androidCreateThreadGetID(android_thread_func_t fn, void *arg, android_thread_id_t *id)
{
    return createThreadEtc(fn, arg, "android:unnamed_thread",
                           PRIORITY_DEFAULT, 0, id);
}

static android_create_thread_fn gCreateThreadFn = androidCreateRawThreadEtc;

int androidCreateThreadEtc(android_thread_func_t entryFunction,
                            void *userData,
                            const char* threadName,
                            int32_t threadPriority,
                            size_t threadStackSize,
                            android_thread_id_t *threadId)
{
    return gCreateThreadFn(entryFunction, userData, threadName,
        threadPriority, threadStackSize, threadId);
}

void androidSetCreateThreadFunc(android_create_thread_fn func)
{
    gCreateThreadFn = func;
}

#ifdef HAVE_ANDROID_OS
int androidSetThreadPriority(pid_t tid, int pri)
{
    int rc = 0;
    int lasterr = 0;

    if (pri >= ANDROID_PRIORITY_BACKGROUND) {
        rc = set_sched_policy(tid, SP_BACKGROUND);
    } else if (getpriority(PRIO_PROCESS, tid) >= ANDROID_PRIORITY_BACKGROUND) {
        rc = set_sched_policy(tid, SP_FOREGROUND);
    }

    if (rc) {
        lasterr = errno;
    }

    if (setpriority(PRIO_PROCESS, tid, pri) < 0) {
        rc = INVALID_OPERATION;
    } else {
        errno = lasterr;
    }

    return rc;
}

int androidGetThreadPriority(pid_t tid)
{
    return getpriority(PRIO_PROCESS, tid);
}
#endif

