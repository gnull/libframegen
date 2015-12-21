#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>

#include "../util.h"
#include "../master.h"

void print_list(char *str, struct flist_head *head)
{
	struct flist_entry *i;

	printf("%s: ", str);
	for_list(i, head)
		printf("%d, ", i->fdata.id);

	printf("\n");
}

void child(int fd)
{
	struct timespec ts;
	struct flist_head head, head2;
	fl_clear(&head);
	fl_clear(&head2);

	fl_push(&head, 0, &ts);
	fl_push(&head, 2, &ts);
	fl_push(&head, 0, &ts);
	fl_push(&head, 3, &ts);
	fl_push(&head, 1, &ts);
	fl_push(&head, 3, &ts);

	print_list("tx1", &head);
	fl_send(&head, fd);

	fl_push(&head2, 1, &ts);
	fl_push(&head2, 0, &ts);
	fl_push(&head2, 4, &ts);
	fl_push(&head2, 2, &ts);

	print_list("tx2", &head2);
	fl_send(&head2, fd);

	fl_sort(&head);
	print_list("sort(tx2)", &head);

	fl_sort(&head2);
	print_list("sort(tx2)", &head2);

	fl_merge(&head, &head2, &head);
	print_list("tx_merged:", &head);
}

void parent(int fd)
{
	struct flist_head all;
	struct flist_head head, head2;

	fl_clear(&all);

	fl_recv(fd, &head);
	print_list("rx1", &head);

	fl_recv(fd, &head2);
	print_list("rx2", &head2);

	fl_sort(&head);
	fl_merge(&all, &head, &all);
	print_list("rx1_merged", &all);

	fl_sort(&head2);
	fl_merge(&all, &head2, &all);
	print_list("rx2_merged", &all);
}

int main() {
	int fd[2];

	pipe(fd);

	if (fork()) {
		parent(fd[0]);
		wait(NULL);
	} else {
		child(fd[1]);
	}

	return 0;
}
