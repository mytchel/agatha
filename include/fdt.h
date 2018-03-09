
struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

struct fdt_reserve_entry {
  uint64_t address;
  uint64_t size;
};

struct fdt_prop {
  uint32_t len;
  uint32_t nameoff;
};

#define FDT_MAGIC         0xd00dfeed
#define FDT_BEGIN_NODE    0x1
#define FDT_END_NODE      0x2
#define FDT_PROP          0x3
#define FDT_NOP           0x4
#define FDT_END           0x9

int
fdt_check(void *dtb, struct fdt_header *head);

size_t
fdt_size(void *dtb);

int
fdt_node_property(void *dtb, void *node,
    char *prop,
    char **data);

bool
fdt_node_regs(void *dtb, void *node,
    int ind,
    size_t *addr,
    size_t *size);

void *
fdt_find_node_phandle(void *dtb, uint32_t handle);

uint32_t
fdt_node_phandle(void *dtb, void *node);

void
fdt_find_node_compatable(void *dtb,
    char *compatable,
    bool (*callback)(void *dtb, void *node, void *arg),
    void *arg);

void
fdt_find_node_device_type(void *dtb, char *type,
    bool (*callback)(void *dtb, void *node, void *arg),
    void *arg);

void *
fdt_root_node(void *dtb);

char *
fdt_node_name(void *dtb, void *node);

int
fdt_node_path(void *dtb, void *node, char **path, size_t max);

void
fdt_check_reserved(void *dtb, 
    void (*callback)(size_t start, size_t len));

