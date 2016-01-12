#include <sys/time.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h> /* the L2 protocols */

#include <unistd.h>
#include <net/if.h>

#include <linux/net_tstamp.h>

#include <time.h>

#include "export.h"
#include "master.h"
#include "ipc.h"
#include "util.h"

//#define ENABLE_TX_SCHED

static int master_pipe;
static unsigned int flowid, fsize;
static uint32_t pktnum;

static struct flist_head stat;

/*
 * Frame/Socket initialization
 */

static int sockfd;

static void setup_sock()
{
	int err;
	int val;

	sockfd = socket(AF_PACKET, SOCK_RAW, 0);
	if (sockfd == -1) {
		perror("socket");
		exit(1);
	}

	val = SOF_TIMESTAMPING_TX_HARDWARE |
#ifdef ENABLE_TX_SCHED
		SOF_TIMESTAMPING_TX_SCHED |
#endif
		SOF_TIMESTAMPING_TX_SOFTWARE |
		SOF_TIMESTAMPING_SOFTWARE |
		SOF_TIMESTAMPING_RAW_HARDWARE;

	err = setsockopt(sockfd, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof(val));
	if (err) {
		perror("setsockopt");
		exit(1);
	}

	/*
	 * Should we setsockopt(..., SOL_PACKET, PACKET_QDISC_BYPASS ...) here?
	 */
}

static struct iovec iov[4];
static struct sockaddr_ll addr;
static struct msghdr msg;
struct payload *payload;

static void setup_frame(header_cfg_t *header)
{
	int payload_len = fsize - HEADERS_LEN;

	int udp_len = sizeof(header->udp) + payload_len;
	int ip_len  = sizeof(header->ip) + udp_len;

	header->ip.tot_len = htons(ip_len);
	header->udp.len = htons(udp_len);

	iov[0].iov_base = &header->eth;
	iov[0].iov_len  = sizeof(header->eth);

	iov[1].iov_base = &header->ip;
	iov[1].iov_len  = sizeof(header->ip);

	iov[2].iov_base = &header->udp;
	iov[2].iov_len  = sizeof(header->udp);

	payload = malloc(payload_len);
	iov[3].iov_base = payload;
	iov[3].iov_len  = payload_len;

	payload->magic = MAGIC;
	payload->flowid = flowid;

	addr.sll_family = AF_PACKET;
	addr.sll_ifindex = if_nametoindex(tx_ifname);

	if (!addr.sll_ifindex) {
		perror("if_nametoindex");
		ERR("can't get rx interface index");
		exit(1);
	}

	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = iov;
	msg.msg_iovlen = sizeof(iov)/sizeof(*iov);
}

/*
 * Signal mask/handlers initialization
 */

static void send_frame(int signum);
static void send_stats(int signum);
static void stop(int sugnum);

static sigset_t signals;

static void set_handler(int signum, void (*handler)(int))
{
	int err;
	struct sigaction act = {
		.sa_mask = signals,
		.sa_handler = handler,
	};

	err = sigaction(signum, &act, NULL);
	if (err) {
		perror("sigaction");
		exit(1);
	}
}


#define ENTER_CS mask(&signals)
/* The code here will not be interrupted by master */
#define LEAVE_CS umask(&signals)

static void setup_signals()
{
	sigemptyset(&signals);
	sigaddset(&signals, SIGALRM);
	sigaddset(&signals, SIGSLAVE_STAT);
	sigaddset(&signals, SIGSLAVE_STOP);

	set_handler(SIGALRM, send_frame);
	set_handler(SIGSLAVE_STAT, send_stats);
	set_handler(SIGSLAVE_STOP, stop);
}


/*
 * Timer initialization
 */

static void rate_to_tv(ethrate_t *rate, struct timeval *tv)
{
	static const long mega = 1000 * 1000;
	double val = rate->val;

	switch (rate->units) {
	case RATE_PERCENT:
		ERR("rate->units == RATE_PERCENT");
		exit(1);
	case RATE_KBPS:
		val *= 1000;
	case RATE_MBPS:
		val *= 1000;
	case RATE_GBPS:
		val *= 1000;
		break;
	default:
		ERR("uknown rate units");
		exit(1);
	}

	val /= 8;		/* bytes per second */
	val /= fsize;		/* frames per second */

	tv->tv_sec = 1 / val;	/* interval in seconds */
	tv->tv_usec = mega / val;
	tv->tv_usec %= mega;	/* interval in microseconds */
}

static void setup_timer(ethrate_t *rate)
{
	int err;
	struct itimerval timer = {};

	rate_to_tv(rate, &timer.it_interval);
	timer.it_value = timer.it_interval;

	err = setitimer(ITIMER_REAL, &timer, NULL);
	if (err) {
		perror("setitimer");
		exit(0);
	}
}


/*
 * Signal hanlers
 */

static void send_frame(int sugnum)
{
	int err;
	struct timespec ts;

	payload->seq = pktnum;

	err = clock_gettime(CLOCK_REALTIME, &ts);
	if (err == -1) {
		perror("clock_gettime");
		exit(1);
	}
	err = sendmsg(sockfd, &msg, 0);
	if (err == -1) {
		perror("sendmsg");
		exit(1);
	}

	fl_push(&stat, pktnum, &ts);
	++pktnum;
}

static void send_stats(int signum)
{
	fl_send(&stat, master_pipe);
	fl_free(&stat);
}

static void stop(int signum)
{
	INFO("stopping");
	send_stats(signum);
	exit(0);
}

void tx_tstamp()
{
	char control[200];
	struct cmsghdr *i;
	char data[HEADERS_LEN + sizeof(struct payload)];

	struct payload *payload = (struct payload *)(data + HEADERS_LEN);

	struct iovec iov = {
		.iov_base = data,
		.iov_len = sizeof(data)
	};

	struct msghdr msg = {
		.msg_control = control,
		.msg_controllen = sizeof(control),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};

	int err;

	err = recvmsg(sockfd, &msg, MSG_ERRQUEUE);
	if (err == -1) {
		perror("recvmsg");
		ERR("can't recvmsg(ERRQUEUE)");
	}

	struct scm_timestamping *tss = 0;
	struct timespec *soft, *hard, *result;

	for_cmsg(i, &msg)
		if (i->cmsg_level == SOL_SOCKET &&
		    i->cmsg_type == SCM_TIMESTAMPING)
			tss = (struct scm_timestamping *) CMSG_DATA(i);

	if (!tss)
		return;

	soft = tss->ts;
	hard = tss->ts + 2;

	if (!ts_empty(hard))
		result = hard;
	else if (!ts_empty(soft))
		result = soft;
	else
		return;

	if (!result)
		return;

	ENTER_CS;
	fl_push(&stat, payload->seq, result);
	LEAVE_CS;
}

/*
 * Main tx entry point
 */

int tx(header_cfg_t *header, ethrate_t rate,
       unsigned int fsz, unsigned int fid, int out)
{
	master_pipe = out;
	flowid = fid;
	fsize = fsz;
	pktnum = 0;
	fl_clear(&stat);

	setup_sock();
	setup_frame(header);
	setup_signals();
	setup_timer(&rate);

	for(;;)
		pause();

	return 0;
}
