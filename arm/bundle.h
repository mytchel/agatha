struct bundle_proc {
	char name[256];
	size_t len;
};

extern struct bundle_proc bundled_procs[];
extern size_t nbundled_procs;
extern struct bundle_proc bundled_idle[];
extern size_t nbundled_idle;
extern struct bundle_proc bundled_drivers[];
extern size_t nbundled_drivers;

