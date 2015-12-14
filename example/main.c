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

	for (i = 0; i < 20; ++i) {
		uint32_t tx, rx;
		int err;

		sleep(1);

		err = handler.tx.get_stat(&tx);
		if (err) {
			ERR("can't get tx stats");
			return 1;
		}

		err = handler.rx.get_stat(&rx, NULL);
		if (err) {
			ERR("can't get rx stats");
			return 1;
		}

		INFO("\tStatistics (tx/rx): %u/%u", tx, rx);
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
