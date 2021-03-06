
#ifndef __BMT_H__
#define __BMT_H__

#ifdef CONFIG_MNTL_SUPPORT
#include "mtk_nand_ops.h"
#endif

#define MAX_BMT_SIZE        (0x200)
#define BMT_VERSION         (1)	/* initial version */

#define MAIN_SIGNATURE_OFFSET   (0)
#define OOB_SIGNATURE_OFFSET    (1)
#define OOB_INDEX_OFFSET        (29)
#define OOB_INDEX_SIZE          (2)
#define FAKE_INDEX              (0xAAAA)

struct bmt_entry {
	u16 bad_index;		/* bad block index */
	u16 mapped_index;	/* mapping block index in the replace pool */
};

enum update_reason_t {
	UPDATE_ERASE_FAIL,
	UPDATE_WRITE_FAIL,
	UPDATE_UNMAPPED_BLOCK,
	UPDATE_REMOVE_ENTRY,
	UPDATE_INIT_WRITE,
	UPDATE_REASON_COUNT,
	INIT_BAD_BLK,
	FTL_MARK_BAD = 64,
};

struct bmt_struct {
	struct bmt_entry table[MAX_BMT_SIZE];
	u8 version;
	u8 mapped_count;	/* mapped block count in pool */
	u8 bad_count;		/* bad block count in pool. Not used in V1 */
#ifdef CONFIG_MNTL_SUPPORT
	struct data_bmt_struct data_bmt;
#endif
};

extern int mtk_nand_exec_read_page(struct mtd_info *mtd, u32 u4RowAddr, u32 u4PageSize,
				   u8 *pPageBuf, u8 *pFDMBuf);
extern int mtk_nand_block_bad_hw(struct mtd_info *mtd, loff_t ofs);
extern int mtk_nand_erase_hw(struct mtd_info *mtd, int page);
extern int mtk_nand_block_markbad_hw(struct mtd_info *mtd, loff_t ofs);
extern int mtk_nand_exec_write_page(struct mtd_info *mtd, u32 row, u32 page_size, u8 *dat,
				    u8 *oob);
#if defined(CONFIG_MTK_TLC_NAND_SUPPORT)
extern bool mtk_is_normal_tlc_nand(void);
extern u32 mtk_nand_page_transform(struct mtd_info *mtd, struct nand_chip *chip, u32 page,
				    u32 *blk, u32 *map_blk);
#endif

void set_bad_index_to_oob(u8 *oob, u16 index);


struct bmt_struct *init_bmt(struct nand_chip *nand, int size);
bool update_bmt(u64 offset, enum update_reason_t reason, u8 *dat, u8 *oob);
unsigned short get_mapping_block_index(int index);
unsigned short get_bad_block_index(int index);
#ifdef CONFIG_MNTL_SUPPORT
int get_data_bmt(struct data_bmt_struct *data_bmt);
#endif
#endif				/* #ifndef __BMT_H__ */
