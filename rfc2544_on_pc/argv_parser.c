#include <librfc2544/rfc2544.h>
#include <libframegen.h>

#include "main.h"

char *rx_ifname, *tx_ifname;

/*
 * Commandline parser
 */

static void parse_mac(char *val, unsigned char mac[ETH_ALEN])
{
	sscanf(val, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
	       mac, mac + 1, mac + 2,
	       mac + 3, mac + 4, mac + 5);
}

static void parse_ip(char *val, __be32 *addr)
{
	char a, b, c, d;
	sscanf(val, "%hhu.%hhu.%hhu.%hhu",
	       &a, &b, &c, &d);

	*addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
}

static void parse_udp(char *val, __be16 *addr)
{
	*addr = htons(atoi(val));
}

FILE *conf_file;

int parse_argv(char **argv, header_cfg_t *hdr)
{
	char *conf_filename = NULL;

	for (; argv[0] && argv[1]; argv += 2) {
		char *key = argv[0];
		char *val = argv[1];

		ifeq(key, "--macsrc")
			parse_mac(val, hdr->eth.h_source);
		else ifeq(key, "--macdst")
			     parse_mac(val, hdr->eth.h_dest);
		else ifeq(key, "--ipsrc")
			     parse_ip(val, &hdr->ip.saddr);
		else ifeq(key, "--ipdst")
			     parse_ip(val, &hdr->ip.daddr);
		else ifeq(key, "--udpsrc")
			     parse_udp(val, &hdr->udp.source);
		else ifeq(key, "--udpdst")
			     parse_udp(val, &hdr->udp.dest);
		else ifeq(key, "--tx_if")
			     tx_ifname = val;
		else ifeq(key, "--rx_if")
			     rx_ifname = val;
		else ifeq(key, "--conf")
			     conf_filename = val;

		else
			return ERR("uknown argument: %s\n", key), 1;
	}

	if (conf_filename) {
		conf_file = fopen(conf_filename, "r");
		if (!conf_file)
			return perror("fopen"), 1;
	} else {
		conf_file = stdin;
	}

	return 0;
}
