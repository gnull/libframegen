#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

#include <signal.h>

#include "master.h"
#include "ipc.h"


static header_cfg_t header;
static ethrate_t rate;
static unsigned int fsize,
	rx_flowid, tx_flowid;

static int tx_pid, rx_pid,
	tx_pipe, rx_pipe;

static uint32_t rx_cnt, tx_cnt;


/* TX control functions */

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

static int tx_stop()
{
	int err;
	siginfo_t info;

	err = kill(tx_pid, SIGSLAVE_STOP);
	if (err)
		return perror("kill"), 1;

	err = read(tx_pipe, &tx_cnt, sizeof(tx_cnt));
	if (err == -1)
		return perror("read"), 1;

	INFO("read %d bytes", err);
	INFO("waiting for tx to terminate");

	err = waitid(P_PID, tx_pid, &info, WEXITED);
	if (err)
		return perror("waitid"), 1;
	INFO("tx terminated with status %d", info.si_status);

	return 0;
}

static int tx_get_stat(uint32_t *tx)
{
	int err;

	err = kill(tx_pid, SIGSLAVE_STAT);
	if (err) {
		if (errno == ESRCH)
			goto out;
		perror("kill");
		return 1;
	}

	err = read(tx_pipe, &tx_cnt, sizeof(tx_cnt));
	if (err == -1)
		return perror("read"), 1;
	INFO("read %d bytes", err);
out:
	*tx = tx_cnt;
	return 0;
}



/*
 * RX control functions
 */

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
		close(slave_end);
		return 0;
	} else {
		perror("fork");
		return 1;
	}
}

static int rx_stop()
{
	int err;
	siginfo_t info;

	err = kill(rx_pid, SIGSLAVE_STOP);
	if (err)
		return perror("kill"), 1;

	err = read(rx_pipe, &rx_cnt, sizeof(rx_cnt));
	if (err == -1)
		return perror("read"), 1;

	INFO("read %d bytes", err);
	INFO("waiting for rx to terminate");

	err = waitid(P_PID, rx_pid, &info, WEXITED);
	if (err)
		return perror("waitid"), 1;
	INFO("rx terminated with status %d", info.si_status);

	return 0;
}

static int rx_get_stat(uint32_t *rx, double *lat)
{
	int err;

	err = kill(rx_pid, SIGSLAVE_STAT);
	if (err) {
		if (errno == ESRCH)
			goto out;
		perror("kill");
		return 1;
	}

	err = read(rx_pipe, &rx_cnt, sizeof(rx_cnt));
	if (err == -1)
		return perror("read"), 1;
	INFO("read %d bytes", err);
out:
	*rx = rx_cnt;
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
