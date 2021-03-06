
#define _USB_OSAI_EXT_
#include "mu3d_hal_osal.h"
#undef _USB_OSAI_EXT_

void os_ms_delay(unsigned int ui4_delay)
{
	mdelay(ui4_delay);
}

void os_us_delay(unsigned int ui4_delay)
{
	udelay(ui4_delay);
}

void os_ms_sleep(unsigned int ui4_sleep)
{
	msleep(ui4_sleep);
}


#ifdef NEVER
void os_spin_lock(spinlock_t *lock)
{
	/* spin_lock(lock); */
}

void os_spin_unlock(spinlock_t *lock)
{
	/* spin_unlock(lock); */
}
#endif				/* NEVER */

void os_memcpy(char *pv_to, char *pv_from, size_t z_l)
{
	/*FIXME: just use memcpy(), why use this???? */
	int i;

	if ((pv_to != NULL) || (z_l == 0)) {
		for (i = 0; i < z_l; i++)
			*(pv_to + i) = *(pv_from + i);
	} else {
		WARN_ON(1);
	}
}

#ifdef NEVER
void *os_virt_to_phys(void *vaddr)
{

	return virt_to_phys(vaddr);
}
#endif				/* NEVER */

void *os_phys_to_virt(void *paddr)
{

	/* return phys_to_virt((phys_addr_t)paddr); */
	return phys_to_virt((phys_addr_t) (long)paddr);
}

#ifdef NEVER
void *os_ioremap(void *paddr, unsigned int t_size)
{

	/* return ioremap(paddr,t_size); */
	return ioremap_nocache(paddr, t_size);
}

void os_iounmap(void *vaddr)
{
	iounmap(vaddr);
	vaddr = NULL;
}
#endif				/* NEVER */


void *os_memset(void *pv_to, unsigned char ui1_c, size_t z_l)
{

	if ((pv_to != NULL) || (z_l == 0))
		return memset(pv_to, ui1_c, z_l);
	/* else */
		WARN_ON(1);

	return pv_to;
}

void *os_mem_alloc(size_t z_size)
{
	void *pv_mem = NULL;

	pv_mem = kmalloc(z_size, GFP_NOIO);

	if (pv_mem == NULL) {
		/*WARNING:OOM_MESSAGE: Possible unnecessary 'out of memory' message*/
		/*pr_err("kmalloc fail!!\n");*/
		WARN_ON(1);
	}

	return pv_mem;
}

void os_mem_free(void *pv_mem)
{

	kfree(pv_mem);
	pv_mem = NULL;
}

void os_disableIrq(unsigned int irq)
{
	disable_irq(irq);
	os_ms_delay(20);
}

void os_enableIrq(unsigned int irq)
{
	enable_irq(irq);
}

void os_clearIrq(unsigned int irq)
{
	os_writel(U3D_LV1IECR, os_readl(U3D_LV1ISR));
}


void os_get_random_bytes(void *buf, int nbytes)
{
	get_random_bytes(buf, nbytes);
}

void os_disableDcache(void)
{
	/* HalDisableDCache(); */
}

void os_flushinvalidateDcache(void)
{
	/* HalFlushInvalidateDCache(); */
}


int os_reg_isr(unsigned int irq, irq_handler_t handler, void *isrbuffer)
{
	int i4_ret;

	i4_ret = request_irq(irq, handler,	/* our handler */
			     IRQF_TRIGGER_LOW, "usb device handler", isrbuffer);

	return i4_ret;
}



void os_free_isr(unsigned int irq, void *isrbuffer)
{

	free_irq(irq, isrbuffer);
}
