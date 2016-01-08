#include <stdio.h>
#include <stdlib.h>

#include <librfc2544/rfc2544.h>

#include "main.h"

static int update_cb(enum rfc2544_test test,
		     rfc2544_data_t *data, void *context)
{
	printf("update_cb\n");
	return 0;
}

static int get_status_cb(void *context)
{
	return 0;
}

int main(int argc, char **argv)
{
	rfc2544_settings_t settings = {};
	int err;

	err = parse_argv(argv + 1, &settings.hdr);
	if (err)
		return err;

	err = parse_conf(&settings);
	if (err)
		return err;

	rfc2544_stat_handler_t stat = {
		.update_cb = update_cb,
		.get_status_cb = get_status_cb,
	};

	err = rfc2544(&settings, &stat, NULL);
	if (err)
		printf("error!\n");
	else
		printf("success!\n");
}
