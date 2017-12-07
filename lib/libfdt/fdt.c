#include "head.h"
#include "fns.h"
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

    ph[i] = beto32(pr[i]);
  }

  if (head->magic != FDT_MAGIC) {
    debug("fdt magic 0x%h != constant 0x%h\n", 
        head->magic, FDT_MAGIC);

    return ERR;

  } else if (head->version != 17) {
    debug("fdt version %i is not 17 and so is not supported!\n",
        head->version);

    return ERR;

  } else {
    return OK;
  }
}

void
fdt_process_reserved_entries(struct fdt_reserve_entry *e)
{
  while (e->address != 0 && e->size != 0) {
    debug("have reserved entry for 0x%h 0x%h\n", 
        (uint32_t) beto64(e->address), 
        (uint32_t) beto64(e->size));
    e++;
  }
}

  static void *
fdt_skip_prop(void *dtb, uint32_t *pr)
{
  uint32_t p;
  size_t len;

  p = *(++pr);
  len = beto32(p);

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
  *len = beto32(p);

  p = *(++pr);
  no = beto32(p);

  *data = (char *) (pr + 1);
  *name = (char *) dtb + head->off_dt_strings + no;

  return (uint32_t *) ((size_t) pr + (((*len) + 3) & ~3));
}

  static uint32_t *
fdt_skip_node(void *dtb, uint32_t *pr)
{
  uint32_t p;
  char *pc;
  size_t i;

  pc = (char *) (pr + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    p = *pr++;
    p = beto32(p);

    switch (p) {
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
  }
}

  int
fdt_node_property(void *dtb, void *node,
    char *prop, char **data)
{
  struct fdt_header head;
  uint32_t *pr, p;
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
    p = *pr;

    switch (beto32(p)) {
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

  addr = beto32(data[ind * sizeof(uint32_t) * 2 + 0]);
  size = beto32(data[ind * sizeof(uint32_t) * 2 + 1]);

  *r_addr = (size_t) addr;
  *r_size = (size_t) size;

  return true;
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
    bool (*callback)(void *dtb, void *node),
    uint32_t *node)
{
  char *name, *data;
  uint32_t p, *pr;
  size_t i, l, len;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    p = *pr;
    switch (beto32(p)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_find_node_compatable_h(dtb,
            head, compatable, callback,
            pr);

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
              if (!callback(dtb, node)) {
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
    bool (*callback)(void *dtb, void *node))
{
  struct fdt_header head;
  uint32_t *node;

  if (fdt_check(dtb, &head) != OK) {
    return;
  }

  node = (uint32_t *) ((size_t) dtb + head.off_dt_struct);

  fdt_find_node_compatable_h(dtb,
      &head, compatable, callback, node);
}

  static uint32_t *
fdt_find_node_phandle_h(void *dtb, 
    struct fdt_header *head, 
    uint32_t handle,
    uint32_t *node,
    bool *found)
{
  char *name, *data;
  uint32_t p, *pr, h;
  size_t i, len;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    p = *pr;
    switch (beto32(p)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_find_node_phandle_h(dtb,
            head, handle,
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
          h = beto32(((uint32_t *) data)[0]);
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
fdt_find_node_phandle(void *dtb, uint32_t handle)
{
  struct fdt_header head;
  uint32_t *node;
  bool found;

  if (fdt_check(dtb, &head) != OK) {
    return nil;
  }

  found = false;

  node = (uint32_t *) ((size_t) dtb + head.off_dt_struct);

  node = fdt_find_node_phandle_h(dtb,
      &head, handle, node, &found);

  if (found) {
    return node;
  } else {
    return nil;
  }
}

  static uint32_t *
fdt_find_node_device_type_h(void *dtb, 
    struct fdt_header *head, 
    char *type,
    bool (*callback)(void *dtb, void *node),
    uint32_t *node)
{
  char *name, *data;
  uint32_t p, *pr;
  size_t i, len;
  char *pc;

  pc = (char *) (node + 1);
  for (i = 0; pc[i] != 0; i++)
    ;

  pr = (uint32_t *) (pc + (((i+1) + 3) & ~3));

  while (true) {
    p = *pr;
    switch (beto32(p)) {
      default:  
        return nil;

      case FDT_END_NODE:
        return pr;

      case FDT_BEGIN_NODE:
        pr = fdt_find_node_device_type_h(dtb,
            head, type, callback,
            pr);

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
            !callback(dtb, node)) {
            return nil;
        }

        break;
    }

    pr++;
  }
}

  void
fdt_find_node_device_type(void *dtb, char *type, 
    bool (*callback)(void *dtb, void *node))
{
  struct fdt_header head;
  uint32_t *node;

  if (fdt_check(dtb, &head) != OK) {
    return;
  }

  node = (uint32_t *) ((size_t) dtb + head.off_dt_struct);

  fdt_find_node_device_type_h(dtb,
      &head, type, callback, node);
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

