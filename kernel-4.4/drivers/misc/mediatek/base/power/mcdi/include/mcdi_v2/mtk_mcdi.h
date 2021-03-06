
#ifndef __MTK_MCDI_H__
#define __MTK_MCDI_H__

#if defined(CONFIG_MACH_MT6739)
#include "mtk_mcdi_mt6739.h"
#endif

#define CPU_PWR_STAT_MASK       0x000000FF
#define CLUSTER_PWR_STAT_MASK   0x00030000

#define MCDI_DEBUG_INFO_MAGIC_NUM           0x1eef9487
#define MCDI_DEBUG_INFO_NON_REPLACE_OFFSET  0x0008

extern u32 aee_rr_rec_mcdi_val(int id, u32 val);

int cluster_idx_get(int cpu);
unsigned int get_menu_predict_us(void);
bool mcdi_task_pause(bool paused);
unsigned int mcdi_mbox_read(void __iomem *id);
void mcdi_mbox_write(void __iomem *id, unsigned int val);
void update_avail_cpu_mask_to_mcdi_controller(unsigned int cpu_mask);
bool is_cpu_pwr_on_event_pending(void);
int mcdi_get_mcdi_idle_state(int idx);
unsigned int mcdi_get_buck_ctrl_mask(void);
void mcdi_status_init(void);
void mcdi_of_init(void);
unsigned int get_pwr_stat_check_map(int type, int idx);
bool mcdi_pause(bool paused);
void update_cpu_isolation_mask_to_mcdi_controller(unsigned int iso_mask);
void mcdi_wakeup_all_cpu(void);

#endif /* __MTK_MCDI_H__ */
