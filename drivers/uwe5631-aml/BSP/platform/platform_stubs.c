/*
 * Platform compatibility stubs for missing platform-specific functions
 * These are weak symbols that can be overridden by platform implementations
 *
 * This file provides stub implementations for external platform-specific
 * functions that are not available in the vanilla Linux kernel.
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/sched.h>

/* Export sched_setscheduler if not available (kernel 6.x changed this) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
/* In kernel 5.9+, use sched_set_fifo instead */
#include <linux/sched/prio.h>
int sched_setscheduler(struct task_struct *p, int policy,
		       const struct sched_param *param)
{
	if (policy == SCHED_FIFO) {
		sched_set_fifo(p);
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(sched_setscheduler);
#endif

#ifdef CONFIG_AML_BOARD

/* Weak implementations of Amlogic platform functions */

int __weak wifi_irq_num(void)
{
	pr_debug("%s: Using stub implementation\n", __func__);
	return -1;
}
EXPORT_SYMBOL(wifi_irq_num);

int __weak wifi_irq_trigger_level(void)
{
	pr_debug("%s: Using stub implementation\n", __func__);
	return 0; /* Default: low level */
}
EXPORT_SYMBOL(wifi_irq_trigger_level);

void __weak extern_wifi_set_enable(int is_on)
{
	pr_debug("%s: Using stub implementation, is_on=%d\n", __func__, is_on);
}
EXPORT_SYMBOL(extern_wifi_set_enable);

void __weak sdio_reinit(void)
{
	pr_debug("%s: Using stub implementation\n", __func__);
}
EXPORT_SYMBOL(sdio_reinit);

void __weak sdio_set_max_regs(unsigned int size)
{
	pr_debug("%s: Using stub implementation, size=0x%x\n", __func__, size);
}
EXPORT_SYMBOL(sdio_set_max_regs);

#endif /* CONFIG_AML_BOARD */

/* mmc_power_up is an internal MMC function, provide stub */
void __weak mmc_power_up(struct mmc_host *host, u32 ocr)
{
	pr_debug("%s: Using stub implementation\n", __func__);
	/* In a real implementation, this would power up the MMC host */
}
EXPORT_SYMBOL(mmc_power_up);
