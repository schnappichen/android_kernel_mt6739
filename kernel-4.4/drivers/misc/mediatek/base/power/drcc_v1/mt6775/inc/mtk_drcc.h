#ifndef _MTK_DRCC_
#define _MTK_DRCC_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#if defined(__MTK_DRCC_C__)
#include <mt-plat/mtk_secure_api.h>
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
#define mt_secure_call_drcc	mt_secure_call
#else
/* This is workaround for drcc use */
static noinline int mt_secure_call_drcc(u64 function_id,
				u64 arg0, u64 arg1, u64 arg2)
{
	register u64 reg0 __asm__("x0") = function_id;
	register u64 reg1 __asm__("x1") = arg0;
	register u64 reg2 __asm__("x2") = arg1;
	register u64 reg3 __asm__("x3") = arg2;
	int ret = 0;

	asm volatile(
	"smc    #0\n"
		: "+r" (reg0)
		: "r" (reg1), "r" (reg2), "r" (reg3));

	ret = (int)reg0;
	return ret;
}
#endif
#endif

#define DRCC_BASEADDR 0x0C532000
#define DRCC_CONF0 (DRCC_BASEADDR + 0x750)
#define DRCC_CONF1 (DRCC_BASEADDR + 0x754)
#define DRCC_CONF2 (DRCC_BASEADDR + 0x758)
#define DRCC_CONF3 (DRCC_BASEADDR + 0x75C)

#ifdef CONFIG_ARM64
#define MTK_SIP_KERNEL_DRCC_INIT		0xC2000300
#define MTK_SIP_KERNEL_DRCC_ENABLE		0xC2000301
#define MTK_SIP_KERNEL_DRCC_TRIG		0xC2000302
#define MTK_SIP_KERNEL_DRCC_COUNT		0xC2000303
#define MTK_SIP_KERNEL_DRCC_MODE		0xC2000304
#define MTK_SIP_KERNEL_DRCC_CODE		0xC2000305
#define MTK_SIP_KERNEL_DRCC_HWGATEPCT		0xC2000306
#define MTK_SIP_KERNEL_DRCC_VREFFILT		0xC2000307
#define MTK_SIP_KERNEL_DRCC_AUTOCALIBDELAY	0xC2000308
#define MTK_SIP_KERNEL_DRCC_FORCETRIM		0xC2000309
#define MTK_SIP_KERNEL_DRCC_PROTECT		0xC200030A
#define MTK_SIP_KERNEL_DRCC_READ		0xC200030B
#else
#define MTK_SIP_KERNEL_DRCC_INIT		0x82000300
#define MTK_SIP_KERNEL_DRCC_ENABLE		0x82000301
#define MTK_SIP_KERNEL_DRCC_TRIG		0x82000302
#define MTK_SIP_KERNEL_DRCC_COUNT		0x82000303
#define MTK_SIP_KERNEL_DRCC_MODE		0x82000304
#define MTK_SIP_KERNEL_DRCC_CODE		0x82000305
#define MTK_SIP_KERNEL_DRCC_HWGATEPCT		0x82000306
#define MTK_SIP_KERNEL_DRCC_VREFFILT		0x82000307
#define MTK_SIP_KERNEL_DRCC_AUTOCALIBDELAY	0x82000308
#define MTK_SIP_KERNEL_DRCC_FORCETRIM		0x82000309
#define MTK_SIP_KERNEL_DRCC_PROTECT		0x8200030A
#define MTK_SIP_KERNEL_DRCC_READ		0x8200030B
#endif

extern int mtk_drcc_calibration_result(void);
extern void mtk_drcc_init(void);
extern void mtk_drcc_enable(unsigned int onOff);
extern int mtk_drcc_feature_enabled_check(void);
/* SRAM debugging */
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_drcc_0(u8 val);
extern void aee_rr_rec_drcc_1(u8 val);
extern void aee_rr_rec_drcc_2(u8 val);
extern void aee_rr_rec_drcc_3(u8 val);

extern u8 aee_rr_curr_drcc_0(void);
extern u8 aee_rr_curr_drcc_1(void);
extern u8 aee_rr_curr_drcc_2(void);
extern u8 aee_rr_curr_drcc_3(void);
#endif

#endif
