#include <sys/types.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h> /* the L2 protocols */
#include <net/if.h>

#include <linux/net_tstamp.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <time.h>

#include "master.h"
#include "export.h"
#include "ipc.h"
#include "util.h"

static int master_pipe;
static unsigned int flowid;

static struct flist_head stat;

/*
 * Socket initialization
 */

static int sockfd;

static void setup_sock()
{
	int err;

	sockfd = socket(AF_PACKET, SOCK_RAW, 0);
	if (sockfd == -1) {
		perror("socket");
		report_fail(1);
	}

	struct sockaddr_ll addr = {
		.sll_family = AF_PACKET,
		.sll_ifindex  = if_nametoindex(rx_ifname),
		.sll_protocol = htons(ETH_P_ALL),
	};

	if (!addr.sll_ifindex) {
		perror("if_nametoindex");
		ERR("can't get rx interface index");
		exit(1);
	}

	err = bind(sockfd, (struct sockaddr *) &addr, sizeof(addr));
	if (err) {
		perror("bind");
		report_fail(1);
	}

	int val = SOF_TIMESTAMPING_RX_SOFTWARE |
		/* SOF_TIMESTAMPING_RX_HARDWARE | */
		SOF_TIMESTAMPING_SOFTWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;

	err = setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof(val));
	if (err) {
		perror("setsockopt");
		report_fail(1);
	}
}



/*
 * Signal handlers initialization
 */

static void handle(int signum);

static sigset_t signals;

#define ENTER_CS mask(&signals)
/* The code here will not be interrupted by master */
#define LEAVE_CS umask(&signals)

static void setup_signals()
{
	int err;
	struct sigaction act = {
		.sa_mask = signals
	};

	sigemptyset(&signals);
	sigaddset(&signals, SIGSLAVE_STAT);
	sigaddset(&signals, SIGSLAVE_STOP);

	act.sa_handler = handle;
	err = sigaction(SIGSLAVE_STAT, &act, NULL);
	if (err) {
		perror("sigaction");
		report_fail(1);
	}

	err = sigaction(SIGSLAVE_STOP, &act, NULL);
	if (err) {
		perror("sigaction");
		report_fail(1);
	}
}

/*
 * Signal handler
 */

void handle(int signum)
{
	int err;

	err = fl_send(&stat, master_pipe);
	if (err) {
		ERR("failed to send stat");
		exit(1);
	}

	fl_free(&stat);

	if (signum == SIGSLAVE_STOP)
		exit(0);
}


static void recv_pkt()
{
	int err;
	int len;
	struct ethhdr eth;
	struct iphdr ip;
	struct udphdr udp;
	struct payload payload;
	struct sockaddr_ll addr;
	struct timespec ts;
	struct cmsghdr *i;
	char cmsg[1000];

	struct iovec iov[] = {
		{
			.iov_base = &eth,
			.iov_len  = sizeof(eth)
		}, {
			.iov_base = &ip,
			.iov_len  = sizeof(ip)
		}, {
			.iov_base = &udp,
			.iov_len  = sizeof(udp)
		}, {
			.iov_base = &payload,
			.iov_len  = sizeof(payload)
		}
	};

	struct msghdr msg = {
		.msg_name = &addr,
		.msg_namelen = sizeof(addr),
		.msg_iov = iov,
		.msg_iovlen = sizeof(iov)/sizeof(*iov),
		.msg_control = cmsg,
		.msg_controllen = sizeof(cmsg)
	};

again:

	len = recvmsg(sockfd, &msg, 0);
	if (len == -1) {
		if (errno == EINTR)
			goto again;
		perror("recvmsg");
		exit(1);
	}
	err = clock_gettime(CLOCK_REALTIME, &ts);
	if (err == -1) {
		perror("clock_gettime");
		exit(1);
	}

	if (addr.sll_pkttype == PACKET_OUTGOING)
		goto again;

	if (len < HEADERS_LEN + sizeof(payload))
		goto again;

	if (payload.magic != MAGIC)
		goto again;

	if (payload.flowid != flowid)
		goto again;

	struct scm_timestamping *tss = 0;
	struct timespec *soft, *hard, *result;

	for_cmsg(i, &msg)
		if (i->cmsg_level == SOL_SOCKET &&
		    i->cmsg_type == SCM_TIMESTAMPING)
			tss = (struct scm_timestamping *) CMSG_DATA(i);

	assert(tss);
	soft = tss->ts;
	hard = tss->ts + 2;

	if (!ts_empty(hard))
		result = hard;
	else if (!ts_empty(soft))
		result = soft;
	else
		result = &ts;

	ENTER_CS;
	fl_push(&stat, payload.seq, result);
	LEAVE_CS;
}

int rx(unsigned int fid, int out)
{
	master_pipe = out;
	flowid = fid;
	fl_clear(&stat);

	setup_sock();
	setup_signals();

	while (1) {
		recv_pkt();
	}
}
