#include <stdio.h>

#include "Thread.h"

class MyThread : public Thread {
public:
	MyThread();

private:
	bool        threadLoop();

	int mCount;
};

MyThread::MyThread(): mCount(0)
{
	printf("Create MyThread\n");
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
	MyThread *thread = new MyThread();
	thread->run("MyThread", 0, 102400);

	while (1) {
		sleep(5);
	}
	return 0;
}

