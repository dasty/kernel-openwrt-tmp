/*
 * ath79-i2s.c -- ALSA DAI (i2s) interface for the QCA Wasp based audio interface
 *
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/pcm_params.h>

#include <asm/mach-ath79/ar71xx_regs.h>
#include <asm/mach-ath79/ath79.h>

#include "ath79-i2s.h"
#include "ath79-i2s-pll.h"

#define DRV_NAME	"ath79-i2s"

#define MCLK_RATE_48kHz		24576000UL
#define MCLK_RATE_44_1kHz	22579200UL

DEFINE_SPINLOCK(ath79_stereo_lock);

void ath79_stereo_reset(void)
{
	u32 t;

	spin_lock(&ath79_stereo_lock);
	t = ath79_stereo_rr(AR934X_STEREO_REG_CONFIG);
	t |= AR934X_STEREO_CONFIG_RESET;
	ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, t);
	spin_unlock(&ath79_stereo_lock);
}
EXPORT_SYMBOL(ath79_stereo_reset);

static void ath79_stereo_set_posedge(u32 posedge)
{
	u32 t;

	spin_lock(&ath79_stereo_lock);

	t = ath79_stereo_rr(AR934X_STEREO_REG_CONFIG);
	t &= ~(AR934X_STEREO_CONFIG_POSEDGE_MASK
		<< AR934X_STEREO_CONFIG_POSEDGE_SHIFT);
	t |= (posedge & AR934X_STEREO_CONFIG_POSEDGE_MASK)
		<< AR934X_STEREO_CONFIG_POSEDGE_SHIFT;
	ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, t);

	spin_unlock(&ath79_stereo_lock);
}

static int ath79_i2s_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	/* Enable I2S and SPDIF by default */
	if (!dai->active) {
		unsigned stereo_config = AR934X_STEREO_CONFIG_SPDIF_ENABLE |
									AR934X_STEREO_CONFIG_I2S_ENABLE |
									AR934X_STEREO_CONFIG_MASTER;

		if (IS_ENABLED(CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK))
			stereo_config |= AR934X_STEREO_CONFIG_MCK_SEL;

		ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, stereo_config);
		ath79_stereo_reset();
	}
	return 0;
}

static void ath79_i2s_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{

	if (!dai->active) {
		if (!IS_ENABLED(CONFIG_SND_ATH79_SOC_PERSISTENT_CLOCKS))
			ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, 0);
	}

	return;
}

static int ath79_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	return 0;
}

static u32 ath79_i2s_calculate_posedge(u32 mclk_rate, u32 rate)
{
	u32 posedge = mclk_rate / (rate * 32 * 2 * 2);

	printk("Using posedge of %u\n", posedge);

	return posedge;
}

static int ath79_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	u32 mask = 0, t;
	u32 rate = params_rate(params);
	u32 mclk_rate = (rate % 16000) == 0 ? MCLK_RATE_48kHz : MCLK_RATE_44_1kHz;

	printk("Called %s with rate %d\n", __FUNCTION__, params_rate(params));

	if (dai->capture_active && dai->playback_active) {
		printk("Previous playback or capture running, skipping reconfiguration\n");
		return 0;
	}

	if (IS_ENABLED(CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK)) {
		/* TODO: Implement the switching of an external PLL and remove this printk() */
		printk("Using external MCLK, no PLL switching implemented.\n");

		ath79_stereo_set_posedge(ath79_i2s_calculate_posedge(mclk_rate, rate));
	} else {
		ath79_audio_set_freq(mclk_rate);
		ath79_stereo_set_posedge(ath79_i2s_calculate_posedge(mclk_rate, rate));
	}

	switch(params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		mask |= AR934X_STEREO_CONFIG_DATA_WORD_8
			<< AR934X_STEREO_CONFIG_DATA_WORD_SIZE_SHIFT;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		mask |= AR934X_STEREO_CONFIG_PCM_SWAP;
	case SNDRV_PCM_FORMAT_S16_BE:
		mask |= AR934X_STEREO_CONFIG_DATA_WORD_16
			<< AR934X_STEREO_CONFIG_DATA_WORD_SIZE_SHIFT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		mask |= AR934X_STEREO_CONFIG_PCM_SWAP;
	case SNDRV_PCM_FORMAT_S24_BE:
		mask |= AR934X_STEREO_CONFIG_DATA_WORD_24
			<< AR934X_STEREO_CONFIG_DATA_WORD_SIZE_SHIFT;
		mask |= AR934X_STEREO_CONFIG_I2S_WORD_SIZE;
		mask |= AR934X_STEREO_CONFIG_MIC_WORD_SIZE;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		mask |= AR934X_STEREO_CONFIG_PCM_SWAP;
	case SNDRV_PCM_FORMAT_S32_BE:
		mask |= AR934X_STEREO_CONFIG_DATA_WORD_32
			<< AR934X_STEREO_CONFIG_DATA_WORD_SIZE_SHIFT;
		mask |= AR934X_STEREO_CONFIG_I2S_WORD_SIZE;
		mask |= AR934X_STEREO_CONFIG_MIC_WORD_SIZE;
		break;
	default:
		printk(KERN_ERR "%s: Format %d not supported\n",
			__FUNCTION__, params_format(params));
		return -ENOTSUPP;
	}

	spin_lock(&ath79_stereo_lock);
	t = ath79_stereo_rr(AR934X_STEREO_REG_CONFIG);
	t &= ~(AR934X_STEREO_CONFIG_DATA_WORD_SIZE_MASK
		<< AR934X_STEREO_CONFIG_DATA_WORD_SIZE_SHIFT);
	t &= ~(AR934X_STEREO_CONFIG_I2S_WORD_SIZE);
	t &= ~(AR934X_STEREO_CONFIG_MIC_WORD_SIZE);
	t |= mask;
	ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, t);
	spin_unlock(&ath79_stereo_lock);

	ath79_stereo_reset();
	return 0;
}

static struct snd_soc_dai_ops ath79_i2s_dai_ops = {
	.startup	= ath79_i2s_startup,
	.shutdown	= ath79_i2s_shutdown,
	.trigger	= ath79_i2s_trigger,
	.hw_params	= ath79_i2s_hw_params,
};

static struct snd_soc_dai_driver ath79_i2s_dai = {
	.name = "ath79-i2s",
	.id = 0,
	.playback = {
		.stream_name = "ath79-i2s playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_22050 |
				SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_88200 |
				SNDRV_PCM_RATE_96000 |
				SNDRV_PCM_RATE_192000,

		.formats = SNDRV_PCM_FMTBIT_S8 |
			SNDRV_PCM_FMTBIT_S16_BE | SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S32_BE | SNDRV_PCM_FMTBIT_S32_LE,
		},
	.capture = {
		.stream_name = "ath79-i2s capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_22050 |
				SNDRV_PCM_RATE_32000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_88200 |
				SNDRV_PCM_RATE_96000 |
				SNDRV_PCM_RATE_192000,

		.formats = SNDRV_PCM_FMTBIT_S8 |
			SNDRV_PCM_FMTBIT_S16_BE | SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S32_BE | SNDRV_PCM_FMTBIT_S32_LE,
		},
	.ops = &ath79_i2s_dai_ops,
};

static const struct snd_soc_component_driver ath79_i2s_component = {
    .name       = "ath79-i2s",
};

static int ath79_i2s_drv_probe(struct platform_device *pdev)
{
	u32 stereo_config = 0;

	spin_lock_init(&ath79_stereo_lock);

	if (IS_ENABLED(CONFIG_SND_ATH79_SOC_PERSISTENT_CLOCKS)) {
		stereo_config |= AR934X_STEREO_CONFIG_SPDIF_ENABLE |
							AR934X_STEREO_CONFIG_I2S_ENABLE |
							AR934X_STEREO_CONFIG_MASTER;

		if (IS_ENABLED(CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK))
			stereo_config |= AR934X_STEREO_CONFIG_MCK_SEL;

		ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, stereo_config);


		if (!IS_ENABLED(CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK))
			ath79_audio_set_freq(MCLK_RATE_48kHz);

		ath79_stereo_set_posedge(ath79_i2s_calculate_posedge(MCLK_RATE_48kHz, 48000));


		ath79_stereo_reset();
	}

	return devm_snd_soc_register_component(&pdev->dev, &ath79_i2s_component, &ath79_i2s_dai, 1);
}

static int ath79_i2s_drv_remove(struct platform_device *pdev)
{
	/* TODO: Unregister properly */
	/* snd_soc_unregister_dai(&pdev->dev); */
	return 0;
}

static struct platform_driver ath79_i2s_driver = {
	.probe = ath79_i2s_drv_probe,
	.remove = ath79_i2s_drv_remove,

	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ath79_i2s_driver);

MODULE_AUTHOR("Qualcomm-Atheros Inc.");
MODULE_AUTHOR("Mathieu Olivari <mathieu@qca.qualcomm.com>");
MODULE_DESCRIPTION("QCA Audio DAI (i2s) module");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:" DRV_NAME); 
