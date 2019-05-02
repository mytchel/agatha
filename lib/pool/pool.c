#include <types.h>
#include <err.h>
#include <pool.h>

int
pool_init(struct pool *p, size_t obj_size)
{
	obj_size += sizeof(struct pool_obj);
	obj_size = (obj_size + 3) & ~3;

	p->obj_size = obj_size;
	p->frames = nil;
	p->objs = nil;
	p->n_free = 0;

	return OK;
}

int
pool_destroy(struct pool *s)
{
	return ERR;
}

int
pool_load(struct pool *p, void *a, size_t len)
{
	struct pool_frame *f = a;
	struct pool_obj *o;
	size_t i;

	if (len < p->obj_size + sizeof(struct pool_frame)) {
		return ERR;
	}

	f->len = len;
	f->nobj = (len - sizeof(struct pool_frame)) / p->obj_size;

	for (i = 0; i < f->nobj; i++) {
		o = (struct pool_obj *) &f->body[p->obj_size * i];
		o->in_use = false;
		o->next = p->objs;
		p->objs = o;
	}

	p->n_free += f->nobj;

	f->next = p->frames;
	p->frames = f;

	return OK;
}

size_t
pool_n_free(struct pool *p)
{
	return p->n_free;
}

size_t
pool_obj_size(struct pool *p)
{
	return p->obj_size;
}

	void *
pool_alloc(struct pool *p)
{
	struct pool_obj *o;

	o = p->objs;
	if (o == nil) {
		return nil;
	}

	p->objs = o->next;
	o->in_use = true;
	p->n_free--;

	return o->body;
}

	void
pool_free(struct pool *p, void *v)
{
	struct pool_obj *o;

	o = (struct pool_obj *) 
		(((size_t) v) - sizeof(struct pool_obj));

	o->in_use = false;
	o->next = p->objs;
	p->objs = o;
	p->n_free++;
}

