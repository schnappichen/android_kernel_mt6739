
#if !defined(_FFU_H_)
#define _FFU_H_

#include <linux/mmc/card.h>

#define CARD_BLOCK_SIZE 512

#define MMC_FFU_DOWNLOAD_OP 302
#define MMC_FFU_INSTALL_OP  303

#define MMC_FFU_MODE_SET    0x1
#define MMC_FFU_MODE_NORMAL 0x0
#define MMC_FFU_INSTALL_SET 0x1

#ifdef CONFIG_MMC_FFU
#define MMC_FFU_ENABLE      0x0
#define MMC_FFU_CONFIG      0x1
#define MMC_FFU_SUPPORTED_MODES 0x1
#define MMC_FFU_FEATURES    0x1

#define FFU_ENABLED(ffu_enable)	(ffu_enable & MMC_FFU_CONFIG)
#define FFU_SUPPORTED_MODE(ffu_sup_mode) \
	(ffu_sup_mode && MMC_FFU_SUPPORTED_MODES)
#define FFU_CONFIG(ffu_config) (ffu_config & MMC_FFU_CONFIG)
#define FFU_FEATURES(ffu_fetures) (ffu_fetures & MMC_FFU_FEATURES)

struct mmc_blk_ioc_data *mmc_ffu_ioctl_copy_from_user(
	struct mmc_ioc_cmd __user *user);
int mmc_ffu_download(struct mmc_card *card, struct mmc_command *cmd,
	u8 *data, int buf_bytes);
int mmc_ffu_install(struct mmc_card *card, u8 *ext_csd);

#endif
#endif /* FFU_H_ */

