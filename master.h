#include <stdio.h>
#include <librfc2544/rfc2544.h>
#include <time.h>

#include "debug.h"

#define for_cmsg(i, msg) for ((i) = CMSG_FIRSTHDR(msg);		\
			      (i); (i) = CMSG_NXTHDR(msg, (i)))

#define HEADERS_LEN\
	(sizeof(struct ethhdr) +\
	 sizeof(struct iphdr) +\
	 sizeof(struct udphdr))

struct payload {
	uint32_t seq;
	unsigned int flowid;
	uint32_t magic;
} __attribute__((packed));

#define MAGIC 0xdeadbeef

struct scm_timestamping {
	struct timespec ts[3];
};

int tx(header_cfg_t *header, ethrate_t ethrate,
       unsigned int fsize, unsigned int flowid, int out);

int rx(unsigned int flowid, int out);


static inline int ts_empty(struct timespec *ts)
{
	return !(ts->tv_sec || ts->tv_nsec);
}
