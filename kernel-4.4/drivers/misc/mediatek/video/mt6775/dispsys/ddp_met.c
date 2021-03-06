
#define LOG_TAG "MET"

#include <linux/trace_events.h>

#include "ddp_log.h"
#include "ddp_irq.h"
#include "ddp_reg.h"
#include "ddp_met.h"
#include "ddp_path.h"
#include "ddp_ovl.h"
#include "ddp_rdma.h"
#include "DpDataType.h"
#include "display_recorder.h"


#define DDP_IRQ_EER_ID				(0xFFFF0000)
#define DDP_IRQ_FPS_ID				(DDP_IRQ_EER_ID + 1)
#define DDP_IRQ_LAYER_FPS_ID		(DDP_IRQ_EER_ID + 2)
#define DDP_IRQ_LAYER_SIZE_ID		(DDP_IRQ_EER_ID + 3)
#define DDP_IRQ_LAYER_FORMAT_ID	(DDP_IRQ_EER_ID + 4)

#define MAX_PATH_NUM (3)
#define RDMA_NUM (2)
#define MAX_OVL_LAYERS (4)
#define OVL_LAYER_NUM_PER_OVL (4)


static unsigned int met_tag_on;

static struct ovl_info_s {
	unsigned char ovl_idx;
	unsigned char layer_num;
} ovl_infos[OVL_NUM] = {
	{DISP_MODULE_OVL0, 4}, {DISP_MODULE_OVL0_2L, 2}, {DISP_MODULE_OVL1_2L, 2},
};


#if 0
static const char *const parse_color_format(DpColorFormat fmt)
{
	switch (fmt) {
	case eBGR565:
		return "eBGR565";
	case eRGB565:
		return "eRGB565";
	case eRGB888:
		return "eRGB888";
	case eBGR888:
		return "eBGR888";
	case eRGBA8888:
		return "eRGBA8888";
	case eBGRA8888:
		return "eBGRA8888";
	case eARGB8888:
		return "eARGB8888";
	case eABGR8888:
		return "eABGR8888";
	case eVYUY:
		return "eVYUY";
	case eUYVY:
		return "eUYVY";
	case eYVYU:
		return "eYVYU";
	case eYUY2:
		return "eYUY2";
	default:
		return "DEFAULT";
	}
}
#endif

int dpp_disp_is_decouple(void)
{
	if (ddp_is_moudule_in_mutex(0, DISP_MODULE_OVL0) ||
	    ddp_is_moudule_in_mutex(0, DISP_MODULE_OVL0_2L))
		return 0;
	else
		return 1;
}

static void ddp_disp_refresh_tag_start(unsigned int index)
{
	static unsigned long sBufAddr[RDMA_NUM];

	static struct RDMA_BASIC_STRUCT rdmaInfo;

	char tag_name[30] = { '\0' };

	if (dpp_disp_is_decouple() == 1) {

		rdma_get_info(index, &rdmaInfo);
		if (rdmaInfo.addr == 0 || (rdmaInfo.addr != 0 && sBufAddr[index] != rdmaInfo.addr)) {
			sBufAddr[index] = rdmaInfo.addr;
			sprintf(tag_name, index ? "ExtDispRefresh" : "PrimDispRefresh");

			preempt_disable();
			event_trace_printk(disp_get_tracing_mark(), "C|%d|%s|%d\n", DDP_IRQ_FPS_ID,
							tag_name, 1);
			preempt_enable();
		}

	} else {
		static struct OVL_BASIC_STRUCT old_ovlInfo[4+2+2];
		static struct OVL_BASIC_STRUCT ovlInfo[4+2+2];
		int layer_idx = -1;
		int layer_pos = 0;
		int b_layer_changed = 0;
		int i, j;

		/*Traversal layers and get layer info*/
		memset(ovlInfo, 0, sizeof(ovlInfo));/*essential for structure comparision*/

		for (i = 0; i < OVL_NUM; i++) {
			if (i > 0)
				layer_pos += ovl_infos[i-1].layer_num;

			ovl_get_info(ovl_infos[i].ovl_idx, &(ovlInfo[layer_pos]));

			for (j = 0; j < ovl_infos[i].layer_num; j++) {
				layer_idx++;

				if (memcmp(&(ovlInfo[layer_idx]), &(old_ovlInfo[layer_idx]),
						sizeof(struct OVL_BASIC_STRUCT)) == 0)
					continue;

				if (ovlInfo[layer_idx].layer_en)
					b_layer_changed = 1;
			}

			/*store old value*/
			memcpy(&(old_ovlInfo[layer_pos]),
				&(ovlInfo[layer_pos]),
				ovl_infos[i].layer_num*sizeof(struct OVL_BASIC_STRUCT));

		}

		if (b_layer_changed) {
			sprintf(tag_name, index ? "ExtDispRefresh" : "PrimDispRefresh");

			preempt_disable();
			event_trace_printk(disp_get_tracing_mark(), "C|%d|%s|%d\n", DDP_IRQ_FPS_ID,
							tag_name, 1);
			preempt_enable();
		}

	}
}

static void ddp_disp_refresh_tag_end(unsigned int index)
{
	char tag_name[30] = { '\0' };

	sprintf(tag_name, index ? "ExtDispRefresh" : "PrimDispRefresh");
	preempt_disable();
	event_trace_printk(disp_get_tracing_mark(), "C|%d|%s|%d\n", DDP_IRQ_FPS_ID,
					tag_name, 0);
	preempt_enable();
}

static void ddp_inout_info_tag(unsigned int index)
{
#if 0
	static unsigned long sLayerBufAddr[OVL_NUM][OVL_LAYER_NUM_PER_OVL];
	static unsigned int sLayerBufFmt[OVL_NUM][OVL_LAYER_NUM_PER_OVL];
	static unsigned int sLayerBufWidth[OVL_NUM][OVL_LAYER_NUM_PER_OVL];
	static unsigned int sLayerBufHeight[OVL_NUM][OVL_LAYER_NUM_PER_OVL];

	struct OVL_BASIC_STRUCT ovlInfo[OVL_NUM*OVL_LAYER_NUM_PER_OVL];
	unsigned int flag, i, idx, enLayerCnt, layerCnt;
	unsigned int width, height, bpp, fmt;
	char *fmtStr;
	char tag_name[30] = { '\0' };
	uint32_t layer_change_bits = 0;
	uint32_t layer_enable_bits = 0;

	memset((void *)ovlInfo, 0, sizeof(ovlInfo));
	ovl_get_info(index, ovlInfo);

	/* Any layer enable bit changes , new frame refreshes */
	enLayerCnt = 0;
	if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY)
		layerCnt = OVL_LAYER_NUM;
	else
		layerCnt = OVL_LAYER_NUM_PER_OVL;

	for (i = 0; i < layerCnt; i++) {
		if (ovl_get_status() == DDP_OVL1_STATUS_PRIMARY)
			index = 1 - i / OVL_LAYER_NUM_PER_OVL;

		idx = i % OVL_LAYER_NUM_PER_OVL;

		fmtStr = parse_color_format(ovlInfo[i].fmt);

		if (ovlInfo[i].layer_en) {
			enLayerCnt++;
			layer_enable_bits |= (1 << i);
			if (sLayerBufAddr[index][idx] != ovlInfo[i].addr) {
				sLayerBufAddr[index][idx] = ovlInfo[i].addr;
				sprintf(tag_name, "OVL%dL%d_InFps", index, idx);
				met_tag_oneshot(DDP_IRQ_LAYER_FPS_ID, tag_name, i+1);
				layer_change_bits |= 1 << i;
			}
#if 0
			if (sLayerBufFmt[index][idx] != ovlInfo[i].fmt) {
				sLayerBufFmt[index][idx] = ovlInfo[i].fmt;
				sprintf(tag_name, "OVL%dL%d_Fmt_%s", index, idx, fmtStr);
				met_tag_oneshot(DDP_IRQ_LAYER_FORMAT_ID, tag_name, i+1);
				layer_change_bits |= 1 << i;
			}
			if (sLayerBufWidth[index][idx] != ovlInfo[i].src_w) {
				sLayerBufWidth[index][idx] = ovlInfo[i].src_w;
				sprintf(tag_name, "OVL%dL%d_Width", index, idx);
				met_tag_oneshot(DDP_IRQ_LAYER_SIZE_ID, tag_name, ovlInfo[i].src_w);
				layer_change_bits |= 1 << i;
			}
			if (sLayerBufHeight[index][idx] != ovlInfo[i].src_h) {
				sLayerBufHeight[index][idx] = ovlInfo[i].src_h;
				sprintf(tag_name, "OVL%dL%d_Height", index, idx);
				met_tag_oneshot(DDP_IRQ_LAYER_SIZE_ID, tag_name, ovlInfo[i].src_h);
				layer_change_bits |= 1 << i;
			}
#endif
		} else {
			sLayerBufAddr[index][idx] = 0;
			sLayerBufFmt[index][idx] = 0;
			sLayerBufWidth[index][idx] = 0;
			sLayerBufHeight[index][idx] = 0;
		}

		if ((i == (OVL_LAYER_NUM_PER_OVL - 1)) || (i == (OVL_LAYER_NUM - 1))) {
			if (enLayerCnt) {
				enLayerCnt = 0;
				sprintf(tag_name, "OVL%d_OutFps", index);
				met_tag_oneshot(DDP_IRQ_LAYER_FPS_ID, tag_name, index);
			}
		}
	}


	/*CLS:met mmsys profile*/
	{
		int i;

		for (i = 0; i < OVL_LAYER_NUM; i++) {
			if (layer_change_bits & (1 << i)) {
				MET_UDTL_GET_PROP(OVL_LAYER_Props).layer	= i;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).layer_en	= layer_enable_bits;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).fmt	= ovlInfo[i].fmt;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).addr	= ovlInfo[i].addr;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).src_w	= ovlInfo[i].src_w;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).src_h	= ovlInfo[i].src_h;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).src_pitch	= ovlInfo[i].src_pitch;
				MET_UDTL_GET_PROP(OVL_LAYER_Props).bpp	= ovlInfo[i].bpp;

				MET_UDTL_TRACELINE_PROP(MMSYS, OVL_LAYERS__LAYER, OVL_LAYER_Props);
			}
		}
	}


#endif

}

static void ddp_err_irq_met_tag(const char *name)
{
	preempt_disable();
	event_trace_printk(disp_get_tracing_mark(), "C|%d|%s|%d\n", DDP_IRQ_EER_ID,
					name, 1);
	event_trace_printk(disp_get_tracing_mark(), "C|%d|%s|%d\n", DDP_IRQ_EER_ID,
					name, 0);
	preempt_enable();
}

static void met_irq_handler(enum DISP_MODULE_ENUM module, unsigned int reg_val)
{
	int index = 0;
	char tag_name[30] = { '\0' };

	/* DDPERR("met_irq_handler() module=%d, val=0x%x\n", module, reg_val); */
	switch (module) {
	case DISP_MODULE_RDMA0:
	case DISP_MODULE_RDMA1:
		index = module - DISP_MODULE_RDMA0;
		if (reg_val & (1 << 2))
			ddp_disp_refresh_tag_end(index);/*Always process eof prior to sof*/

		if (reg_val & (1 << 1))
			ddp_disp_refresh_tag_start(index);

		if (reg_val & (1 << 4)) {
			sprintf(tag_name, "rdma%d_underflow", index);
			ddp_err_irq_met_tag(tag_name);
		}
		if (reg_val & (1 << 3)) {
			sprintf(tag_name, "rdma%d_abnormal", index);
			ddp_err_irq_met_tag(tag_name);
		}
		break;

	case DISP_MODULE_OVL0:
		index = module - DISP_MODULE_OVL0;
		if (reg_val & (1 << 1)) {/*EOF*/
			ddp_inout_info_tag(index);
		}

		break;

	default:
		break;
	}
}

void ddp_init_met_tag(int state, int rdma0_mode, int rdma1_mode)
{
	if ((!met_tag_on) && state) {
		met_tag_on = state;
		disp_register_irq_callback(met_irq_handler);
	}
	if (met_tag_on && (!state)) {
		met_tag_on = state;
		disp_unregister_irq_callback(met_irq_handler);
	}
}
EXPORT_SYMBOL(ddp_init_met_tag);
