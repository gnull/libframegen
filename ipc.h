#include <signal.h>

/* SIGSLAVE_STAT - requests slave to send stats
 *
 * SIGSLAVE_STOP - requests slave to send stats for
 * the last time and then stop
 */
#define SIGSLAVE_STAT SIGUSR1
#define SIGSLAVE_STOP SIGUSR2

/* If slave has started rx/tx successfully
 * it sends to master SIGMASTER_OK
 *
 * If an error occured during slave init
 * it sends to master SIGMASTER_FAIL and then exits
 */
#define SIGMASTER_OK SIGUSR1
#define SIGMASTER_FAIL SIGUSR2

static inline void mask(sigset_t *set)
{
	int err;

	err = sigprocmask(SIG_BLOCK, set, NULL);
	if (err) {
		perror("sigprocmask(SIG_BLOCK...)");
		exit(1);
	}
}

static inline void umask(sigset_t *set)
{
	int err;

	err = sigprocmask(SIG_UNBLOCK, set, NULL);
	if (err) {
		perror("sigprocmask(SIG_UNBLOCK...)");
		exit(1);
	}
}


/* slave will use these functions to inform
 * master of successful/unsuccessful init
 */

static inline void report_fail(int ret)
{
	exit(ret);
}

static inline void report_success()
{
}
