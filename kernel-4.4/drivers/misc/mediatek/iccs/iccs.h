
#ifndef __ICCS_H__
#define __ICCS_H__

#include <linux/proc_fs.h>

#define ICCS_GOV_NAME_LEN	16
#define ICCS_MIN_SAMPLING_RATE	40ULL
#define MS_TO_NS(msec)		((msec) * 1000 * 1000ULL)

#define ICCS_GET_CURR_STATE	1
#define ICCS_GET_TARGET_STATE	2
#define ICCS_SET_TARGET_STATE	3
#define ICCS_SET_CACHE_SHARED	4

#define ALL_CLUSTER_ICCS_DISABLE 0x0	/* For PowerSave & Ondemand Mode */

/* PROCFS */
#define PROC_FOPS_RW(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
	.write          = name ## _proc_write,				\
}

#define PROC_FOPS_RO(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

enum transition {
	ONDEMAND = 0,
	PERFORMANCE,
	POWERSAVE,
	BENCHMARK,
	LAST_TRANSITION
};

enum iccs_state {
	UNINITIALIZED = 0,
	POWER_OFF_CACHE_SHARED_DISABLED,
	POWER_ON_CACHE_SHARED_DISABLED,
	POWER_ON_CACHE_SHARED_ENABLED,
};

struct iccs_governor {
	unsigned int enabled;
	unsigned int enabled_before_suspend;
	unsigned int nr_cluster;
	unsigned int shared_cluster_freq;
	unsigned long dram_bw_threshold;
	unsigned long l2c_hitrate_threshold;
	struct task_struct *task;
	struct hrtimer hr_timer;
	ktime_t sampling;
	spinlock_t spinlock;
	enum transition policy;
	unsigned char iccs_cache_shared_useful_bitmask;
};

struct iccs_cluster_info {
	unsigned int cache_shared_supported;
	enum iccs_state state;
};

/* Governor API */
int governor_activation(struct iccs_governor *gov);
void policy_update(struct iccs_governor *gov, enum transition new_policy);
void sampling_update(struct iccs_governor *gov, unsigned long long rate);

#endif

