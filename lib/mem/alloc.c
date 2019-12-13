#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <mmu.h>
#include <log.h>

/* TODO : release memory when it is no longer used */

#define CHUNK_SIZE(B)    (1<<(B+5))

#define BIN_MAX    7

struct buffer {
	size_t size;
};

struct chunk {
	size_t size;
	struct chunk *next;
};

static struct chunk *bins[BIN_MAX+1] = { 0 };

static void
insert_chunk(size_t bin, struct chunk *c)
{
	struct chunk **p;
	bool a;

	for (p = &bins[bin]; *p != nil; p = &(*p)->next) {
		if (bin < BIN_MAX) {
			a = (size_t) c == align_up((size_t) c, CHUNK_SIZE(bin + 1));
			
			if (a && (size_t) c + CHUNK_SIZE(bin) == (size_t) (*p)->next) {
				
				(*p)->next = (*p)->next->next;

				insert_chunk(bin + 1, c);
				return;

			} else if (!a && (size_t) (*p) + CHUNK_SIZE(bin) == (size_t) c) {

				c = *p;
				*p = (*p)->next;

				insert_chunk(bin + 1, c);
				return;
			}
		}
		
		if ((size_t) c < (size_t) (*p)) {
			break;
		}
	}

	c->next = *p;
	*p = c;
}

	static struct chunk *
get_chunk(size_t bin)
{
	struct chunk *c, *o;

	if (bins[bin] != nil) {
		c = bins[bin];
		bins[bin] = c->next;

		c->size = CHUNK_SIZE(bin);
		c->next = nil;
		return c;
	}

	if (bin < BIN_MAX) {
		c = get_chunk(bin + 1);
		if (c == nil) {
			return nil;
		}

		o = (struct chunk *) (((uint8_t *) c) + CHUNK_SIZE(bin));
		o->size = 0;

		insert_chunk(bin, o);

	} else {
		size_t len = PAGE_ALIGN(CHUNK_SIZE(bin));
		int fid;

		fid = request_memory(len, 0x1000);
		if (fid < 0) {
			return nil;
		}

		c = frame_map_anywhere(fid);
		if (c == nil) {
			release_memory(fid);
			return nil;
		}

		size_t i;
		for (i = CHUNK_SIZE(bin); i < len; i += CHUNK_SIZE(bin)) {
			o = (struct chunk *) (((size_t) c) + i);
			o->size = 0;
			insert_chunk(bin, o);
		}
	}

	c->size = CHUNK_SIZE(bin);
	c->next = nil;

	return c;
}

	static void
free_chunk(size_t bin, struct chunk *c)
{
	c->size = 0;

	insert_chunk(bin, c);
}

	static size_t
bin_n(size_t len)
{
	size_t bin;

	for (bin = 0; bin <= BIN_MAX && CHUNK_SIZE(bin) < len; bin++)
		;

	return bin;
}

	void *
malloc(size_t len)
{
	struct buffer *b;
	struct chunk *c;
	size_t bin;

	bin = bin_n(len + sizeof(size_t));

	if (bin <= BIN_MAX) {
		c = get_chunk(bin);
		if (c == nil) {
			return nil;
		}

		return (void *) (((size_t) c) + sizeof(size_t));

	} else {
		len = PAGE_ALIGN(len + sizeof(struct buffer));
		int fid;

		fid = request_memory(len, 0x1000);
		if (fid < 0) {
			return nil;
		}

		b = frame_map_anywhere(fid);
		if (b == nil) {
			release_memory(fid);
			return nil;
		}

		b->size = len;
		return (void *) (((size_t) b) + sizeof(struct buffer));
	}
}

	void
free(void *p)
{
	struct buffer *b;
	struct chunk *c;
	size_t bin, *size;

	if (p == nil) {
		log(LOG_WARNING, "try to free nil");
		return;
	}

	size = (size_t *) (((size_t) p) - sizeof(size_t));

	bin = bin_n(*size);

	if (bin <= BIN_MAX) {
		c = (struct chunk *) (((size_t) p) - sizeof(size_t));
		free_chunk(bin, c);

	} else {
		b = (struct buffer *) (((size_t) p) - sizeof(struct buffer));
		/* TODO */
	}
}

