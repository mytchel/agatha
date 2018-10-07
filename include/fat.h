/* TODO: If bps * spc is not page aligned we have a problem. */

#define intcopylittle16(X) \
	((X)[0] | ((X)[1]<<8))

#define intcopylittle32(X) \
	((X)[0] | ((X)[1]<<8) | ((X)[2]<<16) | ((X)[3]<<24))

struct fat_bs_ext32 {
  uint8_t spf[4];
  uint8_t extflags[2];
  uint8_t version[2];
  uint8_t rootcluster[4];
  uint8_t info[2];
  uint8_t backup[2];
  uint8_t res_0[12];
  uint8_t drvn;
  uint8_t res_1;
  uint8_t sig;
  uint8_t volid[4];
  uint8_t vollabel[11];
  uint8_t fattypelabel[8];
  uint8_t bootcode[420];
}__attribute__((packed));

struct fat_bs_ext16
{
  uint8_t drvn;
  uint8_t res_1;
  uint8_t sig;
  uint8_t volid[2];
  uint8_t vollabel[11];
  uint8_t fattypelabel[8];
  uint8_t bootcode[448];
}__attribute__((packed));

struct fat_bs {
  uint8_t jmp[3];
  uint8_t oem[8];
  uint8_t bps[2];
  uint8_t spc;
  uint8_t res[2];
  uint8_t nft;
  uint8_t rde[2];
  uint8_t sc16[2];
  uint8_t mdt;
  uint8_t spf[2];
  uint8_t spt[2];
  uint8_t heads[2];
  uint8_t hidden[4];
  uint8_t sc32[4];

  union {
    struct fat_bs_ext32 high;
    struct fat_bs_ext16 low;
  } ext;
  
  uint8_t boot_signature[2];

}__attribute__((packed));

#define FAT_ATTR_read_only        0x01
#define FAT_ATTR_hidden           0x02
#define FAT_ATTR_system           0x04
#define FAT_ATTR_volume_id        0x08
#define FAT_ATTR_directory        0x10
#define FAT_ATTR_archive          0x20
#define FAT_ATTR_lfn \
  (FAT_ATTR_read_only|FAT_ATTR_hidden| \
   FAT_ATTR_system|FAT_ATTR_volume_id)

struct fat_lfn {
  uint8_t order;
  uint8_t first[10];
  uint8_t attr;
  uint8_t type;
  uint8_t chksum;
  uint8_t next[12];
  uint8_t zero[2];
  uint8_t final[4];
}__attribute__((packed));

struct fat_dir_entry {
  uint8_t name[8];
  uint8_t ext[3];
  uint8_t attr;
  uint8_t reserved1;
  uint8_t create_dseconds;
  uint8_t create_time[2];
  uint8_t create_date[2];
  uint8_t last_access[2];
  uint8_t cluster_high[2];
  uint8_t mod_time[2];
  uint8_t mod_date[2];
  uint8_t cluster_low[2];
  uint8_t size[4];
}__attribute__((packed));

struct fat_file {
  uint8_t refs;
  char name[32];
  
  uint32_t attr;
  uint32_t size;
  uint32_t dsize;

  uint32_t start_cluster;

  struct fat_file *parent;
};

typedef enum { FAT16, FAT32 } fat_t;

#define FAT16_END 0xfff8
#define FAT32_END 0x0ffffff8

#define FIDSMAX 32

struct fat {
  int block_pid;
	size_t block_size;
	size_t start, nblocks;

  fat_t type;

  uint32_t bps;
  uint32_t spc;

  uint32_t spf;
  uint8_t nft;

  uint32_t nclusters;
  uint32_t nsectors;
  uint32_t reserved;

  uint32_t rde;
  uint32_t rootdir;
  uint32_t dataarea;

	struct fat_file files[FIDSMAX];
};

#define cluster_to_sector(fat, cluster) \
  (fat->dataarea + ((cluster - 2) * fat->spc))

int
fat_init(struct fat *fat, 
		int block_pid, size_t block_size,
	 	size_t p_start, size_t p_size);

int
fat_read_blocks(struct fat *fat, 
		size_t pa, size_t m_len,
		size_t start, size_t r_len);

int
fat_file_find(struct fat *fat, 
		struct fat_file *parent,
		char *name);

void
fat_file_clunk(struct fat *fat, struct fat_file *file);

int
fat_file_read(struct fat *fat, struct fat_file *file,
	    size_t pa, size_t m_len,
			size_t offset, size_t r_len);

uint32_t
fat_next_cluster(struct fat *fat, uint32_t prev);

uint32_t
fat_table_info(struct fat *fat, uint32_t prev);

int
fat_copy_file_entry_name(struct fat_dir_entry *start, char *name);

int
fat_file_from_entry(struct fat *fat,
		 struct fat_dir_entry *entry,
		 char *name, struct fat_file *parent);

uint32_t
fat_find_free_fid(struct fat *fat);

uint32_t
fat_file_size(struct fat *fat, struct fat_file *file);
 
