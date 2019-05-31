#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>

void
memcpy(uint8_t *d, const uint8_t *s, size_t len)
{
	while (len-- > 0) {
		*d++ = *s++;
	}
}

void
memset(uint8_t *d, uint8_t v, size_t len)
{
	size_t i;
		
	for (i = 0; i < len; i++)
		d[i] = v;
}

bool
memcmp(const uint8_t *a, const uint8_t *b, size_t len)
{
	while (len-- > 0) {
		if (*a++ != *b++) return false;
	}

	return true;
}

