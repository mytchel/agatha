uint32_t
crc_start(void);

uint32_t 
crc_continue(uint8_t *p, size_t l, uint32_t crc);

uint32_t
crc_finish(uint32_t crc);

uint32_t
crc_checksum(uint8_t *p, size_t l);

