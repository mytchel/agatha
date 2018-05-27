#include <types.h>
#include <mach.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <stdarg.h>
#include <string.h>

#include "proc0.h"
#include "../dev.h"

/* TODO: Get the kernel to give this information
	 somehow. Also have a list of l2 pages. */
static uint32_t *l1 = (uint32_t *) 0x1000;
static uint32_t *l2 = (uint32_t *) 0x5000;

struct mem_container {
	size_t pa, len;
	struct mem_container *next;
};

/* Free container structs. */
static struct mem_container *mem_container_free = nil;
/* Memory that is free to be used. */
static struct mem_container *mem = nil;

	static struct mem_container *
get_mem_container(void)
{
	static bool getting_more = false;
	struct mem_container *m;
	size_t pa;
	void *va;

	if (mem_container_free->next == nil && !getting_more) {
		getting_more = true;

		pa = get_mem(0x1000, 0x1000);
		if (pa == nil) {
			raise();
		}

		va = map_free(pa, 0x1000, AP_RW_RW, true);
		if (va == nil) {
			raise();
		}

		for (m = va; (size_t) m < (size_t) va + 0x1000; m++) {
			m->next = mem_container_free;
			mem_container_free = m;
		}
		
		getting_more = false;
	}

	m = mem_container_free;
	mem_container_free = m->next;

	return m;
}

static void
free_mem_container(struct mem_container *m)
{
	m->next = mem_container_free;
	mem_container_free = m;

	/* TODO: free pages somehow. */
}

void
init_mem(void)
{
	static struct mem_container init_mem[32] = { 0 };
	struct mem_container *m;
	int i;

	for (i = 0; i < LEN(init_mem); i++) {
		m = &init_mem[i];
		m->next = mem_container_free;
		mem_container_free = m;
	}

	for (i = 0; i < ndevices; i++) {
		if (!strncmp(devices[i].compatable, "mem", 
					sizeof(devices[i].compatable)))
			continue;

		m = get_mem_container();
		m->pa  = devices[i].reg;
		m->len = devices[i].len;
		m->next = mem;
		mem = m;
	}
}

static struct mem_container *
mem_container_split(struct mem_container *f, size_t off)
{
	struct mem_container *n;

	n = get_mem_container();
	if (n == nil) {
		return nil;
	}

	n->pa = f->pa + off;
	n->len = f->len - off;

	n->next = nil;

	f->len = off;

	return n;
}

static struct mem_container *
get_free_mem(size_t len, size_t align)
{
	struct mem_container *f, *n;
	size_t a;

	for (f = mem; f != nil; f = f->next) {
		a = (f->pa + align - 1) & ~(align - 1);

		if (len < a - f->pa + f->len) {
			if (f->pa < a) {
				f = mem_container_split(f, a - f->pa);
				if (f == nil) 
					return nil;
			}

			n = mem_container_split(f, len);

			n->next = mem;
			mem = n;

			return f;
		}
	}

	return nil; 
}

	size_t
get_mem(size_t l, size_t align)
{
	struct mem_container *m;
	size_t pa;

	m = get_free_mem(l, align);
	if (m == nil) {
		return 0;
	}

	pa = m->pa;

	free_mem_container(m);

	return pa;
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
	size_t s;

	memset(&t[0], 0, 0x4000);

	s = info->kernel_start >> 20;
	memcpy(&t[s], &l1[s], info->kernel_len); 
}

static struct slab *slabs = nil;

struct slab *
slab_new(size_t obj_size)
{
	struct slab *s, *n;
	size_t pa;

	if (slabs == nil) {
		pa = get_mem(0x1000, 0x1000);
		if (pa == nil) {
			return nil;
		}

		n = map_free(pa, 0x1000, AP_RW_RW, true);
		if (n == nil) {
			free_mem(pa, 0x1000);
			return nil;
		}

		for (s = n; (size_t) (s + 1) <= (size_t) n + 0x1000; s++) {
			s->next = slabs;
			slabs = s;
		}
	}

	s = slabs;
	slabs = s->next;

	s->next = nil;
	s->head = nil;
	s->obj_size = obj_size;
	s->nobj = 16 * 8;
	s->frame_size = 
		PAGE_ALIGN(sizeof(struct slab_frame) 
				+ s->nobj / 8
				+ s->obj_size * s->nobj);

	return s;
}

static size_t
slab_alloc_from_frame(struct slab *s, struct slab_frame *f)
{
	size_t *u;
	int i;

	for (i = 0; i < s->nobj; i++) {
		u = &f->use[i / sizeof(size_t)];
		if (!(*u & (1 << (i % sizeof(size_t))))) {
			*u |=	(1 << (i % sizeof(size_t)));
			return (size_t) &f->use[s->nobj] + s->obj_size * i;
		}
	}	

	return nil;
}

	void *
slab_alloc(struct slab *s)
{
	struct slab_frame **f;
	size_t va, pa;

	for (f = &s->head; *f != nil; f = &(*f)->next) {
		va = slab_alloc_from_frame(s, *f);
		if (va != nil) {
			return (void *) va;
		}
	}

	pa = get_mem(s->frame_size, 0x1000);
	if (pa ==  nil) {
		return nil;
	}

	*f = map_free(pa, s->frame_size, AP_RW_RW, true);
	if (*f == nil) {
		free_mem(pa, 0x1000);
		return nil;
	}

	(*f)->next = nil;
	(*f)->pa = pa;
	(*f)->len = s->frame_size;

	memset((*f)->use, 0, s->nobj / 8);

	return (void *) slab_alloc_from_frame(s, *f);
}

	void
slab_free(struct slab *s, void *p)
{

}


