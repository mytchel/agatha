#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>

void
memcpy(void *dst, const void *src, size_t len)
{
	const uint8_t *s;
	uint8_t *d;
	
	d = dst;
	s = src;
	
	while (len-- > 0) {
		*d++ = *s++;
	}
}

void
memset(void *dst, uint8_t v, size_t len)
{
	uint8_t *d = dst;
	size_t i;
		
	for (i = 0; i < len; i++)
		d[i] = v;
}

