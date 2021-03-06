
#include "xhci.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>

/*#define CONFIG_MTK_OTG_PMIC_BOOST_5V 1*/

#undef DRV_Reg32
#undef DRV_WriteReg32
#include <mt-plat/battery_meter.h>
#include <mt-plat/mtk_gpio.h>
#include "xhci-mtk-power.h"

#define CON_HOST_DEV  0

int g_num_u3_port;
int g_num_u2_port;


#if CON_HOST_DEV
/*filter those ports assigned to device*/
int getU3PortNumber(void)
{
	int port_num;
	int real_port_num;
	int i, temp;

	/*check if any port assigned to device*/
	port_num = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	real_port_num = port_num;
	for (i = 0; i < port_num; i++) {
		temp = readl(SSUSB_U3_CTRL(i));
		if (!(temp & SSUSB_U3_PORT_HOST_SEL))
			real_port_num--;
	}
	return real_port_num;
}

/*filter those ports assigned to device*/
int getU2PortNumber(void)
{
	int port_num;
	int real_port_num;
	int i, temp;

	/*check if any port assigned to device*/
	port_num = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));
	real_port_num = port_num;
	for (i = 0; i < port_num; i++) {
		temp = readl(SSUSB_U2_CTRL(i));
		if (!(temp & SSUSB_U2_PORT_HOST_SEL))
			real_port_num--;
	}
	return real_port_num;
}

/*filter those ports assigned to device*/
int getRealPortIndex(int port_index, int port_rev)
{
	int real_port_index, tmp_port_index;
	int i, temp;
	int portNum;

	real_port_index = 0;
	tmp_port_index = 0;
	if (port_rev == 0x3) {
		/*SS port*/
		portNum = getU3PortNumber();
		for (i = 0; i < portNum; i++) {
			temp = SSUSB_U3_CTRL(i);
			tmp_port_index++;
			if (temp & SSUSB_U3_PORT_HOST_SEL) {
				real_port_index++;
				if (real_port_index == port_index)
					return tmp_port_index;
			}
		}
	} else {
		/*HS port*/
		portNum = getU2PortNumber();
		for (i = 0; i < portNum; i++) {
			temp = SSUSB_U2_CTRL(i);
			tmp_port_index++;
			if (temp & SSUSB_U2_PORT_HOST_SEL) {
				real_port_index++;
				if (real_port_index == port_index)
					return tmp_port_index;
			}
		}
	}
	return port_index;
}

#endif

/* set 1 to PORT_POWER of PORT_STATUS register of each port */
void mtktest_enableXhciAllPortPower(struct xhci_hcd *xhci)
{
	int i;
	u32 port_id, temp;
	u32 __iomem *addr;

	#if CON_HOST_DEV
	g_num_u3_port = getU3PortNumber();
	g_num_u2_port = getU2PortNumber();
	#else
	g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	g_num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));
	#endif

	for (i = 1; i <= g_num_u3_port; i++) {
		port_id = i;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((port_id-1) & 0xff);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp |= PORT_POWER;
		xhci_writel(xhci, temp, addr);
		temp = xhci_readl(xhci, addr);
	}
	for (i = 1; i <= g_num_u2_port; i++) {
		port_id = i + g_num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((port_id-1) & 0xff);
		temp = xhci_readl(xhci, addr);

		/*pr_err("[OTG_H]mtktest_enableXhciAllPortPower , read port_status_base: %x\n", temp);*/

		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp |= PORT_POWER;
		xhci_writel(xhci, temp, addr);
		temp = xhci_readl(xhci, addr);
	}
	mdelay(30);
#if TEST_OTG
#ifdef CONFIG_MTK_BQ25896_SUPPORT
	mtktest_xhci_vbus_on();
	bq25890_otg_en(0x01);
	bq25890_set_boost_ilim(0x03);
#endif
#endif

/*#endif*/
	pr_err("[OTG_H]mtktest_enableXhciAllPortPower, done g_num_u3_port: %d, g_num_u2_port: %d\n",
	g_num_u3_port, g_num_u2_port);
}

/* set 0 to PORT_POWER of PORT_STATUS register of each port */
void mtktest_disableXhciAllPortPower(struct xhci_hcd *xhci)
{
	int i;
	u32 port_id, temp;
	u32 __iomem *addr;

	pr_err("[OTG_H]mtktest_disableXhciAllPortPower start\n");

	#if CON_HOST_DEV
		g_num_u3_port = getU3PortNumber();
		g_num_u2_port = getU2PortNumber();
	#else
		g_num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
		g_num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));
	#endif

	for (i = 1; i <= g_num_u3_port; i++) {
		port_id = i;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((port_id-1) & 0xff);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		xhci_writel(xhci, temp, addr);
		while (xhci_readl(xhci, addr) & PORT_POWER)
			;
	}

	for (i = 1; i <= g_num_u2_port; i++) {
		port_id = i+g_num_u3_port;
		addr = &xhci->op_regs->port_status_base + NUM_PORT_REGS*((port_id-1) & 0xff);
		temp = xhci_readl(xhci, addr);
		temp = mtktest_xhci_port_state_to_neutral(temp);
		temp &= ~PORT_POWER;
		xhci_writel(xhci, temp, addr);
		while (xhci_readl(xhci, addr) & PORT_POWER)
			;
	}

	mdelay(30);
#if TEST_OTG
#ifdef CONFIG_MTK_BQ25896_SUPPORT
	bq25890_otg_en(0x0);
	mtktest_xhci_vbus_off();
	/* mt_set_gpio_mode(GPIO_OTG_DRVVBUS_PIN, 1); */
#endif
#endif
/*#endif*/
	pr_err("[OTG_H]mtktest_disableXhciAllPortPower done, g_num_u3_port: %d, g_num_u2_port: %d\n",
	g_num_u3_port, g_num_u2_port);

}

void mtktest_resetIP(void)
{
	writel(readl(SSUSB_IP_PW_CTRL) | (SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
	writel(readl(SSUSB_IP_PW_CTRL) & (~SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
	writel(readl(SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN), SSUSB_IP_PW_CTRL_1);
}
void mtktest_enableAllClockPower(void)
{
	int i;
	u32 temp;
	int num_u3_port;
	int num_u2_port;

	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));
	/*usb_phy_recover(0) ;*/

	/*2.  Enable xHC*/
#if TEST_OTG
	writel(readl(SSUSB_IP_PW_CTRL) & (~SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
	writel(readl(SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN), SSUSB_IP_PW_CTRL_1);
#else
	writel(readl(SSUSB_IP_PW_CTRL) | (SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
	writel(readl(SSUSB_IP_PW_CTRL) & (~SSUSB_IP_SW_RST), SSUSB_IP_PW_CTRL);
	writel(readl(SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN), SSUSB_IP_PW_CTRL_1);
#endif

	/*1.  Enable target ports*/
	for (i = 0; i < num_u3_port; i++) {
		temp = readl(SSUSB_U3_CTRL(i));
#if CON_HOST_DEV
		if (temp & SSUSB_U3_PORT_HOST_SEL)
			temp = temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS);
#else
		temp = (temp & (~SSUSB_U3_PORT_PDN) & (~SSUSB_U3_PORT_DIS)) | SSUSB_U3_PORT_HOST_SEL;
#endif

		writel(temp, SSUSB_U3_CTRL(i));
	}
	for (i = 0; i < num_u2_port; i++) {
		temp = readl(SSUSB_U2_CTRL(i));
#if CON_HOST_DEV
		if (temp & SSUSB_U2_PORT_HOST_SEL)
			temp = temp & (~SSUSB_U2_PORT_PDN) & (~SSUSB_U2_PORT_DIS);
#else
#if TEST_OTG
		temp = temp & (~SSUSB_U2_PORT_PDN) & (~SSUSB_U2_PORT_DIS) & (~SSUSB_U2_PORT_HOST_SEL);
		temp = temp | SSUSB_U2_PORT_OTG_SEL | SSUSB_U2_PORT_OTG_MAC_AUTO_SEL | SSUSB_U2_PORT_HOST_SEL;
#else
		temp = (temp & (~SSUSB_U2_PORT_PDN) & (~SSUSB_U2_PORT_DIS)) | SSUSB_U2_PORT_HOST_SEL;
#endif
#endif
		writel(temp, SSUSB_U2_CTRL(i));
#if 0
		writel(0xf, 0xf00447b0);
		writel((readl(0xf00447b4) & ~(0xffff) | (0x0001)), 0xf00447b4);
		writel((readl(0xf00447bc) & ~(0xff) | (0x0f)), 0xf00447bc);
#endif

	}
	msleep(100);
}

/*called after HC initiated*/
void mtktest_disableAllClockPower(void)
{
	int i;
	u32 temp;
	int num_u3_port;
	int num_u2_port;

#if TEST_OTG
		return;
#endif

	num_u3_port = SSUSB_U3_PORT_NUM(readl(SSUSB_IP_CAP));
	num_u2_port = SSUSB_U2_PORT_NUM(readl(SSUSB_IP_CAP));

	/*disable target ports*/
	for (i = 0; i < num_u3_port; i++) {
		temp = readl(SSUSB_U3_CTRL(i));
#if CON_HOST_DEV
		if (temp & SSUSB_U3_PORT_HOST_SEL)
			temp = temp | SSUSB_U3_PORT_PDN;
#else
		temp = temp | SSUSB_U3_PORT_PDN;
#endif
		writel(temp, SSUSB_U3_CTRL(i));
	}
	for (i = 0; i < num_u2_port; i++) {
		temp = readl(SSUSB_U2_CTRL(i));
#if CON_HOST_DEV
		if (temp & SSUSB_U2_PORT_HOST_SEL)
			temp = temp | SSUSB_U2_PORT_PDN;
#else
		temp = temp | SSUSB_U2_PORT_PDN;
#endif
		writel(temp, SSUSB_U2_CTRL(i));
	}
	msleep(100);
}

void mtktest_disablePortClockPower(int port_index, int port_rev)
{
	/*int i;*/
	u32 temp;
	int real_index;

#if TEST_OTG
	return;
#endif
#if CON_HOST_DEV
	real_index = getRealPortIndex(port_index, port_rev);
#else
	real_index = port_index;
#endif


	if (port_rev == 0x3) {
		temp = readl(SSUSB_U3_CTRL(real_index));
		temp = temp | (SSUSB_U3_PORT_PDN);
		writel(temp, SSUSB_U3_CTRL(real_index));
	} else if (port_rev == 0x2) {
		temp = readl(SSUSB_U2_CTRL(real_index));
		temp = temp | (SSUSB_U2_PORT_PDN);
		writel(temp, SSUSB_U2_CTRL(real_index));
	}
	writel(readl(SSUSB_IP_PW_CTRL_1) | (SSUSB_IP_PDN), SSUSB_IP_PW_CTRL_1);
}

void mtktest_enablePortClockPower(int port_index, int port_rev)
{
	/*int i;*/
	u32 temp;
	int real_index;

#if TEST_OTG
	return;
#endif
#if CON_HOST_DEV
	real_index = getRealPortIndex(port_index, port_rev);
#else
	real_index = port_index;
#endif

	writel(readl(SSUSB_IP_PW_CTRL_1) & (~SSUSB_IP_PDN), SSUSB_IP_PW_CTRL_1);

	if (port_rev == 0x3) {
		temp = readl(SSUSB_U3_CTRL(real_index));
		temp = temp & (~SSUSB_U3_PORT_PDN);
		writel(temp, SSUSB_U3_CTRL(real_index));
	} else if (port_rev == 0x2) {
		temp = readl(SSUSB_U2_CTRL(real_index));
		temp = temp & (~SSUSB_U2_PORT_PDN);
		writel(temp, SSUSB_U2_CTRL(real_index));
	}
}
