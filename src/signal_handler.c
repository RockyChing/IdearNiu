#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <wait.h>

#include <log_util.h>
static void sig_handler(int signo);

static void sig_child(void)
{
	pid_t pid;
	int stat;
	pid = wait(&stat);
	(void) pid;

	struct sigaction sa;
	sa.sa_handler = sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGCHLD, &sa, 0);
}

static void sig_handler(int signo)
{
	func_enter();
	switch (signo) {
	case SIGINT:
	case SIGTERM:
	case SIGPIPE:
	case SIGUSR1:
		printf("Opps, process terminated!\n");
		exit(0);
		break;
	case SIGCHLD:
		sig_child();
		break;
	default:
		printf("Opps, process terminated!\n");
		exit(0);
		break;
	}
	func_exit();
}

void setup_signal_handler()
{
	func_enter();
	struct sigaction sa;
	sa.sa_handler = sig_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	/**
	 * typedef void (*sighandler_t)(int);
	 * sighandler_t signal(int signum, sighandler_t handler);
	 * 
	 * signal() sets the disposition if the signal signum to handler, which is either
	 * SIG_IGN, SIG_DEF, or the address of a paremeter-defined function ("signal handler")
	 *
	 * If the signal signum is delivered to the process, then one of the following happens:
	 *	- If the disposition is set to SIG_IGN, then the signal is ignored.
	 *	- If the disposition is set to SIG_DFL, then the default action associated with the signal
	 *	- If the disposition is set to a function, then first either the disposition is
	 *	  reset to SIG_DFL, or the signal is blocked, and then handler is called with argument signum
	 *
	 * int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
	 * The sigaction() system call is used to change the action taken by a process on receipt of a specific signal
	 *
	 * !The signals SIGKILL and SIGSTOP cannot be caught or ignored
	 */
	// CPU检测到某进程执行了非法指令。默认动作为终止进程并产生core文件
	sigaction(SIGILL,  &sa, 0);
	// 非法访问内存地址，包括内存对齐出错，默认动作为终止进程并产生core文件 
	sigaction(SIGBUS,  &sa, 0);
	// 指示进程进行了无效内存访问。默认动作为终止进程并产生core文件
	sigaction(SIGSEGV, &sa, 0);
	// 无效的系统调用。默认动作为终止进程并产生core文件
	sigaction(SIGSYS,  &sa, 0);
	// 当用户退出shell时，由该shell启动的所有进程将收到这个信号，默认动作为终止进程
	sigaction(SIGHUP,  &sa, 0);
	// Broken pipe向一个没有读端的管道写数据。默认动作为终止进程
	sigaction(SIGPIPE, &sa, 0);
	// 当用户按下了< Ctrl+C>组合键时，用户终端向正在运行中的由该终端启动的程序发出此信号。默认动 作为终止里程
	sigaction(SIGINT,  &sa, 0);
	// 用户定义 的信号。即程序员可以在程序中定义并使用该信号。默认动作为终止进程
	sigaction(SIGUSR1, &sa, 0);
	// 程序结束信号，与SIGKILL不同的是，该信号可以被阻塞和终止。通常用来要示程序正常退出。执行shell命令Kill时，缺省产生这个信号。默认动作为终止进程
	sigaction(SIGTERM, &sa, 0);
	sigaction(SIGCHLD, &sa, 0);

	func_exit();
}
