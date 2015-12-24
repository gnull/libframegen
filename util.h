/* Without this limits.h do not define IOV_MAX */
#define __USE_XOPEN
#include <limits.h>

/*
 * The data, that will be
 * stored in lists/arrays
 */

struct fdata {
	uint32_t id;
	struct timespec ts;
};


/*
 * Frame list implementation
 */

struct flist_entry {
	struct flist_entry *next;
	struct fdata fdata;
};

struct flist_head {
	struct flist_entry *first, *last;
	int size;
};

#define for_list(i, head) for ((i) = (head)->first; (i); (i) = (i)->next)

/* Create empty list */
void fl_clear(struct flist_head *head);

/* Allocate list of size elements */
void fl_alloc(struct flist_head *head, int size);

/* Push new item to the end of the list */
void fl_push(struct flist_head *head, uint32_t id, const struct timespec *ts);

/* Send the whole list to fd */
int fl_send(struct flist_head *head, int fd);

/* Recv list from fd */
int fl_recv(int fd, struct flist_head *head);

/* Merge two sorted lists */
void fl_merge(struct flist_head *left, struct flist_head *right,
	      struct flist_head *result);

/* Sort list */
void fl_sort(struct flist_head *head);

/* Remove duplicates id's from the list */
void fl_uniq(struct flist_head *head);

/* Compute latency by given rx/tx stats */
double fl_latency(struct flist_head *rx, struct flist_head *tx);

/* Receive new items from fd and append them to sorted list */
int fl_recv_append(int fd, struct flist_head *head);

/* Free memory used by the list */
void fl_free(struct flist_head *head);

