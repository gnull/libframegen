#include <argp.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"

char *rx_ifname, *tx_ifname;

void parse_mac(struct argp_state *state, char *arg, unsigned char mac[ETH_ALEN])
{
	int err;

	assert(ETH_ALEN == 6);
	err = sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		     mac, mac + 1, mac + 2, mac + 3, mac + 4,
		     mac + 5);

	if (err != ETH_ALEN)
		argp_error(state, "invalid MAC address: %s", arg);
}

void parse_ip(struct argp_state *state, char *arg, __be32 *ip)
{
	unsigned char a, b, c, d;
	int err;

	err = sscanf(arg, "%hhu.%hhu.%hhu.%hhu",
	       &a, &b, &c, &d);

	if (err != 4)
		argp_error(state, "invalid IP address: %s", arg);

	*ip = htonl((a << 24) | (b << 16) | (c << 8) | d);
}

void parse_port(struct argp_state *state, char *arg, __be16 *port)
{
	unsigned int s;
	int err;

	err = sscanf(arg, "%u", &s);

	if (err != 1)
		argp_error(state, "invalid UDP port: %s", arg);

	if (s > 1 << 16)
		argp_error(state, "UDP port out of range: %u", s);

	*port = htons(s);
}

void parse_uint(struct argp_state *state, char *arg, unsigned int *uint)
{
	int err;
	err = sscanf(arg, "%u", uint);
	if (err != 1)
		argp_error(state, "invalid uint: %s", arg);
}

void parse_frames(struct argp_state *state, char *arg,
		  unsigned int frames[MAX_FRAMES],
		  unsigned int frames_en[MAX_FRAMES])
{
	const int n = MAX_FRAMES;

	int i;
	char buff[strlen(arg) + 1];

	strcpy(buff, arg);

	char *token;
	token = strtok(buff, ",");

	if (!token)
		argp_error(state, "empty frames list: %s", arg);

	for (i = 0; i < n; ++i) {
		if (!token)
			return;
		parse_uint(state, token, frames + i);
		frames_en[i] = 1;
		token = strtok(NULL, ",");
	}

	if (token)
		argp_error(state, "frames list length exceeds %d", n);
}

int str_to_rate_units(struct argp_state *state, char *str)
{
	int i;
	char *units[] = {
		"%", "KBPS", "MBPS", "GBPS"
	};

	for (i = 0; i < ARRAY_SIZE(units); ++i)
		if (!strcmp(str, units[i]))
			return i;
	argp_error(state, "uknown rate units: %s", str);
	return -1;
}

void parse_rate(struct argp_state *state, char *arg,
		ethrate_t *rate)
{
	int err;
	char buff[20];

	err = sscanf(arg, "%lf%19s", &rate->val, buff);
	if (err != 2)
		argp_error(state, "invalid rate: %s", arg);

	rate->units = str_to_rate_units(state, buff);
}

void parse_rates(struct argp_state *state, char *arg,
		 ethrate_t *rates, int n)
{
	int i;
	char buff[strlen(arg) + 1];

	strcpy(buff, arg);

	char *token;
	token = strtok(buff, ",");

	if (!token)
		argp_error(state, "empty rates list: %s", arg);

	for (i = 0; i < n; ++i) {
		if (!token)
			return;
		parse_rate(state, token, rates + i);
		token = strtok(NULL, ",");
	}

	if (token)
		argp_error(state, "rates list length exceeds %d", n);
}

void parse_switch(struct argp_state *state, char *arg, unsigned int *uint)
{
	if (!strcmp(arg, "on"))
		*uint = 1;
	else if (!strcmp(arg, "off"))
		*uint = 0;
	else
		argp_error(state, "expected on/off, got: %s", arg);
}

void parse_source(struct argp_state *state, char *arg,
		  enum test_rate_source *src)
{
	if (!strcmp(arg, "thr"))
		*src = RATE_THROUGHPUT;
	else if (!strcmp(arg, "manual"))
		*src = RATE_MANUAL;
	else
		argp_error(state, "invalid rate source: %s", arg);
}

enum rfc2544_options {
	/* Interface options */
	opt_rxif = 0x100,
	opt_txif,

	/* Header options */
	opt_smac,
	opt_dmac,
	opt_sip,
	opt_dip,
	opt_sport,
	opt_dport,

	/* Throughput options */
	opt_thr_enabled,
	opt_thr_maxrate,
	opt_thr_duration,
	opt_thr_precision,
	opt_thr_threshold,

	/* Latency options */
	opt_lat_enabled,
	opt_lat_duration,
	opt_lat_trials,
	opt_lat_source,
	opt_lat_rates,

	/* Frameloss options */
	opt_frl_enabled,
	opt_frl_duration,
	opt_frl_steps,
	opt_frl_start,
	opt_frl_stop,

	/* Back-to-back options */
	opt_btb_enabled,
	opt_btb_duration,
	opt_btb_trials,
	opt_btb_source,
	opt_btb_rates,

	/* Frame size option */
	opt_frames,
};

static error_t parser(int key, char *arg, struct argp_state *state)
{
	rfc2544_settings_t *settings;
	header_cfg_t *hdr;
	thr_settings_t *thr;
	lat_settings_t *lat;
	frl_settings_t *frl;
	btb_settings_t *btb;

	settings = state->input;
	hdr = &settings->hdr;
	thr = &settings->thr;
	lat = &settings->lat;
	frl = &settings->frl;
	btb = &settings->btb;

	switch (key) {
	case opt_rxif:
		rx_ifname = malloc(strlen(arg) + 1);
		strcpy(rx_ifname, arg);
		break;
	case opt_txif:
		tx_ifname = malloc(strlen(arg) + 1);
		strcpy(rx_ifname, arg);
		break;
	case opt_smac:
		parse_mac(state, arg, hdr->eth.h_source);
		break;
	case opt_dmac:
		parse_mac(state, arg, hdr->eth.h_dest);
		break;
	case opt_sip:
		parse_ip(state, arg, &hdr->ip.saddr);
		break;
	case opt_dip:
		parse_ip(state, arg, &hdr->ip.daddr);
		break;
	case opt_sport:
		parse_port(state, arg, &hdr->udp.source);
		break;
	case opt_dport:
		parse_port(state, arg, &hdr->udp.dest);
		break;

		/* Throughtput */
	case opt_thr_enabled:
		parse_switch(state, arg, &thr->enabled);
		break;
	case opt_thr_maxrate:
		parse_rate(state, arg, &thr->maxrate);
		break;
	case opt_thr_duration:
		parse_uint(state, arg, &thr->duration);
		break;
	case opt_thr_precision:
		parse_rate(state, arg, &thr->precision);
		break;
	case opt_thr_threshold:
		parse_uint(state, arg, &thr->threshold);
		break;

		/* Latency */
	case opt_lat_enabled:
		parse_switch(state, arg, &lat->enabled);
		break;
	case opt_lat_duration:
		parse_uint(state, arg, &lat->duration);
		break;
	case opt_lat_trials:
		parse_uint(state, arg, &lat->trials);
		break;
	case opt_lat_source:
		parse_source(state, arg, &lat->rate_source);
		break;
	case opt_lat_rates:
		parse_rates(state, arg, lat->rates, MAX_FRAMES);
		break;

		/* Frameloss */
	case opt_frl_enabled:
		parse_switch(state, arg, &frl->enabled);
		break;
	case opt_frl_duration:
		parse_uint(state, arg, &frl->duration);
		break;
	case opt_frl_steps:
		parse_uint(state, arg, &frl->steps);
		break;
	case opt_frl_start:
		parse_rate(state, arg, &frl->start_rate);
		break;
	case opt_frl_stop:
		parse_rate(state, arg, &frl->stop_rate);
		break;

		/* Back-to-back */
	case opt_btb_enabled:
		parse_switch(state, arg, &btb->enabled);
		break;
	case opt_btb_duration:
		parse_uint(state, arg, &btb->duration);
		break;
	case opt_btb_trials:
		parse_uint(state, arg, &btb->trials);
		break;
	case opt_btb_source:
		parse_source(state, arg, &btb->rate_source);
		break;
	case opt_btb_rates:
		parse_rates(state, arg, btb->rates, MAX_FRAMES);
		break;

		/* Frame sizes */
	case opt_frames:
		parse_frames(state, arg, settings->frames,
			     settings->frames_en);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp_option options[] = {
	{.doc = "Interface options"},
	{.name = "rx-if", .key = opt_rxif, .arg = "interface",
	 .doc = "RX interface"},
	{.name = "tx-if", .key = opt_txif, .arg = "interface",
	 .doc = "TX interface"},

	{.doc = "Header options"},
	{.name = "mac-src", .key = opt_smac, .arg = "MAC",
	 .doc = "Source MAC address"},
	{.name = "mac-dst", .key = opt_dmac, .arg = "MAC",
	 .doc = "Destination MAC address"},
	{.name = "ip-src", .key = opt_sip, .arg = "IP",
	 .doc = "Source IP"},
	{.name = "ip-dst", .key = opt_dip, .arg = "IP",
	 .doc = "Destination IP"},
	{.name = "port-src", .key = opt_sport, .arg = "UDP port",
	 .doc = "Source port"},
	{.name = "port-dst", .key = opt_dport, .arg = "UDP port",
	 .doc = "Destination port"},

	{.doc = "Throughput options"},
	{.name = "thr", .key = opt_thr_enabled, .arg="on/off",
	 .doc = "Turn on/off throughtput"},
	{.name = "thr-maxrate", .key = opt_thr_maxrate, .arg = "rate"},
	{.name = "thr-duration", .key = opt_thr_duration, .arg = "uint"},
	{.name = "thr-precision", .key = opt_thr_precision, .arg = "rate"},
	{.name = "thr-threshold", .key = opt_thr_threshold, .arg = "uint"},

	{.doc = "Latency options"},
	{.name = "lat", .key = opt_lat_enabled, .arg = "on/off"},
	{.name = "lat-duration", .key = opt_lat_duration, .arg = "uint"},
	{.name = "lat-trials", .key = opt_lat_trials, .arg = "uint"},
	{.name = "lat-source", .key = opt_lat_source, .arg = "source"},
	{.name = "lat-rates", .key = opt_lat_rates, .arg = "rates"},

	{.doc = "Frameloss options"},
	{.name = "frl", .key = opt_frl_enabled, .arg = "on/off"},
	{.name = "frl-duration", .key = opt_frl_duration, .arg = "uint"},
	{.name = "frl-steps", .key = opt_frl_steps, .arg = "uint"},
	{.name = "frl-start", .key = opt_frl_start, .arg = "rate"},
	{.name = "frl-stop", .key = opt_frl_stop, .arg = "rate"},

	{.doc = "Back-to-back options"},
	{.name = "btb", .key = opt_btb_enabled, .arg = "on/off"},
	{.name = "btb-duration", .key = opt_btb_duration, .arg = "uint"},
	{.name = "btb-trials", .key = opt_btb_trials, .arg = "uint"},
	{.name = "btb-source", .key = opt_btb_source, .arg = "source"},
	{.name = "btb-rates", .key = opt_btb_rates, .arg = "rates"},

	{.doc = "Other options"},
	{.name = "frames", .key = opt_frames, .arg = "uints",
	 .doc = "Comma-separated list frame sizes"},

	{}
};

static struct argp argp = {
	.options = options,
	.parser = parser,
};

int parse_argv(int argc, char **argv, rfc2544_settings_t *settings)
{
	return argp_parse(&argp, argc, argv, 0, NULL, settings);
}
