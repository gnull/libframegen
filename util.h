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

/* Compute latency by given rx/tx stats */
double fl_latency(struct flist_head *rx, struct flist_head *tx);

/* Free memory used by the list */
void fl_free(struct flist_head *head);

