/*
 * Atheros DB120 reference board support
 *
 * Copyright (c) 2011 Qualcomm Atheros
 * Copyright (c) 2011-2012 Gabor Juhos <juhosg@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <linux/pci.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/ath9k_platform.h>
#include <linux/ar8216_platform.h>
#include <linux/delay.h>
#include <linux/gpio.h>


#include <asm/mach-ath79/ar71xx_regs.h>

#include "common.h"
#include "dev-ap9x-pci.h"
#include "dev-eth.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-nfc.h"
#include "dev-spi.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "machtypes.h"

#include "dev-audio.h"
#include "ath79.h"

#define DB120_GPIO_LED_USB		1
#define DB120_GPIO_LED_WLAN_5G		2
#define DB120_GPIO_LED_WLAN_2G		3
#define DB120_GPIO_LED_STATUS		18
#define DB120_GPIO_LED_WPS		19

#define DB120_GPIO_BTN_WPS		20

#define DB120_KEYS_POLL_INTERVAL	20	/* msecs */
#define DB120_KEYS_DEBOUNCE_INTERVAL	(3 * DB120_KEYS_POLL_INTERVAL)

#define DB120_MAC0_OFFSET		0
#define DB120_MAC1_OFFSET		6
#define DB120_WMAC_CALDATA_OFFSET	0x1000
#define DB120_PCIE_CALDATA_OFFSET	0x5000

static struct gpio_led db120_leds_gpio[] __initdata = {
	{
		.name		= "db120:green:status",
		.gpio		= DB120_GPIO_LED_STATUS,
		.active_low	= 1,
	},
	{
		.name		= "db120:green:wps",
		.gpio		= DB120_GPIO_LED_WPS,
		.active_low	= 1,
	},
	{
		.name		= "db120:green:wlan-5g",
		.gpio		= DB120_GPIO_LED_WLAN_5G,
		.active_low	= 1,
	},
	{
		.name		= "db120:green:wlan-2g",
		.gpio		= DB120_GPIO_LED_WLAN_2G,
		.active_low	= 1,
	},
	{
		.name		= "db120:green:usb",
		.gpio		= DB120_GPIO_LED_USB,
		.active_low	= 1,
	}
};

static struct gpio_keys_button db120_gpio_keys[] __initdata = {
	{
		.desc		= "WPS button",
		.type		= EV_KEY,
		.code		= KEY_WPS_BUTTON,
		.debounce_interval = DB120_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= DB120_GPIO_BTN_WPS,
		.active_low	= 1,
	},
};

static struct ar8327_pad_cfg db120_ar8327_pad0_cfg = {
	.mode = AR8327_PAD_MAC_RGMII,
	.txclk_delay_en = true,
	.rxclk_delay_en = true,
	.txclk_delay_sel = AR8327_CLK_DELAY_SEL1,
	.rxclk_delay_sel = AR8327_CLK_DELAY_SEL2,
};

static struct ar8327_led_cfg db120_ar8327_led_cfg = {
	.led_ctrl0 = 0x00000000,
	.led_ctrl1 = 0xc737c737,
	.led_ctrl2 = 0x00000000,
	.led_ctrl3 = 0x00c30c00,
	.open_drain = true,
};

static struct ar8327_platform_data db120_ar8327_data = {
	.pad0_cfg = &db120_ar8327_pad0_cfg,
	.port0_cfg = {
		.force_link = 1,
		.speed = AR8327_PORT_SPEED_1000,
		.duplex = 1,
		.txpause = 1,
		.rxpause = 1,
	},
	.led_cfg = &db120_ar8327_led_cfg,
};


static struct mdio_board_info db120_mdio0_info[] = {
	{
		.bus_id = "ag71xx-mdio.0",
		.phy_addr = 0,
		.platform_data = &db120_ar8327_data,
	},
};


static struct platform_device db120_tb388_spdif_codec = { 
	.name       = "ath79-internal-codec",
	.id     = -1, 
};


#define DB120_GPIO_I2S_MCLK     15
#define DB120_GPIO_I2S_SD       12
#define DB120_GPIO_I2S_WS       13
#define DB120_GPIO_I2S_CLK      14
#define DB120_GPIO_I2S_MIC_SD       11
#define DB120_GPIO_SPDIF_OUT        4

static void __init db120_setup(void)
{
	u8 *art = (u8 *) KSEG1ADDR(0x1fff0000);
	u32 t;

	ath79_gpio_output_select(DB120_GPIO_LED_USB, AR934X_GPIO_OUT_GPIO);
	ath79_register_m25p80(NULL);

	ath79_register_leds_gpio(-1, ARRAY_SIZE(db120_leds_gpio),
				 db120_leds_gpio);
	ath79_register_gpio_keys_polled(-1, DB120_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(db120_gpio_keys),
					db120_gpio_keys);
	ath79_register_usb();
	ath79_register_wmac(art + DB120_WMAC_CALDATA_OFFSET, NULL);
	ap91_pci_init(art + DB120_PCIE_CALDATA_OFFSET, NULL);

	ath79_setup_ar934x_eth_cfg(AR934X_ETH_CFG_RGMII_GMAC0 |
				   AR934X_ETH_CFG_SW_ONLY_MODE);

	ath79_register_mdio(1, 0x0);
	ath79_register_mdio(0, 0x0);

	ath79_init_mac(ath79_eth0_data.mac_addr, art + DB120_MAC0_OFFSET, 0);

	mdiobus_register_board_info(db120_mdio0_info,
				    ARRAY_SIZE(db120_mdio0_info));

	/* GMAC0 is connected to an AR8327 switch */
	ath79_eth0_data.phy_if_mode = PHY_INTERFACE_MODE_RGMII;
	ath79_eth0_data.phy_mask = BIT(0);
	ath79_eth0_data.mii_bus_dev = &ath79_mdio0_device.dev;
	ath79_eth0_pll_data.pll_1000 = 0x06000000;
	ath79_register_eth(0);

	/* GMAC1 is connected to the internal switch */
	ath79_init_mac(ath79_eth1_data.mac_addr, art + DB120_MAC1_OFFSET, 0);
	ath79_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_GMII;
	ath79_eth1_data.speed = SPEED_1000;
	ath79_eth1_data.duplex = DUPLEX_FULL;

	ath79_register_eth(1);

	ath79_register_nfc();


	/* Reset I2S internal controller */
	t = ath79_reset_rr(AR71XX_RESET_REG_RESET_MODULE);
	ath79_reset_wr(AR71XX_RESET_REG_RESET_MODULE, t | AR934X_RESET_I2S );
	udelay(1);

	gpio_request(DB120_GPIO_I2S_CLK, "I2S CLK");
	ath79_gpio_output_select(DB120_GPIO_I2S_CLK, AR934X_GPIO_OUT_MUX_I2S_CLK);
	gpio_direction_output(DB120_GPIO_I2S_CLK, 0);

	gpio_request(DB120_GPIO_I2S_WS, "I2S WS");
	ath79_gpio_output_select(DB120_GPIO_I2S_WS, AR934X_GPIO_OUT_MUX_I2S_WS);
	gpio_direction_output(DB120_GPIO_I2S_WS, 0);

	gpio_request(DB120_GPIO_I2S_SD, "I2S SD");
	ath79_gpio_output_select(DB120_GPIO_I2S_SD, AR934X_GPIO_OUT_MUX_I2S_SD);
	gpio_direction_output(DB120_GPIO_I2S_SD, 0);

	if (IS_ENABLED(CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK)) {
		gpio_request(DB120_GPIO_I2S_MCLK, "I2S MCLK");
		ath79_gpio_input_select(DB120_GPIO_I2S_MCLK, AR934X_GPIO_IN_MUX_I2S_MCK);
		gpio_direction_input(DB120_GPIO_I2S_MCLK);
	} else {
		gpio_request(DB120_GPIO_I2S_MCLK, "I2S MCLK");
		ath79_gpio_output_select(DB120_GPIO_I2S_MCLK, AR934X_GPIO_OUT_MUX_I2S_MCK);
		gpio_direction_output(DB120_GPIO_I2S_MCLK, 0);
	}

	gpio_request(DB120_GPIO_SPDIF_OUT, "SPDIF OUT");
	ath79_gpio_output_select(DB120_GPIO_SPDIF_OUT, AR934X_GPIO_OUT_MUX_SPDIF_OUT);
	gpio_direction_output(DB120_GPIO_SPDIF_OUT, 0);

	gpio_request(DB120_GPIO_I2S_MIC_SD, "I2S MIC_SD");
	ath79_gpio_input_select(DB120_GPIO_I2S_MIC_SD, AR934X_GPIO_IN_MUX_I2S_MIC_SD);
	gpio_direction_input(DB120_GPIO_I2S_MIC_SD);

	ath79_audio_setup();

	platform_device_register(&db120_tb388_spdif_codec);
	ath79_audio_device_register();
}

MIPS_MACHINE(ATH79_MACH_DB120, "DB120", "Atheros DB120 reference board",
	     db120_setup);
