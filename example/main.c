#include "../export.h"
#include "../master.h"

#include "unistd.h"

char *tx_ifname = "wlan0";
char *rx_ifname = "wlan0";

int main()
{
	int i;
	rfc2544_ctrl_handler_t handler;

	init_ctrl_handler(&handler, NULL);

	ethrate_t rate = {
		.units = RATE_MBPS,
		.val = 1
	};

	header_cfg_t header = {
		.eth = {
			.h_dest = {0x60, 0xd8, 0x19, 0x6b, 0x01, 0xc5},
			.h_source = {0x60, 0xd8, 0x19, 0x6b, 0x01, 0xc6},
			.h_proto = htons(ETH_P_IP)
		},
		.ip = {
			.saddr = htonl(192 << 24 | 168 << 16 | 1 << 8 | 123),
			.daddr = htonl(192 << 24 | 168 << 16 | 1 << 8 | 132),
			.ttl = 123,
			.id = 1,
			.check = 0
		}
	};

	handler.tx.conf_flowid(0);
	handler.tx.conf_framesize(1000);
	handler.tx.conf_rate(rate);
	handler.tx.conf_header(&header);

	handler.rx.conf_flowid(0);

	if (handler.tx.start()) {
		ERR("can't start tx");
		return 1;
	}

	if (handler.rx.start()) {
		ERR("can't start rx");
		return 1;
	}

	for (i = 0; i < 10; ++i) {
		uint32_t tx, rx;
		double lat;
		int err;

		sleep(1);

		INFO("getting tx stats");

		err = handler.tx.get_stat(&tx);
		if (err) {
			ERR("can't get tx stats");
			return 1;
		}

		INFO("getting rx stats");

		err = handler.rx.get_stat(&rx, &lat);
		if (err) {
			ERR("can't get rx stats");
			return 1;
		}

		INFO("\tStatistics (tx/rx/lat): %u / %u / %f", tx, rx, lat);
	}

	if (handler.tx.stop()) {
		ERR("can't stop tx");
		return 1;
	}

	if (handler.rx.stop()) {
		ERR("can't stop rx");
		return 1;
	}

	return 0;
}
