

#include <linux/module.h>
#include <linux/i2c.h>
#include <mach/mtk_6306_gpio.h>
#include <mt-plat/mtk_gpio.h>
#include <linux/delay.h>
#include "mt6306_gpio_cfg.h"

/***************** MACRO Definition******************************/
#define MT6306_GPIOTAG                "[MT6306_GPIO] "
#define MT6306_GPIOLOG(fmt, arg...)   pr_debug(MT6306_GPIOTAG fmt, ##arg)
#define MT6306_GPIOMSG(fmt, arg...)   pr_warn(MT6306_GPIOTAG fmt, ##arg)

#define MT6306_I2C_NUMBER			0
#define ERR_MT6306_CLIENT_INVALID		0xFF
#define ERR_MT6306_GPIO_INVALID			0xFE
#define MT6306_MAX_PIN				18/* GPIO:1~12; GPI:13~18 */
#define MT6306_LOWPOWER_SUPPORT			1/* 1:support lowpower;0,support clk default on */

/*----------------------------------------------------------------------------*/
static struct mutex sim_gpio_lock;
static struct i2c_client *mt6306_i2c_client;
static const struct i2c_device_id mt6306_i2c_id[] = { {"mt6306", 0}, {} };
/*#define MT6306_TEST*/

/*static struct i2c_board_info __initdata i2c_6306 = { I2C_BOARD_INFO("mt6306", 0x64)};*/
/*----------------------------------------------------------------------------*/
void MT6306_Write_Register8(unsigned char var, unsigned char addr)
{
	char buffer[2] = {0};
	int ret = 0;

	buffer[0] = addr;
	buffer[1] = var;
	ret = i2c_master_send(mt6306_i2c_client, buffer, 2);
	if (ret < 0)
		MT6306_GPIOLOG("Write_Register8-i2c send err: %d\n", ret);
}

unsigned char MT6306_Read_Register8(unsigned char addr)
{
	unsigned char var = 0;
	int ret = 0;

	ret = i2c_master_send(mt6306_i2c_client, &addr, 1);
	if (ret < 0)
		MT6306_GPIOLOG("Read_Register8-i2c send err: %d\n", ret);

	ret = i2c_master_recv(mt6306_i2c_client, &var, 1);
	if (ret < 0)
		MT6306_GPIOLOG("Read_Register8-i2c recv err: %d\n", ret);

	return var;
}

#if MT6306_LOWPOWER_SUPPORT
static int on_cnt;
static int off_cnt;
static void MT6306_clk_on(void)
{
	MT6306_Write_Register8(0x00, 0x09);
	MT6306_GPIOLOG("mt6306 clock enable--->[%d]\n", on_cnt++);
}

static void MT6306_clk_off(void)
{
	MT6306_Write_Register8(0x0F, 0x09);
	MT6306_GPIOLOG("mt6306 clock disable--->[%d]\n", off_cnt++);
}
#endif


/*---------------------------------------------------------------------------*/
int mt6306_set_gpio_dir(unsigned long pin, unsigned long dir)
{
	unsigned char val = 0;
	unsigned char mask = 0;
	unsigned char addr = 0;

	if (mt6306_i2c_client == NULL) {
		MT6306_GPIOMSG("MT6306 GPIO Driver get input value is not ready!!\n");
		return -1;
	}

	/* for check the GPIO valid */
	if (!pin || pin > (sizeof(mtk_pin_6306_dir)/sizeof(struct mtk_pin_info))) {
		MT6306_GPIOMSG("MT6306 Invalid GPIO%ld Pin\n", pin);
		return -2;
	}

	addr = mtk_pin_6306_dir[pin-1].offset;
	mask = 0x01 << mtk_pin_6306_dir[pin-1].bit;
	mutex_lock(&sim_gpio_lock);
#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_on();
#endif
	val = MT6306_Read_Register8(addr);
	val &= ~(mask);
	if (dir == GPIO_DIR_IN)
		val |= ((0x0 << mtk_pin_6306_dir[pin-1].bit) & mask);
	else
		val |= ((0x1 << mtk_pin_6306_dir[pin-1].bit) & mask);
	MT6306_Write_Register8(val, addr);
#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();
#endif
	mutex_unlock(&sim_gpio_lock);
	MT6306_GPIOLOG("set dir (%ld, 0x%x, %ld) done\n", pin, addr, dir);

	return 0;
}

/*---------------------------------------------------------------------------*/
unsigned char mt6306_get_gpio_dir(unsigned long pin)
{
	unsigned char val = 0;
	unsigned char mask = 0;
	unsigned char addr = 0;

	if (mt6306_i2c_client == NULL) {
		MT6306_GPIOMSG("MT6306 GPIO Driver get input value is not ready!!\n");
		return ERR_MT6306_CLIENT_INVALID;
	}

	if (!pin || pin > (sizeof(mtk_pin_6306_dir)/sizeof(struct mtk_pin_info))) {
		MT6306_GPIOMSG("MT6306 Invalid GPIO%ld Pin\n", pin);
		return ERR_MT6306_GPIO_INVALID;
	}

	addr = mtk_pin_6306_dir[pin-1].offset;
	mask = 0x01 << mtk_pin_6306_dir[pin-1].bit;
	mutex_lock(&sim_gpio_lock);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_on();
	#endif
	val = MT6306_Read_Register8(addr);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();
	#endif
	mutex_unlock(&sim_gpio_lock);
	val &= (mask);
	val = val >> mtk_pin_6306_dir[pin-1].bit;
	MT6306_GPIOLOG("get dir (%ld, 0x%x %d) done\n", pin, addr, (int)val);

	return val;
}

/*---------------------------------------------------------------------------*/
int mt6306_set_gpio_out(unsigned long pin, unsigned long output)
{
	unsigned char val = 0;
	unsigned char mask = 0;
	unsigned char addr = 0;

	if (mt6306_i2c_client == NULL) {
		MT6306_GPIOMSG("MT6306 GPIO Driver get input value is not ready!!\n");
		return -1;
	}

	if (!pin ||
			pin > (sizeof(mtk_pin_6306_dataout)/sizeof(struct mtk_pin_info))) {
		MT6306_GPIOMSG("MT6306 Invalid GPIO%ld Pin\n", pin);
		return -2;
	}


	addr = mtk_pin_6306_dataout[pin-1].offset;
	mask = (0x1 << mtk_pin_6306_dataout[pin-1].bit);
	mutex_lock(&sim_gpio_lock);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_on();
	#endif
	val = MT6306_Read_Register8(addr);
	val &= ~(mask);
	if (output == GPIO_OUT_ZERO)
		val |= ((0x0 << mtk_pin_6306_dataout[pin-1].bit) & mask);
	else
		val |= ((0x1 << mtk_pin_6306_dataout[pin-1].bit) & mask);
	MT6306_Write_Register8(val, addr);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();
	#endif
	mutex_unlock(&sim_gpio_lock);
	MT6306_GPIOLOG("set dout(%ld, 0x%x %ld) done\n", pin, addr, output);

	return 0;
}

/*---------------------------------------------------------------------------*/
unsigned char mt6306_get_gpio_out(unsigned long pin)
{
	unsigned char val = 0;
	unsigned char mask = 0;
	unsigned char addr = 0;

	if (mt6306_i2c_client == NULL) {
		MT6306_GPIOMSG("MT6306 GPIO Driver get input value is not ready!!\n");
		return ERR_MT6306_CLIENT_INVALID;
	}

	if (!pin ||
			pin > (sizeof(mtk_pin_6306_dataout)/sizeof(struct mtk_pin_info))) {
		MT6306_GPIOMSG("MT6306 Invalid GPIO%ld Pin\n", pin);
		return ERR_MT6306_GPIO_INVALID;
	}


	addr = mtk_pin_6306_dataout[pin-1].offset;
	mask = (0x1 << mtk_pin_6306_dataout[pin-1].bit);
	mutex_lock(&sim_gpio_lock);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_on();
	#endif
	val = MT6306_Read_Register8(addr);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();
	#endif
	mutex_unlock(&sim_gpio_lock);
	val &= (mask);
	val = val >> mtk_pin_6306_dataout[pin-1].bit;
	MT6306_GPIOLOG("get dout(%ld, 0x%x, %d) done\n", pin, addr, (int)val);

	return val;
}
/*---------------------------------------------------------------------------*/
unsigned char mt6306_get_gpio_in(unsigned long pin)
{
	unsigned char val = 0;
	unsigned char mask = 0;
	unsigned char addr = 0;

	if (mt6306_i2c_client == NULL) {
		MT6306_GPIOMSG("MT6306 GPIO Driver get input value is not ready!!\n");
		return ERR_MT6306_CLIENT_INVALID;
	}

	if (!pin || pin > MT6306_MAX_PIN) {
		MT6306_GPIOMSG("MT6306 Invalid GPIO%ld Pin\n", pin);
		return ERR_MT6306_GPIO_INVALID;
	}

	addr = mtk_pin_6306_datain[pin-1].offset;
	mask = (0x1 << mtk_pin_6306_datain[pin-1].bit);
	mutex_lock(&sim_gpio_lock);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_on();
	#endif
	val = MT6306_Read_Register8(addr);
	#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();
	#endif
	mutex_unlock(&sim_gpio_lock);
	val &= (mask);
	val = val >> mtk_pin_6306_datain[pin-1].bit;
	MT6306_GPIOLOG("get din(%ld, 0x%x, %d) done\n", pin, addr, (int)val);

	return val;
}

/*---------------------------------------------------------------------------*/
int mt6306_set_GPIO_pin_group_power(unsigned long group, unsigned long on)
{
	unsigned char val = 0;
	unsigned char mask = 0;

	MT6306_GPIOLOG("MT6306 GPIO Group:%ld, power: %ld\n", group, on);
#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_on();
#endif
	switch (group) {
	case 1:
		mask = (0x1 << 2);
		val = MT6306_Read_Register8(0x03);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 2) & mask);
		else
			val |= ((0x1 << 2) & mask);
		MT6306_Write_Register8(val, 0x03);
		break;
	case 2:
		mask = (0x1 << 3);
		val = MT6306_Read_Register8(0x03);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 3) & mask);
		else
			val |= ((0x1 << 3) & mask);
		MT6306_Write_Register8(val, 0x03);
		break;
	case 3:
		mask = (0x1 << 2);
		val = MT6306_Read_Register8(0x07);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 2) & mask);
		else
			val |= ((0x1 << 2) & mask);
		MT6306_Write_Register8(val, 0x07);
		break;
	case 4:
		mask = (0x1 << 3);
		val = MT6306_Read_Register8(0x07);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 3) & mask);
		else
			val |= ((0x1 << 3) & mask);
		MT6306_Write_Register8(val, 0x07);
		break;
	case 5:
		mask = (0x1 << 3);
		val = MT6306_Read_Register8(0x09);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 3) & mask);
		else
			val |= ((0x1 << 3) & mask);
		MT6306_Write_Register8(val, 0x09);
		break;
	case 6:
		mask = (0x1 << 2);
		val = MT6306_Read_Register8(0x09);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 2) & mask);
		else
			val |= ((0x1 << 2) & mask);
		MT6306_Write_Register8(val, 0x09);
		break;
	case 7:
		mask = (0x1 << 1);
		val = MT6306_Read_Register8(0x09);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 1) & mask);
		else
			val |= ((0x1 << 1) & mask);
		MT6306_Write_Register8(val, 0x09);
		break;
	case 8:
		mask = (0x1 << 0);
		val = MT6306_Read_Register8(0x09);
		val &= ~(mask);
		if (on == 0)
			val |= ((0x0 << 0) & mask);
		else
			val |= ((0x1 << 0) & mask);
		MT6306_Write_Register8(val, 0x09);
		break;
	default:
		MT6306_GPIOMSG("MT6306 No GPIO Group%ld Pin\n", group);
		break;
	}
#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();
#endif

	return 0;
}
#ifdef MT6306_TEST
int MT6306_test(void)
{
	unsigned long i = 0;

	MT6306_GPIOLOG("MT6306 test start!\n");
	for (i = 1; i < 13; i++) {
		mt6306_set_gpio_dir(i, GPIO_DIR_OUT);
		if (mt6306_get_gpio_dir(i) != GPIO_DIR_OUT) {
			MT6306_GPIOLOG("MT6306 dir test failed, pin is %d, set dir is %d, get dir is %d!\n",
				(int)i, GPIO_DIR_OUT, mt6306_get_gpio_dir(i));
		}
	}
	for (i = 1; i < 13; i++) {
		mt6306_set_gpio_dir(i, GPIO_DIR_IN);
		if (mt6306_get_gpio_dir(i) != GPIO_DIR_IN) {
			mdelay(10);
			MT6306_GPIOLOG("MT6306 dir test failed, pin is %d, set dir is %d, get dir is %d!\n",
			(int)i, GPIO_DIR_IN, mt6306_get_gpio_dir(i));
		}
	}
	for (i = 1; i < 13; i++) {
		mt6306_set_gpio_dir(i, GPIO_DIR_OUT);
		mt6306_set_gpio_out(i, 1);
		if (mt6306_get_gpio_out(i) != 1) {
			MT6306_GPIOLOG("MT6306 out test failed, pin is %d, set out is 1, get out is %d!\n",
				(int)i, mt6306_get_gpio_out(i));
		}
	}
	for (i = 1; i < 13; i++) {
		mt6306_set_gpio_dir(i, GPIO_DIR_OUT);
		mt6306_set_gpio_out(i, 0);
		if (mt6306_get_gpio_out(i) != 0) {
			MT6306_GPIOLOG("MT6306 dir test failed, pin is %d, set out is 0, get out is %d!\n",
			(int)i, mt6306_get_gpio_out(i));
		}
	}
	MT6306_GPIOLOG("MT6306 test success!\n");
	return 0;
}
#endif
/*---------------------------------------------------------------------------*/

static int mt6306_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	MT6306_GPIOMSG("mt6306_i2c_probe, start!\n");

	mt6306_i2c_client = client;

	/* mt6306 GPIO initial sequence */
	/* Set pin group 1,2,3,4 to GPIO mode */
	MT6306_Write_Register8(0x0F, 0x1A);

	MT6306_GPIOMSG("addr: 0x1A, value: 0x%x\n", MT6306_Read_Register8(0x1A));
	/* Power on GPIO pin group 1,2 */
	MT6306_Write_Register8(0x0C, 0x03);
	mdelay(2);
	/* Power on GPIO pin group 3,4 */
	MT6306_Write_Register8(0x0C, 0x07);
	/* Switch VIO to 1.8v */
	MT6306_Write_Register8(0x08, 0x10);
#if MT6306_LOWPOWER_SUPPORT
	MT6306_clk_off();/* disable clk after init */
	on_cnt = 0;
	off_cnt = 0;
#endif
#if 0
	/* Power select 2.8v on GPIO pin group 1,2 */
	MT6306_Write_Register8(0x0C, 0x02);

	/* Power select 2.8v on GPIO pin group 3,4 */
	MT6306_Write_Register8(0x0C, 0x06);

	/* Power select 3.0v on GPIO pin group 1,2 */
	MT6306_Write_Register8(0x0F, 0x03);

	/* Power select 3.0v on GPIO pin group 3,4 */
	MT6306_Write_Register8(0x0F, 0x07);
#endif
#ifdef MT6306_TEST
	MT6306_test();
#endif
	MT6306_GPIOMSG("mt6306_i2c_probe, end\n");
	return 0;

}

/*---------------------------------------------------------------------------*/
#if MT6306_LOWPOWER_SUPPORT
#else
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int mt6306_gpio_suspend(struct device *pdev)
{
	/* Set clock stop H/L to keep/unkeep SCLK/SRST/SIO value */
	MT6306_GPIOLOG("MT6306 GPIO suspend\n");
	MT6306_Write_Register8(0x0F, 0x09);
	return 0;
}

/*---------------------------------------------------------------------------*/
int mt6306_gpio_resume(struct device *pdev)
{
	/* Set clock stop H/L to keep/unkeep SCLK/SRST/SIO value */
	MT6306_GPIOLOG("MT6306 GPIO resume\n");
	MT6306_Write_Register8(0x00, 0x09);
	return 0;
}
SIMPLE_DEV_PM_OPS(mt6306, mt6306_gpio_suspend, mt6306_gpio_resume);

/*---------------------------------------------------------------------------*/
#endif/*CONFIG_PM */
#endif

/*---------------------------------------------------------------------------*/

static int mt6306_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct of_device_id mt6306_of_match[] = {
	{.compatible = "mediatek,mt6306",},
	{},
};

struct i2c_driver mt6306_i2c_driver = {
	.probe = mt6306_i2c_probe,
	.remove = mt6306_i2c_remove,
	.driver = {
		   .name = "mt6306",
		   .of_match_table = mt6306_of_match,
		   #if !MT6306_LOWPOWER_SUPPORT
		   .pm = &mt6306,
		   #endif
		   },
	.id_table = mt6306_i2c_id,
};

/* called when loaded into kernel */
static int __init mt6306_gpio_driver_init(void)
{
	MT6306_GPIOMSG("MT6306 GPIO driver init\n");
	mutex_init(&sim_gpio_lock);

/*    i2c_register_board_info(MT6306_I2C_NUMBER, &i2c_6306, 1);*/

	if (i2c_add_driver(&mt6306_i2c_driver) != 0) {
		MT6306_GPIOMSG("mt6306_i2c_driver initialization failed!!\n");
		return -1;
	}
	MT6306_GPIOMSG("mt6306_i2c_driver initialization succeed!!\n");
	return 0;
}

/* should never be called */
static void __exit mt6306_gpio_driver_exit(void)
{
	MT6306_GPIOLOG("MT6306 GPIO driver exit\n");
}

module_init(mt6306_gpio_driver_init);
module_exit(mt6306_gpio_driver_exit);

