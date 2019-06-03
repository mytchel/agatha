#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>

void
memcpy(void *dd, const void *ss, size_t len)
{
	uint8_t *d = dd;
	const uint8_t *s = ss;

	while (len-- > 0) {
		*d++ = *s++;
	}
}

void
memset(void *dd, uint8_t v, size_t len)
{
	uint8_t *d = dd;
	size_t i;
		
	for (i = 0; i < len; i++)
		d[i] = v;
}

bool
memcmp(const void *aa, const void *bb, size_t len)
{
	const uint8_t *a = aa, *b = bb;

	while (len-- > 0) {
		if (*a++ != *b++) return false;
	}

	return true;
}

