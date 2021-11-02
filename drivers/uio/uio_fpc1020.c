// SPDX-License-Identifier: GPL-2.0
/*
 * UIO driver for Fingerprint Cards FPC1020 series fingerprint sensors
 * This driver manages power and reset through Runtime PM.
 * on these chips.
 *
 * Copyright (C) 2021, Yassine Oudjana <y.oudjana@protonmail.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/uio_driver.h>

static const char * const uio_fpc1020_supply_name[] = {
	"vcc",
	"vdda",
	"vddio",
};
#define FPC1020_NUM_SUPPLIES ARRAY_SIZE(uio_fpc1020_supply_name)

struct uio_fpc1020_chip {
	struct device *dev;
	struct uio_info *uioinfo;

	struct regulator_bulk_data supplies[FPC1020_NUM_SUPPLIES];
	struct gpio_desc *reset;
	int irq;
};

static int uio_fpc1020_probe(struct spi_device *spi)
{
	struct uio_fpc1020_chip *chip;
	struct uio_info *uioinfo;
	int i, irq, ret;

        chip = devm_kzalloc(&spi->dev, sizeof(*chip), GFP_KERNEL);
        if (!chip)
                return -ENOMEM;

	uioinfo = devm_kzalloc(&spi->dev, sizeof(*uioinfo), GFP_KERNEL);
        if (!uioinfo)
                return -ENOMEM;

	chip->dev = &spi->dev;
	spi_set_drvdata(spi, chip);

	chip->reset = devm_gpiod_get(chip->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(chip->reset)) {
		ret = PTR_ERR(chip->reset);
		dev_err(chip->dev, "Failed to get reset pin: %d\n", ret);
		return ret;
	}

	for (i = 0; i < FPC1020_NUM_SUPPLIES; i++)
		chip->supplies[i].supply = uio_fpc1020_supply_name[i];

	ret = devm_regulator_bulk_get(chip->dev, FPC1020_NUM_SUPPLIES,
					chip->supplies);
	if (ret) {
		dev_err(chip->dev, "Failed to get regulators: %d\n", ret);
		return ret;
	}

	irq = of_irq_get(chip->dev->of_node, 0);
	if (irq < 0)
		return irq;

	devm_pm_runtime_enable(chip->dev);

	uioinfo->name = "fpc1020";
	uioinfo->version = "0.0.1";
	uioinfo->irq = irq;
	uioinfo->irq_flags = IRQF_ONESHOT;
	chip->uioinfo = uioinfo;

	ret = devm_uio_register_device(chip->dev, chip->uioinfo);
	if (ret)
		dev_err(chip->dev, "Failed to register uio device\n");

	dev_info(chip->dev, "device registered");

	return 0;
}

static int uio_fpc1020_runtime_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct uio_fpc1020_chip *chip = spi_get_drvdata(spi);
	int ret;

	disable_irq(chip->uioinfo->irq);

	/* Power-off chip */
	ret = regulator_bulk_disable(FPC1020_NUM_SUPPLIES, chip->supplies);
	if (ret) {
		dev_err(chip->dev, "Failed to disable regulators: %d", ret);
		return ret;
	}

	/* Leave chip in reset */
	gpiod_set_value_cansleep(chip->reset, 1);

	dev_info(chip->dev, "device powered off");

	return 0;
}

static int uio_fpc1020_runtime_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct uio_fpc1020_chip *chip = spi_get_drvdata(spi);
	int ret;

	/* Power-on chip */
	ret = regulator_bulk_enable(FPC1020_NUM_SUPPLIES, chip->supplies);
	if (ret) {
		dev_err(chip->dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	/* Reset chip */
	gpiod_set_value_cansleep(chip->reset, 1);
	usleep_range(1000, 1010);
	gpiod_set_value_cansleep(chip->reset, 0);

	enable_irq(chip->uioinfo->irq);

	dev_info(chip->dev, "device powered on");

	return 0;
}

static const struct dev_pm_ops uio_fpc1020_dev_pm_ops = {
	.runtime_suspend = uio_fpc1020_runtime_suspend,
	.runtime_resume = uio_fpc1020_runtime_resume,
};

static struct of_device_id uio_fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020" },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, uio_fpc1020_of_match);

static struct spi_driver uio_fpc1020 = {
	.driver = {
		.name = "uio_fpc1020",
		.pm = &uio_fpc1020_dev_pm_ops,
		.of_match_table = uio_fpc1020_of_match,
	},
	.probe = uio_fpc1020_probe,
};
module_spi_driver(uio_fpc1020);

MODULE_AUTHOR("Yassine Oudjana <y.oudjana@protonmail.com");
MODULE_DESCRIPTION("Userspace I/O driver for FPC1020 fingerprint sensors");
MODULE_LICENSE("GPL v2");
