#ifndef _UTIL_MISC_H
#define _UTIL_MISC_H
#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "ThreadDefs.h"
#include "AndroidThreads.h"
#include "sched_policy.h"

#ifdef HAVE_ANDROID_OS
#define __android_unused
#else
#define __android_unused __attribute__((__unused__))
#endif

/*
 * Create and run a new thread.
 *
 * We create it "detached", so it cleans up after itself.
 */
typedef void* (*android_pthread_entry)(void*);

void androidSetThreadName(const char* name);
int androidCreateRawThreadEtc(android_thread_func_t entryFunction,
                               void *userData,
                               const char* threadName __android_unused,
                               int32_t threadPriority,
                               size_t threadStackSize,
                               android_thread_id_t *threadId);
int androidCreateThreadEtc(android_thread_func_t entryFunction,
                            void *userData,
                            const char* threadName,
                            int32_t threadPriority,
                            size_t threadStackSize,
                            android_thread_id_t *threadId);
android_thread_id_t androidGetThreadId();
int androidCreateThread(android_thread_func_t fn, void* arg);
int androidCreateThreadGetID(android_thread_func_t fn, void *arg, android_thread_id_t *id);
void androidSetCreateThreadFunc(android_create_thread_fn func);
#ifdef HAVE_ANDROID_OS
int androidSetThreadPriority(pid_t tid, int pri);
int androidGetThreadPriority(pid_t tid);
#endif

struct thread_data_t {
    thread_func_t   entryFunction;
    void*           userData;
    int             priority;
    char *          threadName;

    // we use this trampoline when we need to set the priority with
    // nice/setpriority, and name with prctl.
    static int trampoline(const thread_data_t* t) {
        thread_func_t f = t->entryFunction;
        void* u = t->userData;
        int prio = t->priority;
        char * name = t->threadName;
        delete t;
        setpriority(PRIO_PROCESS, 0, prio);
        if (prio >= ANDROID_PRIORITY_BACKGROUND) {
            set_sched_policy(0, SP_BACKGROUND);
        } else {
            set_sched_policy(0, SP_FOREGROUND);
        }

        if (name) {
            androidSetThreadName(name);
            free(name);
        }
        return f(u);
    }
};

#endif

