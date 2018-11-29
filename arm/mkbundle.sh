echo -n > bundle.c
echo '#include <types.h>' >> bundle.c
echo '#include "../bundle.h"' >> bundle.c

P=$(sh ../bundle.sh \
	bundled_procs \
	bundle.bin \
	bundle.c \
	0 \
	$PROCS)

P=$(sh ../bundle.sh \
	bundled_drivers \
	bundle.bin \
	bundle.c \
	$P \
	$DRIVERS)

