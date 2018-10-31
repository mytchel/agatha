#include <types.h>
#include <mach.h>
#include <err.h>
#include <c.h>
#include <stdarg.h>
#include <string.h>
#include <fdt.h>

/* This is all pretty bad and could go for a lot of reworking. 
   But it works. 
TODO: add support for #address-cells and #size_cells when
dealing with reg parameters.
 */

  int
fdt_check(void *dtb, struct fdt_header *head)
{
  uint32_t *pr, *ph;
  size_t i;

  pr = (uint32_t *) dtb;
  ph = (uint32_t *) head;
  for (i = 0; 
      i < sizeof(struct fdt_header) / sizeof(uint32_t); 
      i++) {

    ph[i] = beto32(&pr[i]);
  }

  if (head->magic != FDT_MAGIC) {
    return ERR;

  } else if (head->version != 17) {
    return ERR;

  } else {
    return OK;
  }
}

size_t
fdt_size(void *dtb)
{
	struct fdt_header head;

  if (fdt_check(dtb, &head) != OK) {
    return 0;
  
	} else {
		return head.totalsize;	
	}
}

void
fdt_check_reserved(void *dtb, 
    void (*callback)(size_t start, size_t len))
{
  struct fdt_header head;
  struct fdt_reserve_entry *e;

  if (fdt_check(dtb, &head) != OK) {
    return;
  }

  e = (struct fdt_reserve_entry *) 
    ((size_t) dtb + head.off_mem_rsvmap);

  while (e->address != 0 && e->size != 0) {
    callback((size_t) beto64(&e->address), 
        (size_t) beto64(&e->size));

    e++;
  }
}

  static void *
fdt_skip_prop(void *dtb, uint32_t *pr)
{
  uint32_t p;
  size_t len;

  p = *(++pr);
  len = beto32(&p);

  /* Skip name offset. */
  ++pr;

  return (void *) ((size_t) pr + (((len) + 3) & ~3));
}

  static uint32_t *
fdt_get_prop(void *dtb,
    struct fdt_header *head,
    uint32_t *pr,
    char **name,
    char **data,
    size_t *len)
{
  uint32_t p;
  size_t no;

  p = *(++pr);
  *len = beto32(&p);

  p = *(++pr);
  no = beto32(&p);

  *data = (char *) (pr + 1);
  *name = (char *) dtb + head->off_dt_strings + no;

  return (uint32_t *) ((size_t) pr + (((*len) + 3) & ~3));
}

  static uint32_t *
fdt_skip_node(void *dtb, uint32_t *pr)
{
  char *pc;
  size_t i;

  pc = (char *) (pr + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    switch (beto32(pr)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_skip_node(dtb, pr);
        if (pr == nil) {
          return nil;
        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
        pr = fdt_skip_prop(dtb, pr);
        break;
    }

		pr++;
  }
}

  int
fdt_node_property(void *dtb, void *node,
    char *prop, char **data)
{
  struct fdt_header head;
  uint32_t *pr;
  char *name;
  char *pc;
  size_t i;

  if (fdt_check(dtb, &head) != OK) {
    return -1;
  }

  pc = (char *) ((size_t) node + sizeof(uint32_t));
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    switch (beto32(pr)) {
      default:
        return -1;

      case FDT_END_NODE:
        return -1;

      case FDT_BEGIN_NODE:

        pr = fdt_skip_node(dtb, pr);
        if (pr == nil) {
          return -1;

        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
        pr = fdt_get_prop(dtb, &head,
            pr, &name,
            data, &i); 

        if (strcmp(prop, name)) {
          return i;

        } else {
          break;
        }
    }

    pr++;
  }
}

  bool
fdt_node_regs(void *dtb, void *node,
    int ind,
    size_t *r_addr,
    size_t *r_size)
{
  uint64_t addr, size;
  uint32_t *data; 
  size_t len;

  len = fdt_node_property(dtb, node, "reg", (char **) &data);

  /* TODO: should use address-cells and size-cells
     to support 64 bit and so on. */

  if (ind * sizeof(uint32_t) * 2 >= len) {
    return false;
  }

  addr = beto32(&data[ind * sizeof(uint32_t) * 2 + 0]);
  size = beto32(&data[ind * sizeof(uint32_t) * 2 + 1]);

  *r_addr = (size_t) addr;
  *r_size = (size_t) size;

  return true;
}

	int
fdt_node_path(void *dtb, void *node, char *path, size_t max)
{
	return -1;
}

  char *
fdt_node_name(void *dtb, void *node)
{
  return ((char *) node) + sizeof(uint32_t);
}

  static uint32_t *
fdt_find_node_compatable_h(void *dtb, 
    struct fdt_header *head, 
    char *compatable,
    bool (*callback)(void *dtb, void *node, void *arg),
    uint32_t *node,
    void *arg)
{
  char *name, *data;
  uint32_t *pr;
  size_t i, l, len;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    switch (beto32(pr)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_find_node_compatable_h(dtb,
            head, compatable, callback,
            pr, arg);

        if (pr == nil) {
          return nil;

        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
        pr = fdt_get_prop(dtb,
            head, pr, &name,
            &data, &len); 

        if (strcmp("compatible", name)) {
          for (i = 0; i < len; i += l + 1) {
            if (strcmp(compatable, &data[i])) {
              if (!callback(dtb, node, arg)) {
                return nil;
              }
            }

            l = strlen(&data[i]);
          }
        }

        break;
    }

    pr++;
  }

  return pr;
}

  void
fdt_find_node_compatable(void *dtb, char *compatable,
    bool (*callback)(void *dtb, void *node, void *arg),
    void *arg)
{
  struct fdt_header head;
  uint32_t *node;

  if (fdt_check(dtb, &head) != OK) {
    return;
  }

  node = (uint32_t *) ((size_t) dtb + head.off_dt_struct);

  fdt_find_node_compatable_h(dtb,
      &head, compatable, callback, node, arg);
}

uint32_t
fdt_node_phandle(void *dtb, void *node)
{
  char *data;
  int len;

  len = fdt_node_property(dtb, node, "phandle", &data);
  if (len <= 0) {
    return 0;
  }

  return beto32(data);
}

  static uint32_t *
fdt_find_node_phandle_h(void *dtb, struct fdt_header *head, 
		uint32_t handle,
    uint32_t *node, bool *found)
{
  char *name, *data;
  uint32_t *pr, h;
  size_t i, len;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    switch (beto32(pr)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_find_node_phandle_h(dtb, head, 
						handle,
            pr, found);

        if (pr == nil) {
          return nil;

        } else if (*found) {
          return pr;

        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
        pr = fdt_get_prop(dtb,
            head, pr, &name,
            &data, &len); 

        if (strcmp("phandle", name)) {
          h = beto32(data);
          if (h == handle) {
            *found = true;
            return node;
          }
        }

        break;
    }

    pr++;
  }

  return pr;
}

  void *
fdt_find_node_phandle(void *dtb, void *node, uint32_t handle)
{
	struct fdt_header head;
  bool found;

  if (fdt_check(dtb, &head) != OK) {
    return nil;
  }

	found = false;

  node = fdt_find_node_phandle_h(dtb,
      &head, handle, node, &found);

  if (found) {
    return node;
  } else {
    return nil;
  }
}

void *
fdt_find_node_path_h(void *dtb, 
		char *path, 
		uint32_t *node, bool *found)
{
  uint32_t *pr;
	bool skip;
  size_t i;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;
  
	pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

	skip = false;
	for (i = 0; pc[i] != 0 && path[i] != 0 && path[i] != '/'; i++) {
		if (pc[i] != path[i]) {
			skip = true;
			break;
		}
	}

	if (skip) {
		return fdt_skip_node(dtb, node);

	} else {
		if (path[i] == 0) {
			*found = true;
			return node;
		}

		path = &path[i+1];
	}
	
  while (true) {
    switch (beto32(pr)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
				pr = fdt_find_node_path_h(dtb,
						path, 
						pr, found);

        if (pr == nil) {
          return nil;

        } else if (*found) {
          return pr;

        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
				pr = fdt_skip_prop(dtb, pr);
        break;
    }

    pr++;
  }
}

void *
fdt_find_node_path(void *dtb, void *node, char *path)
{
  bool found;

	found = false;

  node = fdt_find_node_path_h(dtb, path, 
			node, &found);

  if (found) {
    return node;
  } else {
    return nil;
  }
}

bool
fdt_compatable(char *device, size_t devlen, char *driver, size_t drilen)
{
	int len, dlen;
	char *d;

	while (devlen > 0 && *device != 0) {
		d = driver;
		dlen = drilen;
		while (dlen > 0 && *d != 0) {
			if (strcmp(device, d)) {
				return true;
			}

			len = strlen(d);
			d += len + 1;
			dlen -= len + 1;
		}

		len = strlen(device);
		device += len + 1;
		devlen -= len + 1;
	}

	return false;
}

  static uint32_t *
fdt_find_node_device_type_h(void *dtb, 
    struct fdt_header *head, 
    char *type,
    bool (*callback)(void *dtb, void *node, void *arg),
    uint32_t *node,
    void *arg)
{
  char *name, *data;
  size_t i, len;
  uint32_t *pr;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    switch (beto32(pr)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_find_node_device_type_h(dtb,
            head, type, callback,
            pr, arg);

        if (pr == nil) {
          return nil;

        } else {
          break;
        }

      case FDT_NOP:
        break;

      case FDT_PROP:
        pr = fdt_get_prop(dtb,
            head, pr, &name,
            &data, &len); 

        if (strcmp("device_type", name) && 
            strcmp(type, data) &&
            !callback(dtb, node, arg)) {
            return nil;
        }

        break;
    }

    pr++;
  }
}

  void
fdt_find_node_device_type(void *dtb, char *type, 
    bool (*callback)(void *dtb, void *node, void *arg),
    void *arg)
{
  struct fdt_header head;
  uint32_t *node;

  if (fdt_check(dtb, &head) != OK) {
    return;
  }

  node = (uint32_t *) ((size_t) dtb + head.off_dt_struct);

  fdt_find_node_device_type_h(dtb,
      &head, type, callback, node, arg);
}

  void *
fdt_root_node(void *dtb)
{
  struct fdt_header head;

  if (fdt_check(dtb, &head) != OK) {
    return nil;
  }

  return (void *) ((size_t) dtb + head.off_dt_struct);
}

