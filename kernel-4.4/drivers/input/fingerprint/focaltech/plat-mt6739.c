/**
 * plat-mt6755.c
 *
**/

#include <linux/stddef.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#include<linux/spi/spi.h>
#include <linux/platform_data/spi-mt65xx.h>


#if !defined(CONFIG_MTK_CLKMGR)
# include <linux/clk.h>
#else
# include <mach/mt_clkmgr.h>
#endif

#include "ff_log.h"

# undef LOG_TAG
#define LOG_TAG "mt6755"

/* TODO: */
#define FF_COMPATIBLE_NODE_1 "mediatek,focal_fp"
#define FF_COMPATIBLE_NODE_2 "mediatek,goodix_fp"
#define FF_COMPATIBLE_NODE_3 "mediatek,mt6739-spi"

/* Define pinctrl state types. */
typedef enum {
    FF_PINCTRL_STATE_CS_CLR,
    FF_PINCTRL_STATE_CS_ACT,
    FF_PINCTRL_STATE_MI_CLR,
    FF_PINCTRL_STATE_MI_ACT,
    FF_PINCTRL_STATE_MO_CLR,
    FF_PINCTRL_STATE_MO_ACT,
    FF_PINCTRL_STATE_CLK_CLR,
    FF_PINCTRL_STATE_CLK_ACT,
    FF_PINCTRL_STATE_RST_CLR,
    FF_PINCTRL_STATE_RST_ACT,
    FF_PINCTRL_STATE_INT,
    FF_PINCTRL_STATE_INT_CLR,
    FF_PINCTRL_STATE_INT_SET,
    FF_PINCTRL_STATE_POWER_ON,
    FF_PINCTRL_STATE_POWER_OFF,
    FF_PINCTRL_STATE_MAXIMUM /* Array size */
} ff_pinctrl_state_t;
static const char *g_pinctrl_state_names[FF_PINCTRL_STATE_MAXIMUM] = {
    "spi_cs_clr",
    "spi_cs_set",
    "spi_mi_clr",
    "spi_mi_set",
    "spi_mo_clr",
    "spi_mo_set",
    "spi_mclk_clr",
    "spi_mclk_set",
    "finger_rst_clr",
    "finger_rst_set",
    "eint",
    "eint_clr",
    "eint_set",
    "power_on",
    "power_off",
};


extern struct spi_master *spi_ctl;

/* Native context and its singleton instance. */
typedef struct {
    struct pinctrl *pinctrl;
    struct pinctrl_state *pin_states[FF_PINCTRL_STATE_MAXIMUM];
#if !defined(CONFIG_MTK_CLKMGR)
    struct clk *spiclk;
#endif
    bool b_spiclk_enabled;
} ff_mt6755_context_t;
static ff_mt6755_context_t ff_mt6755_context, *g_context = &ff_mt6755_context;

int ff_ctl_init_pins(int *irq_num)
{
    int err = 0, i;
    struct device_node *dev_node = NULL;
    struct platform_device *pdev = NULL;
    FF_LOGI("'%s' enter.", __func__);

    /* Find device tree node. */
    dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_1);
    if (!dev_node) {
        FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_1);
        return (-ENODEV);
    }

    /* Convert to platform device. */
    pdev = of_find_device_by_node(dev_node);
    if (!pdev) {
        FF_LOGE("of_find_device_by_node(..) failed.");
        return (-ENODEV);
    }

    /* Retrieve the pinctrl handler. */
    g_context->pinctrl = devm_pinctrl_get(&pdev->dev);
    if (!g_context->pinctrl) {
        FF_LOGE("devm_pinctrl_get(..) failed.");
        return (-ENODEV);
    }

    /* Register all pins. */
    for (i = 0; i < FF_PINCTRL_STATE_MAXIMUM; ++i) {
        g_context->pin_states[i] = pinctrl_lookup_state(g_context->pinctrl, g_pinctrl_state_names[i]);
        if (!g_context->pin_states[i]) {
            FF_LOGE("can't find pinctrl state for '%s'.", g_pinctrl_state_names[i]);
            err = (-ENODEV);
            break;
        }
    }
    if (i < FF_PINCTRL_STATE_MAXIMUM) {
        return (-ENODEV);
    }

    /* Initialize the SPI pins. */
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_CS_ACT]);
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_CLK_ACT]);
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_MO_ACT]);
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_MI_ACT]);

    /* Initialize the INT pin. */
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_INT]);

    /* Retrieve the irq number. */
    dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_2);
    if (!dev_node) {
        FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_2);
        return (-ENODEV);
    }
    *irq_num = irq_of_parse_and_map(dev_node, 0);
    FF_LOGD("irq number is %d.", *irq_num);

#if !defined(CONFIG_MTK_CLKMGR)
    //
    // Retrieve the clock source of the SPI controller.
    //

    /* 3-1: Find device tree node. */
    dev_node = of_find_compatible_node(NULL, NULL, FF_COMPATIBLE_NODE_3);
    if (!dev_node) {
        FF_LOGE("of_find_compatible_node(.., '%s') failed.", FF_COMPATIBLE_NODE_3);
        return (-ENODEV);
    }

    /* 3-2: Convert to platform device. */
    pdev = of_find_device_by_node(dev_node);
    if (!pdev) {
        FF_LOGE("of_find_device_by_node(..) failed.");
        return (-ENODEV);
    }
    FF_LOGD("spi controller name: %s.", pdev->name);

    /* 3-3: Retrieve the SPI clk handler. */
    g_context->spiclk = devm_clk_get(&pdev->dev, "spi-clk");
    if (!g_context->spiclk) {
        FF_LOGE("devm_clk_get(..) failed.");
        return (-ENODEV);
    }
#endif
    /* Initialize the RESET pin. */
    pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_ACT]);

    FF_LOGI("'%s' leave.", __func__);
    return err;
}

int ff_ctl_free_pins(void)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);

    // TODO:

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ff_ctl_enable_spiclk(bool on)
{
    int err = 0;

    FF_LOGI("'%s' enter.", __func__);
    FF_LOGD("clock: '%s'.", on ? "enable" : "disabled");

    if (unlikely(!g_context->spiclk)) {
        return (-ENOSYS);
    }
    FF_LOGI("b_spiclk_enabled = %d.", g_context->b_spiclk_enabled);
    /* Control the clock source. */
    if (on && !g_context->b_spiclk_enabled) {
        err = clk_prepare_enable(g_context->spiclk);
        if (err) {
            FF_LOGE("clk_prepare_enable(..) = %d.", err);
        }
        g_context->b_spiclk_enabled = true;
    } else if (!on && g_context->b_spiclk_enabled) {
        clk_disable_unprepare(g_context->spiclk);
        g_context->b_spiclk_enabled = false;
    }

    FF_LOGI("'%s' leave.", __func__);
    return err;
}

int ff_ctl_enable_power(bool on)
{
    int err = 0;
    FF_LOGI("'%s' enter.", __func__);
    FF_LOGD("power: '%s'.", on ? "on" : "off");

    if (unlikely(!g_context->pinctrl)) {
        return (-ENOSYS);
    }

    if (on) {
     //   err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_POWER_ON]);
    } else {
     //   err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_POWER_OFF]);
    }

    FF_LOGI("'%s' leave.", __func__);
    return err;
}

int ff_ctl_reset_device(void)
{
    int err = 0;
    FF_LOGV("'%s' enter.", __func__);

    if (unlikely(!g_context->pinctrl)) {
        return (-ENOSYS);
    }

    /* 3-1: Pull down RST pin. */
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_CLR]);

    /* 3-2: Delay for 10ms. */
    mdelay(10);

    /* Pull up RST pin. */
    err = pinctrl_select_state(g_context->pinctrl, g_context->pin_states[FF_PINCTRL_STATE_RST_ACT]);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

const char *ff_ctl_arch_str(void)
{
    return CONFIG_MTK_PLATFORM;
}

