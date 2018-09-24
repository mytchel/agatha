#define MBR_part_len 16
#define MBR_part1    0x1be
#define MBR_part2    0x1ce
#define MBR_part3    0x1de
#define MBR_part4    0x1ee

struct mbr_partition {
  uint8_t status;
  uint8_t start_chs[3];
  uint8_t type;
  uint8_t end_chs[3];
  uint32_t lba;
  uint32_t sectors;
} __attribute__((__packed__));

struct mbr {
  uint8_t bootstrap[446];
  struct mbr_partition parts[4];
  uint8_t boot_signature[2];
};

