#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "util.h"

/* Frame list */

void fl_clear(struct flist_head *head)
{
	head->size = 0;
	head->first = head->last = NULL;
}

void fl_alloc(struct flist_head *head, int size)
{
	struct flist_entry *next, *first;
	int i;

	next = NULL;
	for (i = 0; i < size; ++i) {
		first = malloc(sizeof(*first));
		assert(first);
		if (!i)
			head->last = first;
		first->next = next;
		next = first;
	}

	head->first = first;
	head->size = size;
}

void fl_free(struct flist_head *head)
{
	struct flist_entry *i;

	for_list (i, head)
		free(i);

	fl_clear(head);
}

void fl_push(struct flist_head *head, uint32_t id,
	     const struct timespec *ts)
{
	struct flist_entry *new = malloc(sizeof(struct flist_entry));
	assert(new);
	new->fdata.id = id;
	new->fdata.ts = *ts;
	new->next = NULL;

	if (!head->size) {
		head->first = new;
		head->last  = new;
	} else {
		head->last->next = new;
		head->last = new;
	}

	head->size++;
}

/* Make iovec from list */
void fl_iovec(struct flist_head *head, struct iovec *iov)
{
	struct flist_entry *i;

	for_list (i, head) {
		iov->iov_base = &i->fdata;
		iov->iov_len  = sizeof(i->fdata);
		iov++;
	}
}

static inline int min(int a, int b)
{
	return (a > b) ? b : a;
}

int fl_send(struct flist_head *head, int fd)
{
	int err;
	int iovlen = head->size + 1;
	struct iovec iov[iovlen];
	int i;

	iov->iov_base = &head->size;
	iov->iov_len  = sizeof(head->size);

	fl_iovec(head, iov + 1);

	for (i = 0; i < iovlen; i += IOV_MAX) {
		struct iovec *data_to_write;
		int len_to_write;

		data_to_write = iov + i;
		len_to_write = min(IOV_MAX, iovlen - i);

		err = writev(fd, data_to_write, len_to_write);
		if (err == -1)
			return perror("writev"), 1;
	}

	return 0;
}

int fl_recv(int fd, struct flist_head *head)
{
	int err;
	int i;

	int size;
	err = read(fd, &size, sizeof(size));
	if (err == -1)
		return perror("read"), 1;

	struct iovec iov[size];
	fl_alloc(head, size);
	fl_iovec(head, iov);

	for (i = 0; i < size; i += IOV_MAX) {
		struct iovec *data_to_read;
		int len_to_read;

		data_to_read = iov + i;
		len_to_read  = min(IOV_MAX, size - i);

		err = readv(fd, data_to_read, len_to_read);
		if (err == -1)
			return perror("readv"), 1;
	}

	return 0;
}

/* remove item following entry */
static void fl_rm(struct flist_entry *entry)
{
	struct flist_entry *next;

	if (!entry->next)
		return;

	next = entry->next;
	entry->next = entry->next->next;
	free(next);
}

static int ts_cmp(const struct timespec *ts1,
		  const struct timespec *ts2)
{
	return (ts1->tv_sec - ts2->tv_sec)?:(ts1->tv_nsec - ts2->tv_nsec);
}

void fl_uniq(struct flist_head *head)
{
	struct flist_entry *i, *prev;
	struct flist_entry dummy;

	if (!head->size)
		return;

	dummy.next = head->first;
	prev = &dummy;
	i = prev->next;

	while (i && i->next) {
		struct flist_entry *next;

		next = i->next;
		if (i->fdata.id == next->fdata.id) {
			int cmp = ts_cmp(&i->fdata.ts, &next->fdata.ts);
			if (cmp > 0) {
				i = i->next;
				fl_rm(prev);
			} else {
				fl_rm(i);
			}
		} else {
			prev = i;
			i = i->next;
		}
	}
}

void fl_merge(struct flist_head *left, struct flist_head *right,
	      struct flist_head *result)
{
	struct flist_entry entry = {};

	struct flist_entry *i;
	struct flist_entry *l, *r;

	i = &entry;
	l = left->first;
	r = right->first;
	while (l || r) {
		struct flist_entry **min = NULL;

		if (!l)
			min = &r;
		else if (!r)
			min = &l;
		else if (l->fdata.id < r->fdata.id)
			min = &l;
		else
			min = &r;

		i->next = *min;
		i = i->next;
		*min = (*min)->next;
	}

	i->next = NULL;
	result->first = entry.next;
	result->last = i;
	result->size = left->size + right->size;
}

struct flist_entry *fl_ind(struct flist_entry *pos, int ind)
{
	int i;
	for(i = 0; i < ind; ++i)
		pos = pos->next;

	return pos;
}

void fl_sort(struct flist_head *head)
{
	if (head->size == 1 ||
	    head->size == 0)
		return;
	assert(head->size > 0);
	int left_size, right_size;
	struct flist_entry *left_end, *right_begin;

	left_size = head->size >> 1;
	right_size = head->size - left_size;

	left_end = fl_ind(head->first, left_size - 1);
	right_begin = left_end->next;

	left_end->next = NULL;

	struct flist_head left = {
		.size = left_size,
		.first = head->first,
		.last = left_end
	};
	struct flist_head right = {
		.size = right_size,
		.first = right_begin,
		.last = head->last
	};

	fl_sort(&left);
	fl_sort(&right);

	fl_merge(&left, &right, head);
}

/* return ts1 - ts2 */
static double ts_sub(struct timespec *ts1, struct timespec *ts2)
{
	double res = (double)(ts1->tv_sec - ts2->tv_sec) +
		(double)1e-9 * (ts1->tv_nsec - ts2->tv_nsec);
	return res;
}

double fl_latency(struct flist_head *rx, struct flist_head *tx)
{
	struct flist_entry *rx_it, *tx_it;

	double lat = 0;

	rx_it = rx->first;
	tx_it = tx->first;

	while (rx_it && tx_it) {
		uint32_t rx_id, tx_id;
		rx_id = rx_it->fdata.id;
		tx_id = tx_it->fdata.id;
		if (tx_id == rx_id) {
			lat += ts_sub(&rx_it->fdata.ts, &tx_it->fdata.ts);

			rx_it = rx_it->next;
			tx_it = tx_it->next;
			continue;
		}

		if (tx_id < rx_id)
			tx_it = tx_it->next;
		else
			rx_it = rx_it->next;
	}

	return lat;
}

int fl_recv_append(int fd, struct flist_head *head)
{
	int err;
	struct flist_head tmp;

	err = fl_recv(fd, &tmp);
	if (err)
		return err;

	fl_sort(&tmp);
	fl_merge(&tmp, head, head);

	return 0;
}
