#include <types.h>
#include <err.h>
#include <sys.h>
#include <c.h>
#include <am335x/mmchs.h>

#include "sdmmcreg.h"
#include "sdhcreg.h"
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

  if (!mmchssendcmd(cmd, 0)) {
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
  uint32_t ecsdcount;
  
  if (!cardextcsd()) {
    printf("%s failed to read extended csd\n", name);
    return false;
  }

  ecsdcount = intcopylittle32(&ext_csd[212]);
  if (ecsdcount > 0) {
    nblk = ecsdcount;
  } else {
    nblk = MMC_CSD_CAPACITY(csd);
  }

  return true;
}

