#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>

#include "proc0.h"

static struct frame *free = nil;

/* TODO: Get the kernel to give this information
	 somehow. Also have a list of l2 pages. */
static uint32_t *l1 = (uint32_t *) 0x1000;
static uint32_t *l2 = (uint32_t *) 0x5000;

struct frame *
get_frame(void)
{
	static struct frame init_frames[32] = { 0 };
	static int next_frame = 0;
	static int nframes = 32;

	struct frame *f;

	f = &init_frames[next_frame++];

	if (next_frame == nframes) {
		/* TODO */
		raise();
	}

	return f;
}

void
init_mem(void)
{
	struct frame *f;

	f = get_frame();
	f->pa  = 0x80000000;
	f->len =  0x2000000;
	f->pid = -1;
	f->va = 0;
	f->next = free;
	free = f;

	f = get_frame();
	f->pa  = 0x83000000;
	f->len = 0x1d000000;
	f->pid = -1;
	f->va = 0;
	f->next = free;
	free = f;
}

	struct frame *
frame_split(struct frame *f, size_t off)
{
	struct frame *n;

	n = get_frame();
	if (n == nil) {
		return nil;
	}

	n->pa = f->pa + off;
	n->len = f->len - off;

	n->va = 0;
	n->pid = -1;
	n->next = nil;

	f->len = off;

	return n;
}

	struct frame *
get_mem_frame(size_t len, size_t align)
{
	struct frame *f, *n;
	size_t a;

	for (f = free; f != nil; f = f->next) {
		a = (f->pa + align - 1) & ~(align - 1);

		if (len < a - f->pa + f->len) {
			if (f->pa < a) {
				f = frame_split(f, a - f->pa);
				if (f == nil) 
					return nil;
			}

			n = frame_split(f, len);

			n->next = free;
			free = n;

			return f;
		}
	}

	return nil; 
}

	size_t
get_mem(size_t l, size_t align)
{
	struct frame *f;

	f = get_mem_frame(l, align);
	if (f == nil) {
		return 0;
	}

	return f->pa;
}

	void
free_mem(size_t a, size_t l)
{
	/* TODO */
}

/* TODO: just improve this. */

	void *
map_free(size_t pa, size_t len, int ap, bool cache)
{
	size_t i, j;

	/* TODO: should be able to find first page but don't
		 want it to do that if l2 is at 0. */

	i = 1; 
	while (true) {
		if (i == 0x1000 / sizeof(uint32_t)) {
			return nil;
		}

		if (l2[i] == 0) {
			for (j = 1; j * PAGE_SIZE < len; j++) {
				if (i + j == 0x1000 / sizeof(uint32_t)) {
					return nil;

				} else if (l2[i+j] != 0) {
					i += j;
					goto too_small;
				}
			}

			break;
		}

too_small:
		i++;
	}

	map_pages(l2, pa, i * PAGE_SIZE, len, ap, cache);

	return (void *) (i * PAGE_SIZE);
}

	void
unmap(void *va, size_t len)
{
	unmap_pages(l2, (size_t) va, len);
}

	void
init_l1(uint32_t *t)
{
	memset(&t[0],     0, 0x800 * sizeof(uint32_t));
	memcpy(&t[0x800], &l1[0x800], 0x100 * sizeof(uint32_t)); 
	memset(&t[0x900], 0, 0x700 * sizeof(uint32_t));
}

