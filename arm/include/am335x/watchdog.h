
struct watchdog {
	uint32_t widr;
	uint32_t pad1[3];
	uint32_t wdsc;
	uint32_t wdst;
	uint32_t wisr;
	uint32_t wier;
	uint32_t pad2[1];
	uint32_t wclr;
	uint32_t wcrr;
	uint32_t wldr;
	uint32_t wtgr;
	uint32_t wwps;
	uint32_t pad3[3];
	uint32_t wdly;
	uint32_t wspr;
	uint32_t pad4[2];
	uint32_t wirqstatraw;
	uint32_t wirqstat;
	uint32_t wirqenset;
	uint32_t wireqenclr;
};

