#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <librfc2544/rfc2544.h>

#include "main.h"

int get_token(char *buff, int size)
{
	char fmt[50];
	int err;

	snprintf(fmt, sizeof(fmt), "%%%ds ", size - 1);

	err = fscanf(conf_file, fmt, buff);
	if (err == EOF)
		if (errno) {
			perror("fscanf");
			exit(1);
		} else {
			return 1;
		}

	assert(err == 1);
	return 0;
}

char *rate_units_str[] = {
	"%", "Kbps", "Mbps", "Gbps",
};

int parse_ethrate(ethrate_t *rate)
{
	char units[20];
	int err;
	int i;

	err = fscanf(conf_file, "%lf", &rate->val);
	if (err == 0)
		return ERR("can't get rate->val"), 1;
	else if (err == EOF)
		return ERR("unexpected EOF"), 1;

	if (get_token(units, sizeof(units)))
		return ERR("unexpected EOF"), 1;

	int found = 0;

	for (i = 0; i < ARRAY_SIZE(rate_units_str); ++i)
		ifeq(units, rate_units_str[i]) {
			rate->units = i;
			found = 1;
			break;
		}

	if (!found)
		return ERR("uknown rate units: '%s'", units), 1;

	return 0;
}

int parse_uint(unsigned int *val)
{
	int err;

	err = fscanf(conf_file, "%u", val);
	if (err == 0)
		ERR("can't get uint");
	else if (err == EOF)
		ERR("unexpected EOF");

	return err != 1;
}

int parse_thr(thr_settings_t *thr)
{
	char buff[100];
	int err;

	thr->enabled = 1;

	err = 0;
	while (!err) {
		if (get_token(buff, sizeof(buff))) {
			ERR("unexpected EOF");
			return 1;
		}

		ifeq(buff, "maxrate")
			err = parse_ethrate(&thr->maxrate);
		else ifeq(buff, "duration")
			     err = parse_uint(&thr->duration);
		else ifeq(buff, "precision")
			     err = parse_ethrate(&thr->precision);
		else ifeq(buff, "threshold")
			     err = parse_uint(&thr->threshold);
		else ifeq(buff, ";")
			     break;
		else
			return ERR("invalid token: '%s'", buff), 1;
	}

	return err;
}

int parse_lat(lat_settings_t *lat)
{
	return 0;
}

int parse_frl(frl_settings_t *frl)
{
	return 0;
}

int parse_btb(btb_settings_t *btb)
{
	return 0;
}

int isalldigit(char *s)
{
	int i;
	for (i = 0; s[i]; ++i)
		if (!isdigit(s[i]))
			return 0;
	return 1;
}

const char frames_delim[] = " \t\n";
int parse_frames(unsigned int frames[MAX_FRAMES],
		 unsigned int frames_en[MAX_FRAMES])
{
	char buff[100];
	int err;

	if (!fgets(buff, sizeof(buff), conf_file)) {
		if (ferror(conf_file))
			perror("fgets");
		else
			ERR("unexpected EOF"), 1;
		return 1;
	}

	char *token = strtok(buff, frames_delim);
	int i;
	for (i = 0; i < MAX_FRAMES && token; ++i) {
		if (!isalldigit(token))
			return ERR("token contains invalid chars: '%s'", token), 1;
		frames[i] = atoi(token);
		frames_en[i] = 1;

		token = strtok(NULL, frames_delim);
	}

	if (token)
		ERR("Too many tokens, ignoring: %s %s", token, strtok(NULL, ""));

	err = get_token(buff, sizeof(buff));
	if (err)
		return ERR("failed to read ';'"), 1;

	ifeq(buff, ";")
		return 0;
	else
		return ERR("got '%s' instead of ';'", buff), 1;
}

int parse_conf(rfc2544_settings_t *set)
{
	char buff[100];
	int err;

	err = 0;
	while (!err) {
		if (get_token(buff, sizeof(buff)))
			return 0;

		ifeq(buff, "thr")
			err = parse_thr(&set->thr);
		else ifeq(buff, "lat")
			     err = parse_lat(&set->lat);
		else ifeq(buff, "frl")
			     err = parse_frl(&set->frl);
		else ifeq(buff, "btb")
			     err = parse_btb(&set->btb);
		else ifeq(buff, "frames")
			     err = parse_frames(set->frames, set->frames_en);
		else
			return ERR("invalid config token: '%s'", buff);
	}
}
