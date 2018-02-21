#include <libc.h>
#include <fs.h>
#include <string.h>
#include <block.h>

#include "sdmmcreg.h"
#include "sdhcreg.h"
#include "omap_mmc.h"
#include "mmchs.h"

static uint8_t ext_csd[512];

static bool
cardextcsd(void)
{
  uint32_t cmd;

  cmd = MMCHS_SD_CMD_INDEX_CMD(MMC_SEND_EXT_CSD)
    | MMCHS_SD_CMD_RSP_TYPE_48B
    | MMCHS_SD_CMD_DP_DATA
    | MMCHS_SD_CMD_DDIR_READ;

  if (!mmchssendappcmd(cmd, 0)) {
    return false;
  }

  if (!mmchsreaddata((uint32_t *) ext_csd, 512)) {
    return false;
  }

  return true;
}

bool
mmcinit(void)
{
  if (!cardextcsd()) {
    printf("%s failed to read extended csd\n", name);
    return false;
  }

  nblk = MMC_CSD_CAPACITY(csd);

  return true;
}
