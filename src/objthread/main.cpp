#include <stdio.h>

#define LOG_TAG "Main"

#include "Thread.h"
#include "log.h"

class MyThread : public Thread {
public:
	MyThread();

private:
	bool        threadLoop();
	void		onFirstRef();
	void		onLastStrongRef(const void* id);

	int mCount;
};

MyThread::MyThread(): mCount(0)
{
	ALOGD("Create MyThread");
}

void MyThread::onFirstRef()
{
	ALOGD("onFirstRef");
}

void MyThread::onLastStrongRef(const void* id)
{
	ALOGD("onLastStrongRef: %p", id);
}

bool MyThread::threadLoop()
{
	while (1) {
		printf("mCount: %d\n", ++mCount);
		sleep(1);
	}

	return true;
}

int main(int argc, char **argv)
{
	sp<MyThread> thread = new MyThread();
	//thread->run("MyThread", PRIORITY_DEFAULT, 102400);

	while (1) {
		sleep(3);
		break;
	}

	sleep(1);
	return 0;
}

