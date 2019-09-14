#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mesg.h>
#include <proc0.h>
#include <stdarg.h>
#include <string.h>
#include <dev_reg.h>
#include <log.h>
#include <eth.h>
#include <ip.h>
#include <net_dev.h>
#include <net.h>
#include "net.h"

void
print_mac(char *s, uint8_t *mac)
{
	int i;

	for (i = 0; i < 6; i++) {
		uint8_t l, h;
		
		l = mac[i] & 0xf;
		h = (mac[i] >> 4) & 0xf;
	
		if (h < 0xa) s[i*3] = h + '0';
		else s[i*3] = h - 0xa + 'a';

		if (l < 0xa) s[i*3+1] = l + '0';
		else s[i*3+1] = l - 0xa + 'a';

		s[i*3+2] = ':';
	}

	s[5*3+2] = 0;
}

void
dump_hex_block(uint8_t *buf, size_t len)
{
	char s[33];
	size_t i = 0;
	while (i < len) {
		int j;
		s[0] = 0;
		for (j = 0; j < 8 && i + j < len; j++) {
			uint8_t v = buf[i+j];
			uint8_t l = v & 0xf;
			uint8_t h = (v >> 4) & 0xf;

			if (h < 0xa) s[j*2] = h + '0';
			else s[j*2] = h - 0xa + 'a';

			if (l < 0xa) s[j*2+1] = l + '0';
			else s[j*2+1] = l - 0xa + 'a';
		}

		s[j*2] = 0;

		log(LOG_INFO, "p%x %s", i, s);

		i += j;
	}
}

