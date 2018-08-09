#include <stdio.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>


static pid_t safe_waitpid(pid_t pid, int *wstat, int options)
{
	pid_t r;

	do {
		r = waitpid(pid, wstat, options);
	} while ((r == -1) && (errno == EINTR));
	return r;
}

/*
 * Wait for the specified child PID to exit, returning child's error return.
 */
static int wait4pid(pid_t pid)
{
	int status;

	if (pid <= 0) {
		/*errno = ECHILD; -- wrong. */
		/* we expect errno to be already set from failed [v]fork/exec */
		return -1;
	}

	if (safe_waitpid(pid, &status, 0) == -1)
		return -1;

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	if (WIFSIGNALED(status))
		return WTERMSIG(status) + 0x180;

	return 0;
}
/**
 * This does a fork/exec in one call, using vfork().  Returns PID of new child,
 * -1 for failure.  Runs argv[0], searching path if that has no / in it.
 */
static pid_t spawn(char **argv)
{
	/* Compiler should not optimize stores here */
	volatile int failed;
	pid_t pid;

	fflush(NULL);

	/* Be nice to nommu machines. */
	failed = 0;
	pid = vfork();
	if (pid < 0) /* error */
		return pid;

	if (!pid) { /* child */
		/* This macro is ok - it doesn't do NOEXEC/NOFORK tricks */
		execvp(argv[0], argv);

		/* We are (maybe) sharing a stack with blocked parent,
		 * let parent know we failed and then exit to unblock parent
		 * (but don't run atexit() stuff, which would screw up parent.)
		 */
		failed = errno;
		/* mount, for example, does not want the message */
		/*bb_perror_msg("can't execute '%s'", argv[0]);*/
		_exit(111);
	}

	/* parent */
	/* Unfortunately, this is not reliable: according to standards
	 * vfork() can be equivalent to fork() and we won't see value
	 * of 'failed'.
	 * Interested party can wait on pid and learn exit code.
	 * If 111 - then it (most probably) failed to exec */
	if (failed) {
		safe_waitpid(pid, NULL, 0); /* prevent zombie */
		errno = failed;
		return -1;
	}
	return pid;
}

int spawn_and_wait(char **argv)
{
	int rc;
	rc = spawn(argv);
	return wait4pid(rc);
}

