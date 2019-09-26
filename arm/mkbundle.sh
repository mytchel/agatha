echo -n > bundle.c
echo '#include <types.h>' >> bundle.c
echo '#include "../bundle.h"' >> bundle.c

P=0

P=$(sh arm/bundle.sh \
	bundled_idle \
	bundle.bin \
	bundle.c \
	$P \
	$IDLE)

P=$(sh arm/bundle.sh \
	bundled_procs \
	bundle.bin \
	bundle.c \
	$P \
	$PROCS)

