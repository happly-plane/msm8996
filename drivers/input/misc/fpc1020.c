// SPDX-License-Identifier: GPL-2.0
/*
 * Fingerprint Cards FPC1020 Fingerprint Sensor Driver
 *
 * Copyright (c) 2021, Yassine Oudjana <y.oudjana@protonmail.com>
 */

#define DEBUG

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>

#define FPC102X_TIRQVALID			1300
#define FPC102X_SPI_CLK_FREQ			8000000U

#define FPC102X_ROWS				56
#define FPC102X_COLUMNS				192
#define FPC102X_IMG_DATA_LEN			10752

#define FPC102X_CMD_WAIT_FOR_FINGER_PRESENT	0x24
#define FPC102X_CMD_CAPTURE_IMG			0xc0
#define FPC102X_CMD_READ_IMG_DATA		0xc4

#define FPC102X_REG_INTR			0x18
#define FPC102X_REG_INTR_CLEAR			0x1c
#define FPC102X_REG_FNGRDRIVECONF		0x8c
#define FPC102X_REG_HWID			0xfc

#define FPC102X_REG_INTR_SIZE			1
#define FPC102X_REG_HWID_SIZE			2

#define FPC102X_INTR_BIT_FINGER_DOWN		BIT(0)
#define FPC102X_INTR_BIT_ERROR			BIT(2)
#define FPC102X_INTR_BIT_IMG_DATA_AVAIL		BIT(5)
#define FPC102X_INTR_BIT_CMD_DONE		BIT(7)

static const char * const fpc1020_supply_name[] = {
	"vcc",
	"vdda",
	"vddio",
};

#define FPC1020_NUM_SUPPLIES ARRAY_SIZE(fpc1020_supply_name)

struct fpc1020_chip {
	struct device *dev;
	struct spi_device *spi;

	struct regulator_bulk_data supplies[FPC1020_NUM_SUPPLIES];
	struct gpio_desc *reset;
	int irq;

	struct completion ready;
	struct completion cmd_done;
	struct completion img_avail;
	struct work_struct scan_work;
};

static int fpc1020_send_cmd(struct fpc1020_chip *chip, u8 addr)
{
	int error;

	error = spi_write(chip->spi, &addr, 1);
	if (error)
		return error;
/*
	wait_for_completion(&chip->cmd_done);
	reinit_completion(&chip->cmd_done);
*/
	return 0;
}

static int fpc1020_access_reg(struct fpc1020_chip *chip, u8 addr, size_t len,
				void *val, bool write)
{
	struct spi_message msg;
	struct spi_transfer *reg;
	struct spi_transfer *data;

	reg = devm_kzalloc(chip->dev, sizeof(*reg), GFP_KERNEL);
	if (!reg)
		return -ENOMEM;

	data = devm_kzalloc(chip->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	reg->speed_hz = FPC102X_SPI_CLK_FREQ;
	reg->len = 1;
	reg->tx_buf = &addr;

	data->speed_hz = FPC102X_SPI_CLK_FREQ;
	data->len = len;
	data->bits_per_word = 8 * (!(len % 2) && len <= 4 ? len : 1);
	if (write)
		data->tx_buf = val;
	else
		data->rx_buf = val;

	spi_message_init(&msg);
	spi_message_add_tail(reg, &msg);
	spi_message_add_tail(data, &msg);

	return spi_sync(chip->spi, &msg);
}

static void fpc1020_scan_worker(struct work_struct *work)
{
	struct fpc1020_chip *chip = container_of(work, struct fpc1020_chip,
						scan_work);
	struct spi_message msg;
	struct spi_transfer *reg;
	struct spi_transfer *dummy;
	struct spi_transfer *data;
	u8 cmd, temp;
	u8 *img_data; //FPC102X_IMG_DATA_LEN
	char pixels[FPC102X_ROWS]; //FPC102X_COLUMNS
	int i, j, error, cnt = 0;
	u8 *data_ptr;

	reg = devm_kzalloc(chip->dev, sizeof(*reg), GFP_KERNEL);
	if (!reg)
		return;

	dummy = devm_kzalloc(chip->dev, sizeof(*dummy), GFP_KERNEL);
	if (!dummy)
		return;

	data = devm_kzalloc(chip->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return;

	img_data = devm_kmalloc(chip->dev, sizeof(u8) * FPC102X_IMG_DATA_LEN,
			GFP_KERNEL);
	if (!img_data)
		return;

	/* Capture image */
	error = fpc1020_send_cmd(chip, FPC102X_CMD_CAPTURE_IMG);
	if (error) {
		dev_err(chip->dev, "Failed to send capture command: %d\n", error);
		return;
	}

	wait_for_completion(&chip->img_avail);
	reinit_completion(&chip->img_avail);

	/* Read captured image data */
	spi_message_init(&msg);

	cmd = FPC102X_CMD_READ_IMG_DATA;
	reg->speed_hz = FPC102X_SPI_CLK_FREQ;
	reg->len = 1;
	reg->tx_buf = &cmd;
	spi_message_add_tail(reg, &msg);

	dummy->speed_hz = FPC102X_SPI_CLK_FREQ;
	dummy->len = 1;
	dummy->rx_buf = &temp;
	spi_message_add_tail(dummy, &msg);

	data->speed_hz = FPC102X_SPI_CLK_FREQ;
	data->len = FPC102X_IMG_DATA_LEN;
	data->rx_buf = img_data;
	spi_message_add_tail(data, &msg);

	error = spi_sync(chip->spi, &msg);
	if (error) {
		dev_err(chip->dev, "Failed to read image data: %d\n", error);
		return;
	}

	dev_info(chip->dev, "first pixel %d\n", *img_data);

	for (i = 0; i < FPC102X_COLUMNS; i++) {
		for (j = 0; j < FPC102X_ROWS; j++) {
			data_ptr = img_data + cnt;
			pixels[j] = *data_ptr;
			cnt++;
		}
		dev_info(chip->dev, "%s\n", pixels);
	}
}

static irqreturn_t fpc1020_irq_handler(int irq, void *ptr)
{
	struct fpc1020_chip *chip = ptr;
	u8 intr_type;
	int error;

	error = fpc1020_access_reg(chip, FPC102X_REG_INTR_CLEAR,
				FPC102X_REG_INTR_SIZE, &intr_type, false);
	if (error) {
		dev_err(chip->dev, "Failed to read interrupt type: %d\n", error);
		return IRQ_NONE;
	}

	dev_dbg(chip->dev, "Interrupt type 0x%02x", intr_type);

	if(intr_type == 0xff) {
		/* Chip came out of reset */
		complete_all(&chip->ready);
		return IRQ_HANDLED;
	}

	if (intr_type & FPC102X_INTR_BIT_FINGER_DOWN) {
		dev_dbg(chip->dev, "Finger detected\n");
		schedule_work(&chip->scan_work);
	}
	if (intr_type & FPC102X_INTR_BIT_ERROR)
		dev_dbg(chip->dev, "Received error interrupt\n");
		/* TODO: Handle error */
	if (intr_type & FPC102X_INTR_BIT_IMG_DATA_AVAIL) {
		dev_dbg(chip->dev, "Image data available\n");
		complete_all(&chip->img_avail);
	}
	if (intr_type & FPC102X_INTR_BIT_CMD_DONE) {
		dev_dbg(chip->dev, "Command done\n");
		complete_all(&chip->cmd_done);
	}

	return IRQ_HANDLED;
}

static int fpc1020_probe(struct spi_device *spi)
{
        struct fpc1020_chip *chip;
	u16 hw_id;
	u8 temp;
	int i, irq, error = 0;

        chip = devm_kzalloc(&spi->dev, sizeof(*chip), GFP_KERNEL);
        if (!chip)
                return -ENOMEM;

	chip->dev = &spi->dev;
	chip->spi = spi;
	spi_set_drvdata(spi, chip);

	chip->reset = devm_gpiod_get(chip->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(chip->reset)) {
		error = PTR_ERR(chip->reset);
		dev_err(chip->dev, "Failed to get reset pin: %d\n", error);
		return error;
	}

	for (i = 0; i < FPC1020_NUM_SUPPLIES; i++)
		chip->supplies[i].supply = fpc1020_supply_name[i];

	error = devm_regulator_bulk_get(chip->dev, FPC1020_NUM_SUPPLIES,
					chip->supplies);
	if (error) {
		dev_err(chip->dev, "Failed to get regulators: %d\n", error);
		return error;
	}

	init_completion(&chip->ready);
	init_completion(&chip->cmd_done);
	init_completion(&chip->img_avail);
	INIT_WORK(&chip->scan_work, fpc1020_scan_worker);

	irq = of_irq_get(chip->dev->of_node, 0);
	if (irq < 0) {
		dev_err(chip->dev, "Failed to get IRQ: %d\n", irq);
		return irq;
	}
	error = devm_request_threaded_irq(chip->dev, irq,
					NULL, fpc1020_irq_handler,
					IRQF_ONESHOT,
					"fpc1020", chip);
	if (error) {
		dev_err(chip->dev,
			"Failed to request IRQ: %d", error);
		return error;
	}

	/* Power-on chip */
	error = regulator_bulk_enable(FPC1020_NUM_SUPPLIES, chip->supplies);
	if (error) {
		dev_err(chip->dev, "Failed to enable regulators: %d\n", error);
		return error;
	}

	/* Reset chip */
	gpiod_set_value_cansleep(chip->reset, 1);
	usleep_range(1000, 1010);
	gpiod_set_value_cansleep(chip->reset, 0);

	/* Wait for chip to become ready */
	error = wait_for_completion_timeout(&chip->ready,
				msecs_to_jiffies(FPC102X_TIRQVALID));
	if (error <= 0) {
		error = -ETIMEDOUT;
		dev_err(chip->dev, "Failed to power-on chip: %d\n", error);
		return error;
	}

	/* Read hardware ID */
	error = fpc1020_access_reg(chip, FPC102X_REG_HWID, FPC102X_REG_HWID_SIZE,
				&hw_id, false);
	if (error) {
		dev_err(chip->dev, "Failed to read HW ID: %d\n", error);
		return error;
	}
	dev_info(chip->dev, "Chip type: FPC1%x%X\n", hw_id >> 4, hw_id & 0xf);

	/* Wait for finger */
	error = fpc1020_send_cmd(chip, FPC102X_CMD_WAIT_FOR_FINGER_PRESENT);
	if (error) {
		dev_err(chip->dev,
			"Failed to begin querying finger presence: %d\n", error);
		return error;
	}

        return 0;
}

static const struct of_device_id fpc1020_of_match[] = {
	{ .compatible = "fpc,fpc1020" },
	{},
};
MODULE_DEVICE_TABLE(spi, fpc1020_of_match);

static struct spi_driver fpc1020_driver = {
	.driver = {
		.name = "fpc1020",
		//.pm = &CHIP_pm_ops,
		.of_match_table = fpc1020_of_match,
        },
	.probe = fpc1020_probe,
	// .remove = CHIP_remove,
};
module_spi_driver(fpc1020_driver);

MODULE_AUTHOR("Yassine Oudjana <y.oudjana@protonmail.com");
MODULE_DESCRIPTION("Fingerprint Cards FPC1020 Fingerprint Sensor Driver");
MODULE_LICENSE("GPL v2");