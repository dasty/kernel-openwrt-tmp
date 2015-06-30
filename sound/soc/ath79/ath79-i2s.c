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

static int ath79_i2s_startup(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	/* Enable I2S and SPDIF by default */
	if (!dai->active) {
		unsigned stereo_config = AR934X_STEREO_CONFIG_SPDIF_ENABLE |
									AR934X_STEREO_CONFIG_I2S_ENABLE |
									AR934X_STEREO_CONFIG_SAMPLE_CNT_CLEAR_TYPE |
									AR934X_STEREO_CONFIG_MASTER;

#ifdef CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK
		stereo_config |= AR934X_STEREO_CONFIG_MCK_SEL;
#endif

		ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, stereo_config);
		ath79_stereo_reset();
	}
	return 0;
}

static void ath79_i2s_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	if (!dai->active) {
		ath79_stereo_wr(AR934X_STEREO_REG_CONFIG, 0);
	}
	return;
}

static int ath79_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
			      struct snd_soc_dai *dai)
{
	return 0;
}

#ifdef CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK
/*
 * This table is only valid for a MCLK of 24576000Hz or 22579200Hz.
 * See also datasheet AR9344.pdf page 205.
 * TODO: Make this a real calculation based on the actual MCLK rate,
 *       channel size and rate.
 */
static u32 calculate_posedge(struct snd_pcm_hw_params *params)
{
	switch(params_rate(params)) {
		case 22050:
			return 8;
			break;

		case 32000:
			return 6;
			break;

		case 44100:
		case 48000:
			return 4;
			break;

		case 88200:
		case 96000:
			return 2;
			break;

		case 192000:
			return 1;
			break;

		default:
			printk("Undefined POSEDGE value for a rate of %u, using fallback value of 4\n", params_rate(params));
			return 4;
			break;
	}
}
#endif

static int ath79_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	u32 mask = 0, t;

	printk("Called %s with rate %d\n", __FUNCTION__, params_rate(params));

	if (dai->capture_active && dai->playback_active) {
		printk("Previous playback or capture running, skipping reconfiguration\n");
		return 0;
	}

#ifdef CONFIG_SND_ATH79_SOC_USE_EXTERNAL_MCLK
	/* TODO: Implement the switching of an external PLL and remove this printk() */
	printk("Using external MCLK, no PLL switching implemented.\n");

	ath79_stereo_set_posedge(calculate_posedge(params));
#else
	ath79_audio_set_freq(params_rate(params));
#endif

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
	spin_lock_init(&ath79_stereo_lock);

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
