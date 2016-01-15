#include <stdio.h>
#include <stdlib.h>

#include "main.h"

static char *test_name[] = {
	"throughput", "latency",
	"frameloss", "back-to-back"
};

static char *status_name[] = {
	"TEST_PENDING",
	"TEST_RUNNING",
	"TEST_PASSED",
	"TEST_CANCELLED",
	"TEST_FAILED",
	"TEST_UNAVAIL",
	"TEST_UNKNOWN"
};

void pprint_stat(trial_stat_t *stat)
{
	printf("%lld/%lld/%f", stat->rx, stat->tx, stat->lat);
}

void pprint_thr(thr_data_t *thr,
		unsigned frames[MAX_FRAMES], unsigned frames_nr)
{
	int i;

	printf("maxrate = %f\n", thr->maxrate);

	printf("# framesize\t" "result\t" "trial_stat\t"
	       "status\n");

	for (i = 0; i < frames_nr; ++i) {
		printf("%u\t", frames[i]);
		printf("%f\t", thr->res[i]);
		pprint_stat(thr->trial_stat + i);
		printf("\t%s\n", status_name[thr->status[i]]);
	}
}

void pprint_lat(lat_data_t *lat,
		unsigned frames[MAX_FRAMES], unsigned frames_nr)
{
	int i;

	printf("# framesize\t" "result\t" "rate\t" "trial_stat\t"
	       "status\n");

	for (i = 0; i < frames_nr; ++i) {
		printf("%u\t%f\t%f\t", frames[i],
		       lat->res[i], lat->rate[i]);
		pprint_stat(lat->trial_stat + i);
		printf("\t%s\n", status_name[lat->status[i]]);
	}
}

void pprint_frl(frl_data_t *frl,
		unsigned frames[MAX_FRAMES], unsigned frames_nr)
{
	int i, j;
	double rate;

	printf("status:\n");
	for (i = 0; i < frames_nr; ++i)
		printf("%u\t%s\n", frames[i],
		       status_name[frl->status[i]]);

	printf("result:\n");
	rate = frl->start_rate;
	for (i = 0; i < frl->cnt; ++i) {
		printf("\t%fMBPS", rate);
		rate += frl->step_rate;
	}

	printf("\n");

	for (i = 0; i < frames_nr; ++i) {
		printf("%u:\t", frames[i]);
		for (j = 0; j < frl->cnt; ++j)
			printf("%f\t", frl->v[i][j]);
		printf("\n");
	}

	printf("statistics:\n");
	for (i = 0; i < frames_nr; ++i) {
		printf("%u:\t", frames[i]);
		for (j = 0; j < frl->cnt; ++j) {
			pprint_stat(&frl->trial_stat[i][j]);
			printf("\t");
		}
		printf("\n");
	}
}

void pprint_btb(btb_data_t *btb,
		unsigned frames[MAX_FRAMES], unsigned frames_nr)
{
	int i;
	printf("# framesize\t" "result\t"
	       "rate\t" "trial_stat\t" "status\n");

	for (i = 0; i < frames_nr; ++i) {
		printf("%u\t" "%f\t" "%f\t",
		       frames[i], btb->res[i], btb->rate[i]);

		pprint_stat(btb->trial_stat + i);
		printf("\t%s\n", status_name[btb->status[i]]);
	}
}

static void pprint_test(rfc2544_data_t *data,
			enum rfc2544_test test,
			unsigned frames_nr)
{
	unsigned *frames = data->set.frames;

	printf("%s:\n"
	       "-------------------\n", test_name[test]);

	switch (test) {
	case TEST_THROUGHPUT:
		return pprint_thr(&data->thr, frames, frames_nr);
	case TEST_LATENCY:
		return pprint_lat(&data->lat, frames, frames_nr);
	case TEST_FRAMELOSS:
		return pprint_frl(&data->frl, frames, frames_nr);
	case TEST_BACK_TO_BACK:
		return pprint_btb(&data->btb, frames, frames_nr);
	}
}

static test_status_t *get_status(rfc2544_data_t *data,
				 enum rfc2544_test test)
{
	switch (test) {
	case TEST_THROUGHPUT:
		return data->thr.status;
	case TEST_LATENCY:
		return data->lat.status;
	case TEST_FRAMELOSS:
		return data->frl.status;
	case TEST_BACK_TO_BACK:
		return data->btb.status;
	default:
		ERR("uknown test_number: %d", test);
		exit(1);
	}

	printf("\n\n");
}

static int test_finished(rfc2544_data_t *data,
			 enum rfc2544_test test,
			 unsigned frames_nr)
{
	test_status_t *status;
	int i;

	status = get_status(data, test);

	for (i = 0; i < frames_nr; ++i)
		if (status[i] != TEST_PASSED)
			return 0;
	return 1;
}

static void progress(rfc2544_data_t *data,
		     enum rfc2544_test test,
		     unsigned frames_nr)
{
	static char *prog[] = {
		"-", "\\", "|", "/"
	};

	int i;
	test_status_t *status;

	status = get_status(data, test);

	static int it = 0;
	fprintf(stderr, "[%s] %s [", prog[it], test_name[test]);
	it = (it + 1) % ARRAY_SIZE(prog);

	for (i = 0; i < frames_nr; ++i) {
		char c;
		switch (status[i]) {
		case TEST_PENDING:
			c = ' ';
			break;
		case TEST_RUNNING:
			c = 'R';
			break;
		case TEST_PASSED:
			c = '#';
			break;
		default:
			c = 'E';
		}
		fprintf(stderr, "%c", c);
	}
	fprintf(stderr, "]");
}

static int update_cb(enum rfc2544_test test,
		     rfc2544_data_t *data, void *context)
{
	static int last_test = -1;
	unsigned frames_nr = *(unsigned *)context;

	/* Костыль, ибо librfc косячит */
	data->frl.cnt = data->set.frl.steps;

	fprintf(stderr, "\r%50s\r", "");

	if (last_test == -1) {
		last_test = test;
	} else if (test_finished(data, test, frames_nr)) {
		pprint_test(data, test, frames_nr);
		return 0;
	}

	progress(data, test, frames_nr);

	return 0;
}

static int get_status_cb(void *context)
{
	return 0;
}

static unsigned int sum(unsigned int *ar, int n)
{
	int i;
	unsigned int result = 0;

	for (i = 0; i < n; ++i)
		result += !!ar[i];

	return result;
}

int main(int argc, char **argv)
{
	rfc2544_settings_t settings = {
		.hdr = {
			.eth = { .h_proto = htons(ETH_P_IP) },
			.ip  = {
				.version = 4,
				.ihl = 5,
				.ttl = 64,
				.protocol = 0x11, /* UDP */
			},
			.udp = {
			}
		}
	};
	int err;

	err = parse_argv(argc, argv, &settings);
	if (err)
		return err;

	rfc2544_stat_handler_t stat = {
		.update_cb = update_cb,
		.get_status_cb = get_status_cb,
	};

	unsigned int frames_nr = sum(settings.frames_en, MAX_FRAMES);

	err = rfc2544(&settings, &stat, &frames_nr);
	if (err)
		printf("error!\n");
	else
		printf("success!\n");
	return 0;
}
