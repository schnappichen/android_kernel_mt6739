

#define __MTK_DRCC_C__


/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef __KERNEL__
	#include <linux/topology.h>

	/* local includes (kernel-4.4)*/
	#include <mt-plat/mtk_chip.h>
	#include <mt-plat/mtk_gpio.h>
	#include "mtk_drcc.h"
	#include <mt-plat/mtk_devinfo.h>
#endif

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

#define LOG_INTERVAL	(2LL * NSEC_PER_SEC)

/* Get time stmp to known the time period */
static unsigned long long drcc_pTime_us, drcc_cTime_us, drcc_diff_us;
#ifdef __KERNEL__
#define TIME_TH_US 3000
#define DRCC_IS_TOO_LONG()	\
	do {			\
		drcc_diff_us = drcc_cTime_us - drcc_pTime_us;		\
		if (drcc_diff_us > TIME_TH_US) {			\
			pr_debug(DRCC_TAG "caller_addr %p: %llu us\n",	\
			__builtin_return_address(0), drcc_diff_us);	\
		} else if (drcc_diff_us < 0) {				\
			pr_debug(DRCC_TAG "E: misuse caller_addr %p\n",	\
			__builtin_return_address(0));			\
		}							\
	} while (0)
#endif
#undef  BIT
#define BIT(bit)	(1U << (bit))

#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)
#define BITMASK(r)	\
	(((unsigned) -1 >> (31 - MSB(r))) & ~((1U << LSB(r)) - 1))

/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))
#define GET_BITS_VAL(_bits_, _val_)   (((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

#define DRCC_TAG	 "[xxxx_drcc] "

#define drcc_info(fmt, args...)		pr_info(DRCC_TAG fmt, ##args)
#define drcc_debug(fmt, args...)	pr_debug(DRCC_TAG"(%d)" fmt, __LINE__, ##args)

#ifdef __KERNEL__
	#define drcc_read(addr)	__raw_readl((void __iomem *)(addr))
	#define drcc_read_field(addr, range)	\
		((drcc_read(addr) & BITMASK(range)) >> LSB(range))
	#define drcc_write(addr, val)	mt_reg_sync_writel(val, addr)
#endif
#define drcc_write_field(addr, range, val)	\
	drcc_write(addr, (drcc_read(addr) & ~BITMASK(range)) | BITS(range, val))

static struct hrtimer drcc_timer_log;
static bool drcc_timer_en;
static DEFINE_SPINLOCK(drcc_spinlock);

#if 0 /* def CONFIG_MTK_RAM_CONSOLE */
static void _mt_drcc_aee_init(void)
{
	aee_rr_rec_drcc_0(0xFFFFFFFF);
	aee_rr_rec_drcc_1(0xFFFFFFFF);
	aee_rr_rec_drcc_2(0xFFFFFFFF);
	aee_rr_rec_drcc_3(0xFFFFFFFF);
}
#endif

static long long drcc_get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec);
}

static void mtk_drcc_lock(unsigned long *flags)
{
#ifdef __KERNEL__
	spin_lock_irqsave(&drcc_spinlock, *flags);
	drcc_pTime_us = drcc_get_current_time_us();
#endif
}

static void mtk_drcc_unlock(unsigned long *flags)
{
#ifdef __KERNEL__
	drcc_cTime_us = drcc_get_current_time_us();
	DRCC_IS_TOO_LONG();
	spin_unlock_irqrestore(&drcc_spinlock, *flags);
#endif
}

void mtk_drcc_log2RamConsole(void)
{
	unsigned int i, value[4];

	for (i = 0; i < 4; i++) {
		value[i] = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
			DRCC_CONF0 + (i * 4), 0, 0);
		/* drcc_debug("reg_0x%x = 0x%X\n", i, value[i]); */
	}
	#if 0 /* def CONFIG_MTK_RAM_CONSOLE */
	aee_rr_rec_drcc_0(value[0]);
	aee_rr_rec_drcc_1(value[1]);
	aee_rr_rec_drcc_2(value[2]);
	aee_rr_rec_drcc_3(value[3]);
	#endif
}

int mtk_drcc_feature_enabled_check(void)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	return (status & 0x01);
}

int mtk_drcc_calibration_result(void)
{
	unsigned long flags;
	unsigned int value, result;

	/* check the calibration result */
	mtk_drcc_lock(&flags);
	value = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
		DRCC_CONF3, 0, 0);
	/* drcc_debug("DRCC calibration result = (0x%x) !!\n", value); */
	if ((((value >> 12) & 0x03) != 0x01) &&
		(value != 0xDEADBEEF)) {
		mtk_drcc_enable(0);
		result = 0;
	} else
		result = 1;
	mtk_drcc_unlock(&flags);

	return result;
}

void mtk_drcc_init(void)
{
	drcc_debug("DRCC init in kernel !!\n");
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_INIT, 0, 0, 0);
	drcc_debug("DRCC init in kernel leave !!!!!!!\n");
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_enable(unsigned int onOff)
{
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_ENABLE,
		onOff, 0, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_trig(unsigned int onOff, unsigned int value)
{
	onOff = (onOff) ? 1 : 0;
	value = (value) ? 1 : 0;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_TRIG,
		onOff, value, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_count(unsigned int onOff, unsigned int value)
{
	onOff = (onOff) ? 1 : 0;
	value = (value) ? 1 : 0;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_COUNT,
		onOff, value, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_mode(unsigned int value)
{
	value = (value > 4) ? 0 : value;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_MODE,
		value, 0, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_code(unsigned int value)
{
	value = (value > 63) ? 0 : value;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_CODE,
		value, 0, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_hwgatepct(unsigned int value)
{
	value = (value > 7) ? 3 : value;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_HWGATEPCT,
		value, 0, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_vreffilt(unsigned int value)
{
	value = (value > 7) ? 0 : value;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_VREFFILT,
		value, 0, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_autocalibdelay(unsigned int value)
{
	value = (value > 15) ? 0 : value;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_AUTOCALIBDELAY,
		value, 0, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_forcetrim(unsigned int onOff, unsigned int value)
{
	onOff = (onOff) ? 1 : 0;
	value = (value > 15) ? 0 : value;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_FORCETRIM,
		onOff, value, 0);
	mtk_drcc_log2RamConsole();
}

void mtk_drcc_protect(unsigned int value)
{
	value = (value) ? 1 : 0;
	mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_PROTECT,
		value, 0, 0);
	mtk_drcc_log2RamConsole();
}

static enum hrtimer_restart drcc_timer_log_func(struct hrtimer *timer)
{
	unsigned int i, value[4];

	for (i = 0; i < 4; i++)
		value[i] = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
			DRCC_CONF0 + (i * 4), 0, 0);

	drcc_debug("[0x%x], [0x%x], [0x%x], [0x%x]\n",
		value[0], value[1], value[2], value[3]);

	hrtimer_forward_now(timer, ns_to_ktime(LOG_INTERVAL));
	return HRTIMER_RESTART;
}

static int drcc_enable_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "%d\n", (status & 0x01));

	return 0;
}

static ssize_t drcc_enable_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		drcc_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	switch (enable) {
	case 0:
		drcc_debug("Disable DRCC !!\n");
		mtk_drcc_enable(0);
		break;

	case 6771:
		drcc_debug("Enable DRCC !!\n");
		mtk_drcc_enable(1);
		break;

	default:
		break;
	}

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_trig_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "%s\n", (status & 0x10) ? "enable" : "disable");
	seq_printf(m, "%s\n", (status & 0x20) ? "clock gate" : "comparator");

	return 0;
}

static ssize_t drcc_trig_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &enable, &value) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_trig(enable, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_count_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "%s\n", (status & 0x40) ? "enable" : "disable");
	seq_printf(m, "%s\n", (status & 0x80) ? "clock gate" : "comparator");

	return 0;
}

static ssize_t drcc_count_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &enable, &value) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_count(enable, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_mode_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "drcc_mode = %x\n", (status >> 12) & 0x07);

	return 0;
}

static ssize_t drcc_mode_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &value)) {
		drcc_debug("bad argument!! Should be 0~4\n");
		goto out;
	}

	mtk_drcc_mode((unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_code_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "drcc_code = %x\n", (status >> 16) & 0x3F);

	return 0;
}

static ssize_t drcc_code_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &value)) {
		drcc_debug("bad argument!! Should be 0~63\n");
		goto out;
	}

	mtk_drcc_code((unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_hwgatepct_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "drcc_hwgatepct = %x\n", (status >> 24) & 0x07);

	return 0;
}

static ssize_t drcc_hwgatepct_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &value)) {
		drcc_debug("bad argument!! Should be 0~7\n");
		goto out;
	}

	mtk_drcc_hwgatepct((unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_vreffilt_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF0, 0, 0);

	seq_printf(m, "drcc_vreffilt = %x\n", (status >> 28) & 0x07);

	return 0;
}

static ssize_t drcc_vreffilt_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &value)) {
		drcc_debug("bad argument!! Should be 0~7\n");
		goto out;
	}

	mtk_drcc_vreffilt((unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_autocalibdelay_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF3, 0, 0);

	seq_printf(m, "drcc_autocalibdelay = %x\n", status & 0x0F);

	return 0;
}

static ssize_t drcc_autocalibdelay_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &value)) {
		drcc_debug("bad argument!! Should be 0~15\n");
		goto out;
	}

	mtk_drcc_autocalibdelay((unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_forcetrim_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF3, 0, 0);

	seq_printf(m, "%s\n", (status & 0x100) ? "enable" : "disable");
	seq_printf(m, "0x%x\n", (status >> 4) & 0x0F);

	return 0;
}

static ssize_t drcc_forcetrim_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int enable = 0, value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (sscanf(buf, "%u %u", &enable, &value) != 2) {
		drcc_debug("bad argument!! Should input 2 arguments.\n");
		goto out;
	}

	mtk_drcc_forcetrim(enable, value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_protect_proc_show(struct seq_file *m, void *v)
{
	int status = 0;

	status = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
				DRCC_CONF3, 0, 0);

	seq_printf(m, "Disable auto protect = %x\n", (status >> 9) & 0x01);

	return 0;
}

static ssize_t drcc_protect_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int value = 0;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &value)) {
		drcc_debug("bad argument!! Should be 0~1\n");
		goto out;
	}

	mtk_drcc_protect((unsigned int)value);

out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_timer_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", drcc_timer_en);
	return 0;
}

static ssize_t drcc_timer_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int ret = -EINVAL;
	int enable;
	char *buf = (char *) __get_free_page(GFP_USER);

	if (!buf)
		return -ENOMEM;

	if (count >= PAGE_SIZE)
		goto out;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	if (kstrtoint(buf, 10, &enable)) {
		drcc_debug("bad argument!! Should be \"0\" or \"1\"\n");
		goto out;
	}

	ret = 0;
	switch (enable) {
	case 0:
		drcc_debug("Disable DRCC timer!!\n");
		drcc_timer_en = 0;
		hrtimer_cancel(&drcc_timer_log);
		break;

	case 1:
	default:
		drcc_debug("Enable DRCC timer!!\n");
		drcc_timer_en = 1;
		hrtimer_start(&drcc_timer_log, ns_to_ktime(LOG_INTERVAL), HRTIMER_MODE_REL);
		break;
	}
out:
	free_page((unsigned long)buf);
	return count;
}

static int drcc_reg_dump_proc_show(struct seq_file *m, void *v)
{
	int value = 0;
	unsigned int i;

	drcc_debug("dump drcc whole register !!\n");

	for (i = DRCC_CONF0; i <= DRCC_CONF3; i += 4) {
		value = mt_secure_call_drcc(MTK_SIP_KERNEL_DRCC_READ,
			i, 0, 0);
		seq_printf(m, "reg %x = 0x%X\n", i, value);
	}

	return 0;
}

#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode,		\
		struct file *file)					\
	{								\
		return single_open(file, name ## _proc_show,		\
			PDE_DATA(inode));				\
	}								\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

PROC_FOPS_RW(drcc_enable);
PROC_FOPS_RW(drcc_trig);
PROC_FOPS_RW(drcc_count);
PROC_FOPS_RW(drcc_mode);
PROC_FOPS_RW(drcc_code);
PROC_FOPS_RW(drcc_hwgatepct);
PROC_FOPS_RW(drcc_vreffilt);
PROC_FOPS_RW(drcc_autocalibdelay);
PROC_FOPS_RW(drcc_forcetrim);
PROC_FOPS_RW(drcc_protect);
PROC_FOPS_RW(drcc_timer);
PROC_FOPS_RO(drcc_reg_dump);

static int create_procfs(void)
{
	struct proc_dir_entry *drcc_dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	struct pentry drcc_entries[] = {
		PROC_ENTRY(drcc_enable),
		PROC_ENTRY(drcc_trig),
		PROC_ENTRY(drcc_count),
		PROC_ENTRY(drcc_mode),
		PROC_ENTRY(drcc_code),
		PROC_ENTRY(drcc_hwgatepct),
		PROC_ENTRY(drcc_vreffilt),
		PROC_ENTRY(drcc_autocalibdelay),
		PROC_ENTRY(drcc_forcetrim),
		PROC_ENTRY(drcc_protect),
		PROC_ENTRY(drcc_timer),
		PROC_ENTRY(drcc_reg_dump),
	};

	drcc_dir = proc_mkdir("drcc", NULL);
	if (!drcc_dir) {
		drcc_debug("[%s]: mkdir /proc/drcc failed\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(drcc_entries); i++) {
		if (!proc_create(drcc_entries[i].name,
			S_IRUGO | S_IWUSR | S_IWGRP,
			drcc_dir,
			drcc_entries[i].fops)) {
			drcc_debug("[%s]: create /proc/drcc/%s failed\n",
				__func__,
				drcc_entries[i].name);
			return -3;
		}
	}
	return 0;
}

static int drcc_probe(struct platform_device *pdev)
{
	#if 0 /* def CONFIG_MTK_RAM_CONSOLE */
	_mt_drcc_aee_init();
	#endif
	return 0;
}

static int drcc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int drcc_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver drcc_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= drcc_probe,
	.suspend	= drcc_suspend,
	.resume		= drcc_resume,
	.driver		= {
		.name   = "mt-drcc",
	},
};

static int __init drcc_init(void)
{
	int err = 0;

	/* init timer for log */
	hrtimer_init(&drcc_timer_log, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drcc_timer_log.function = drcc_timer_log_func;

	create_procfs();

	err = platform_driver_register(&drcc_driver);
	if (err) {
		drcc_debug("DRCC driver callback register failed..\n");
		return err;
	}
	return 0;
}

static void __exit drcc_exit(void)
{
	drcc_debug("drcc de-initialization\n");
}

late_initcall(drcc_init);

MODULE_DESCRIPTION("MediaTek DRCC Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_DRCC_C__
