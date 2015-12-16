#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <signal.h>

#include "master.h"
#include "ipc.h"
#include "util.h"

static header_cfg_t header;
static ethrate_t rate;
static unsigned int fsize,
	rx_flowid, tx_flowid;

static int tx_pid, rx_pid,
	tx_pipe, rx_pipe;

static struct flist_head rx_stat, tx_stat;


/*
 * Start functions
 */

static int tx_start()
{
	int err;
	int fd[2], slave_end;

	pipe(fd);

	tx_pipe = fd[0];
	slave_end = fd[1];

	tx_pid = fork();
	if (tx_pid == 0) {
		close(tx_pipe);
		err = tx(&header, rate, fsize, tx_flowid, slave_end);
		INFO("tx returned %d\n", err);
		exit(err);
	} else if (tx_pid > 0) {
		close(slave_end);
		return 0;
	} else {
		perror("fork");
		return 1;
	}
}

static int rx_start()
{
	int err;
	int fd[2], slave_end;

	pipe(fd);

	rx_pipe = fd[0];
	slave_end = fd[1];

	rx_pid = fork();
	if (rx_pid == 0) {
		close(rx_pipe);
		err = rx(rx_flowid, slave_end);
		INFO("rx returned %d\n", err);
		exit(err);
	} else if (rx_pid > 0) {
		INFO("rx_started at pid %d", rx_pid);
		close(slave_end);
		return 0;
	} else {
		perror("fork");
		return 1;
	}
}

/*
 * Stop functions
 */

static int slave_stop(int fd, int pid, struct flist_head *head)
{
	int err;
	siginfo_t info;

	err = kill(pid, SIGSLAVE_STOP);
	if (err)
		return perror("kill"), 1;

	err = fl_recv_append(fd, head);
	if (err)
		ERR("failed to fl_recv_append");

	INFO("waiting for %d to terminate", pid);

	err = waitid(P_PID, pid, &info, WEXITED);
	if (err)
		return perror("waitid"), 1;
	INFO("%d terminated with status %d", pid, info.si_status);

	return 0;
}

static int tx_stop()
{
	return slave_stop(tx_pipe, tx_pid, &tx_stat);
}


static int rx_stop()
{
	return slave_stop(rx_pipe, rx_pid, &rx_stat);
}

/*
 * Statistics functions
 */

static int slave_stat(int fd, int pid, struct flist_head *head)
{
	int err;

	err = kill(pid, SIGSLAVE_STAT);
	if (err) {
		if (errno == ESRCH)
			return 0;
		perror("kill");
		return 1;
	}

	err = fl_recv_append(fd, head);
	if (err)
		ERR("fl_recv_append failed to get tx_stat");
	return err;
}

static int tx_get_stat(uint32_t *tx)
{
	int err;

	err = slave_stat(tx_pipe, tx_pid, &tx_stat);
	if (err)
		return err;

	if (tx)
		*tx = tx_stat.size;
	return 0;
}


static int rx_get_stat(uint32_t *rx, double *lat)
{
	int err;

	err = slave_stat(rx_pipe, rx_pid, &rx_stat);
	if (err)
		return err;

	if (rx)
		*rx = rx_stat.size;
	if (lat)
		*lat = fl_latency(&rx_stat, &tx_stat);
	return 0;
}

/*
 * Configuration functions
 */

static int tx_conf_header(header_cfg_t *hdr)
{
	header = *hdr;
	return 0;
}

static int tx_conf_rate(ethrate_t ethrate)
{
	if (ethrate.units == RATE_PERCENT)
		return 1;
	rate = ethrate;
	return 0;
}

static int tx_conf_framesize(unsigned int size)
{
	if (size < sizeof(struct payload) + HEADERS_LEN)
		return 1;
	fsize = size;
	return 0;
}

static int tx_conf_flowid(unsigned int fid)
{
	tx_flowid = fid;
	return 0;
}

static int rx_conf_flowid(unsigned int fid)
{
	rx_flowid = fid;
	return 0;
}


/*
 * Interface to librfc2544
 *
 */

int init_ctrl_handler(rfc2544_ctrl_handler_t *handler, void *context)
{
	rfc2544_ctrl_handler_t hand = {
		.tx = {
			.start = tx_start,
			.stop = tx_stop,
			.get_stat = tx_get_stat,
			.conf_header = tx_conf_header,
			.conf_rate = tx_conf_rate,
			.conf_framesize = tx_conf_framesize,
			.conf_flowid = tx_conf_flowid,
		},
		.rx = {
			.start = rx_start,
			.stop = rx_stop,
			.conf_flowid = rx_conf_flowid,
			.get_stat = rx_get_stat,
		}
	};

	*handler = hand;
	return 0;
}

void deinit_ctrl_hanler()
{
}
