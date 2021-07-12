// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm Snapdragon Sensor Core (SSC) driver
 *
 * Copyright (c) 2021, Yassine Oudjana <y.oudjana@protonmail.com>
 */

#define DEBUG

#include <linux/firmware.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <linux/soc/qcom/qmi.h>
#include <net/sock.h>

#include "qcom_ssc.h"

struct qcom_ssc {
	struct device *dev;
	struct iio_dev *indio_dev;

	/* Sensor registry */
	const struct firmware *sns_reg;
	struct qmi_handle sns_reg_hdl;

	/* Sensor manager */
	struct qmi_handle sns_smgr_hdl;
	struct sockaddr_qrtr sns_smgr_info;
	struct work_struct sns_smgr_work;

	struct sns_smgr_sensor *sensors;
	u8 sensor_count;

	s32 vals[3];
};

static enum sns_smgr_sensor_type qcom_ssc_sensor_type_from_str(const char *str)
{
	enum sns_smgr_sensor_type i;

	for (i = SNS_SMGR_SENSOR_TYPE_UNKNOWN + 1;
	     i < SNS_SMGR_SENSOR_TYPE_COUNT; i++)
		     if (!strcmp(str, sns_smgr_sensor_type_names[i]))
		     	return i;

	return SNS_SMGR_SENSOR_TYPE_UNKNOWN;
}

/* Sensor registry */
static void qcom_ssc_sns_reg_req_cb(struct qmi_handle *hdl,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn, const void *data)
{
	struct qcom_ssc *ssc = container_of(hdl, struct qcom_ssc, sns_reg_hdl);
	struct qcom_ssc_sns_reg_req *req = (struct qcom_ssc_sns_reg_req *)data;
	struct qcom_ssc_sns_reg_resp resp = { };
	const struct qcom_ssc_sns_reg_entry *entry = NULL;
	size_t i;
	ssize_t ret;

	dev_vdbg(ssc->dev,
		"Received registry request from 0x%x:0x%x for key 0x%04x\n",
		sq->sq_node, sq->sq_port, req->key);

	resp.key = req->key;

	for (i = 0; i < ARRAY_SIZE(qcom_ssc_sns_reg_map); i++) {
		if (req->key == qcom_ssc_sns_reg_map[i].key) {
			entry = &qcom_ssc_sns_reg_map[i];
			break;
		}
	}

	if (!entry) {
		dev_err(ssc->dev, "Unmapped key 0x%04x requested\n", req->key);
		resp.result = QMI_RESULT_FAILURE_V01;
	} else {
		resp.data.len = entry->size;
		memcpy(resp.data.buf, ssc->sns_reg->data + entry->addr,
			resp.data.len);
	}

	ret = qmi_send_response(hdl, sq, txn, SNS_REG_MSG_ID,
				SNS_REG_DATA_RESP_MAX_LEN,
				qcom_ssc_sns_reg_resp_ei, &resp);
	if (ret)
		dev_err(ssc->dev, "Failed to respond to key 0x%04x request: %ld\n",
			req->key, ret);
}

static const struct qmi_msg_handler qcom_ssc_sns_reg_msg_handlers[] = {
	{
		.type = QMI_REQUEST,
		.msg_id = SNS_REG_MSG_ID,
		.ei = qcom_ssc_sns_reg_req_ei,
		.decoded_size = sizeof(struct qcom_ssc_sns_reg_req),
		.fn = qcom_ssc_sns_reg_req_cb,
	},
	{ }
};

/* TODO: Find out what this request is used for */
// static int qcom_ssc_unknown(struct qcom_ssc *ssc)
// {
// 	struct sns_smgr_unknown_resp resp = { };
// 	struct qmi_txn txn;
// 	int ret;

// 	ret = qmi_txn_init(&ssc->sns_smgr_hdl, &txn,
// 				ssc_unknown_resp_ei, &resp);
// 	if (ret < 0) {
// 		dev_err(ssc->dev, "Failed to initialize QMI TXN: %d\n", ret);
// 		return ret;
// 	}

// 	ret = qmi_send_request(&ssc->sns_smgr_hdl, &ssc->sns_smgr_info, &txn,
// 				SNS_SMGR_UNKNOWN_MSG_ID, 0, NULL, NULL);
// 	if (ret < 0) {
// 		dev_err(ssc->dev, "QMI send req fail %d\n", ret);
// 		qmi_txn_cancel(&txn);
// 		return ret;
// 	}

// 	ret = qmi_txn_wait(&txn, 5 * HZ);
// 	if (ret < 0) {
// 		dev_err(ssc->dev, "QMI TXN wait fail: %d\n", ret);
// 		return ret;
// 	}

// 	/* Check the response */
// 	if (resp.result) {
// 		dev_err(ssc->dev, "QMI request failed 0x%x\n",
// 			resp.result);
// 		return -EREMOTEIO;
// 	}

// 	dev_info(ssc->dev, "ret %d R1 %d R2 %d\n", resp.result, resp.unknown1,
// 		resp.unknown2);

// 	return 0;
// }

/* Sensor manager */
static int qcom_ssc_get_avail_sensors(struct qcom_ssc *ssc,
				      struct sns_smgr_sensor **sensors,
				      u8 *count)
{
	struct sns_smgr_avail_sensors_resp resp = { };
	struct qmi_txn txn;
	struct sns_smgr_avail_sensor *avail_sensor;
	struct sns_smgr_sensor *sensor;
	u8 i;
	int ret;

	dev_dbg(ssc->dev, "Getting available sensors\n");

	ret = qmi_txn_init(&ssc->sns_smgr_hdl, &txn,
				sns_smgr_avail_sensors_resp_ei, &resp);
	if (ret < 0) {
		dev_err(ssc->dev, "Failed to initialize QMI TXN: %d\n", ret);
		return ret;
	}

	ret = qmi_send_request(&ssc->sns_smgr_hdl, &ssc->sns_smgr_info, &txn,
				SNS_SMGR_AVAIL_SENSORS_MSG_ID,
				0, NULL, NULL);
	if (ret) {
		dev_err(ssc->dev,
			"Failed to send available sensors request: %d\n", ret);
		qmi_txn_cancel(&txn);
		return ret;
	}

	ret = qmi_txn_wait(&txn, 5 * HZ);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to wait for available sensors transmission %d\n",
			ret);
		return ret;
	}

	/* Check the response */
	if (resp.result) {
		dev_err(ssc->dev, "Available sensors request failed: 0x%x\n",
			resp.result);
		return -EREMOTEIO;
	}

	*count = resp.avail_sensors.count;
	*sensors = devm_kmalloc(ssc->dev, *count * sizeof(struct sns_smgr_sensor),
				GFP_KERNEL);
	if (!sensors)
		return -ENOMEM;

	for (i = 0; i < *count; i++) {
		avail_sensor = &resp.avail_sensors.info[i];
		sensor = &(*sensors)[i];

		sensor->id = avail_sensor->id;
		sensor->type = qcom_ssc_sensor_type_from_str(
						avail_sensor->type);
	}

	return 0;
}

static int qcom_ssc_get_sensor_info(struct qcom_ssc *ssc,
				    struct sns_smgr_sensor *sensor)
{
	struct sns_smgr_sensor_info_req req = { .id = sensor->id, };
	struct sns_smgr_sensor_info_resp resp = { .info = {}, };
	struct qmi_txn txn;
	struct qmi_elem_info *sensor_info_prop_ei;
	u8 prop_idx, i;
	int ret;

	dev_vdbg(ssc->dev, "Getting sensor data for ID 0x%02x\n", sensor->id);

	/* Set sensor data property lengths according to sensor type */
	for (prop_idx = 0; prop_idx < 4; prop_idx++) {
		sensor_info_prop_ei = NULL;
		for (i = 0; i < ARRAY_SIZE(sns_smgr_sensor_info_resp_ei); i++)
			if (sns_smgr_sensor_info_resp_ei[i].tlv_type ==
			    sns_smgr_sensor_info_prop_tlv_types[prop_idx] &&
			    sns_smgr_sensor_info_resp_ei[i].data_type !=
			    					QMI_OPT_FLAG)
				sensor_info_prop_ei =
					&sns_smgr_sensor_info_resp_ei[i];

		if (WARN_ON(!sensor_info_prop_ei))
			return -EINVAL;

		sensor_info_prop_ei->elem_len =
			sensor_info_prop_lengths[sensor->type][prop_idx];
	}

	ret = qmi_txn_init(&ssc->sns_smgr_hdl, &txn,
				sns_smgr_sensor_info_resp_ei, &resp);
	if (ret < 0) {
		dev_err(ssc->dev, "Failed to initialize QMI TXN: %d\n", ret);
		return ret;
	}

	ret = qmi_send_request(&ssc->sns_smgr_hdl, &ssc->sns_smgr_info, &txn,
				SNS_SMGR_SENSOR_INFO_MSG_ID,
				SNS_SMGR_SENSOR_INFO_REQ_MAX_LEN,
				sns_smgr_sensor_info_req_ei, &req);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to send sensor data request: %d\n", ret);
		qmi_txn_cancel(&txn);
		return ret;
	}

	ret = qmi_txn_wait(&txn, 5 * HZ);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to wait for sensor data transmission %d\n", ret);
		return ret;
	}

	/* Check the response */
	if (resp.result) {
		dev_err(ssc->dev, "Sensor data request failed: 0x%x\n",
			resp.result);
		return -EREMOTEIO;
	}

	sensor->channel_count = resp.info.channel_count;
	/* TODO: Maybe find a better way to move the strings over */
	for (i = 0; i < sensor->channel_count; i++) {
		sensor->channels[i].name = devm_kmemdup(ssc->dev,
						resp.info.channels[i].name,
						resp.info.channels[i].name_len + 1,
						GFP_KERNEL);
		if (!sensor->channels[i].name)
			return -ENOMEM;

		sensor->channels[i].vendor = devm_kmemdup(ssc->dev,
						resp.info.channels[i].vendor,
						resp.info.channels[i].vendor_len + 1,
						GFP_KERNEL);
		if (!sensor->channels[i].vendor)
			return -ENOMEM;
	}

	return 0;
}

static int qcom_ssc_enable(struct qcom_ssc *ssc)
{
	/* TODO: Find what these values mean and remove the un-hardcode them */
	struct sns_smgr_enable_req req = {
		.unknown1 = 1,
		.unknown2 = 1,
		.unknown3 = 0x0a0000,
		.unknown4 = {0x1, 0x0, 0x0, 0x3, 0x0, 0xf, 0x0, 0x1, 0x0},
		.unknown5 = {0x0, 0x0, 0x0, 0x0, 0x0}
	};
	struct sns_smgr_enable_resp resp = { };
	struct qmi_txn txn;
	int ret;

	dev_vdbg(ssc->dev, "Enabling sensor data\n");

	ret = qmi_txn_init(&ssc->sns_smgr_hdl, &txn,
				sns_smgr_enable_resp_ei, &resp);
	if (ret < 0) {
		dev_err(ssc->dev, "Failed to initialize QMI TXN: %d\n", ret);
		return ret;
	}

	ret = qmi_send_request(&ssc->sns_smgr_hdl, &ssc->sns_smgr_info, &txn,
				SNS_SMGR_ENABLE_MSG_ID,
				SNS_SMGR_ENABLE_REQ_MAX_LEN,
				sns_smgr_enable_req_ei, &req);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to send enable request: %d\n", ret);
		qmi_txn_cancel(&txn);
		return ret;
	}

	ret = qmi_txn_wait(&txn, 5 * HZ);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to wait for enable transmission %d\n", ret);
		return ret;
	}

	/* Check the response */
	if (resp.result) {
		dev_err(ssc->dev, "Enable request failed: 0x%x\n",
			resp.result);
		return -EREMOTEIO;
	}

	dev_dbg(ssc->dev, "Enable unkown1 %d unknown2 %d\n", resp.unknown1,
		resp.unknown2);

	return 0;
}

static void qcom_ssc_sns_smgr_worker(struct work_struct *work) {
	struct qcom_ssc *ssc = container_of(work, struct qcom_ssc, sns_smgr_work);
	u8 i, j;
	int ret;

	ret = qcom_ssc_get_avail_sensors(ssc, &ssc->sensors, &ssc->sensor_count);
	if (ret) {
		dev_err(ssc->dev, "Failed to get available sensors: %d\n", ret);
		return;
	}

	for (i = 0; i < ssc->sensor_count; i++) {
		dev_dbg(ssc->dev, "Found %s sensor with ID 0x%02x\n",
			sns_smgr_sensor_type_names[ssc->sensors[i].type],
			ssc->sensors[i].id);
	}

	for (i = 0; i < ssc->sensor_count; i++) {
		ret = qcom_ssc_get_sensor_info(ssc, &ssc->sensors[i]);
		if (ret)
			dev_err(ssc->dev,
				"Failed to get sensor 0x%02x data: %d\n",
				ssc->sensors[i].id, ret);

		for (j = 0; j < ssc->sensors[i].channel_count; j++)
			dev_dbg(ssc->dev, "0x%02x,%d: %s %s\n",
				ssc->sensors[i].id, j,
				ssc->sensors[i].channels[j].vendor,
				ssc->sensors[i].channels[j].name);
	}

	ret = qcom_ssc_enable(ssc);
	if (ret)
		dev_err(ssc->dev, "Failed to enable sensors: %d\n", ret);
}

static int qcom_ssc_sns_smgr_new_server(struct qmi_handle *hdl,
				   struct qmi_service *service)
{
	struct qcom_ssc *ssc = container_of(hdl, struct qcom_ssc, sns_smgr_hdl);

	dev_dbg(ssc->dev, "SSC added a sensor manager server\n");

	ssc->sns_smgr_info.sq_family = AF_QIPCRTR;
	ssc->sns_smgr_info.sq_node = service->node;
	ssc->sns_smgr_info.sq_port = service->port;

	schedule_work(&ssc->sns_smgr_work);

	return 0;
}

static void qcom_ssc_sns_smgr_del_server(struct qmi_handle *hdl,
				   struct qmi_service *service)
{
	struct qcom_ssc *ssc = container_of(hdl, struct qcom_ssc, sns_smgr_hdl);

	dev_dbg(ssc->dev, "SSC deleted the sensor manager server\n");

	ssc->sns_smgr_info.sq_node = 0;
	ssc->sns_smgr_info.sq_port = 0;
}

static const struct qmi_ops qcom_ssc_sns_smgr_ops = {
	.new_server = qcom_ssc_sns_smgr_new_server,
	.del_server = qcom_ssc_sns_smgr_del_server,
};

/*
 * Hacky proof-of-concept accelerometer integration into IIO
 *
 * TODO: Clean up this mess
 */
static void qcom_ssc_sns_smgr_sensor_data_cb(struct qmi_handle *hdl,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn, const void *data)
{
	struct qcom_ssc *ssc = container_of(hdl, struct qcom_ssc, sns_smgr_hdl);
	struct sns_smgr_sensor_data_ind *ind =
		(struct sns_smgr_sensor_data_ind *)data;

	ssc->vals[0] = ind->data.x;
	ssc->vals[1] = ind->data.y;
	ssc->vals[2] = ind->data.z;
}

static const struct qmi_msg_handler qcom_ssc_sns_smgr_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = SNS_SMGR_SENSOR_DATA_MSG_ID,
		.ei = sns_smgr_sensor_data_ind_ei,
		.decoded_size = sizeof(struct sns_smgr_sensor_data_ind),
		.fn = qcom_ssc_sns_smgr_sensor_data_cb,
	},
	{ }
};

static int qcom_ssc_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	struct qcom_ssc *ssc = iio_priv(indio_dev);

	*val2 = 0;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = ssc->vals[chan->address];
		dev_vdbg(ssc->dev, "Read channel %ld raw: %d\n",
			chan->address, *val);
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 9355; // 9.81 * 0x100000, then converted to nano
		dev_vdbg(ssc->dev, "Read channel %ld scale: %d\n",
			chan->address, *val);
		return IIO_VAL_INT_PLUS_NANO;
	default:
		dev_err(ssc->dev, "Unhandled IIO mask %ld\n", mask);
	}

	return -EINVAL;
}

static const struct iio_info qcom_ssc_iio_info = {
	.read_raw = qcom_ssc_read_raw,
};

/* TODO: Get mount matrix from SSC or read it from the device tree */
static const struct iio_mount_matrix qcom_ssc_mount_matrix = {
	.rotation = {
		"0", "-1", "0",
		"-1", "0", "0",
		"0", "0", "1"
	}
};

static const struct iio_mount_matrix *
qcom_ssc_get_mount_matrix(const struct iio_dev *indio_dev,
			  const struct iio_chan_spec *chan)
{
	return &qcom_ssc_mount_matrix;
}

static const struct iio_chan_spec_ext_info qcom_ssc_ext_info[] = {
	IIO_MOUNT_MATRIX(IIO_SHARED_BY_DIR, qcom_ssc_get_mount_matrix),
	{ }
};

static const struct iio_chan_spec qcom_ssc_iio_channels[] = {
	{
		.type = IIO_ACCEL,
		.address = 0,
		.modified = true,
		.channel2 = IIO_MOD_X,
		.scan_type = {
			.sign = 's',
			.realbits = 24,
			.storagebits = 32,
		},
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_SCALE),
		.ext_info = qcom_ssc_ext_info
	},
	{
		.type = IIO_ACCEL,
		.address = 1,
		.modified = true,
		.channel2 = IIO_MOD_Y,
		.scan_type = {
			.sign = 's',
			.realbits = 24,
			.storagebits = 32,
		},
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_SCALE),
		.ext_info = qcom_ssc_ext_info
	},
	{
		.type = IIO_ACCEL,
		.address = 2,
		.modified = true,
		.channel2 = IIO_MOD_Z,
		.scan_type = {
			.sign = 's',
			.realbits = 24,
			.storagebits = 32,
		},
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_SCALE),
		.ext_info = qcom_ssc_ext_info
	},
};
/* End of IIO hacks */

static int qcom_ssc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct iio_dev *indio_dev;
	struct qcom_ssc *ssc;
	const char *fw_name = "sns.reg";
	int ret;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*ssc));
	if (!indio_dev)
		return -ENOMEM;

	ssc = iio_priv(indio_dev);
	ssc->dev = dev;
	ssc->indio_dev = indio_dev;

	indio_dev->name = "qcom-ssc";
	indio_dev->info = &qcom_ssc_iio_info;
	indio_dev->channels = qcom_ssc_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(qcom_ssc_iio_channels);
	indio_dev->modes = INDIO_DIRECT_MODE;

	INIT_WORK(&ssc->sns_smgr_work, qcom_ssc_sns_smgr_worker);

	platform_set_drvdata(pdev, ssc);

	/* Use custom firmware name if available */
	ret = of_property_read_string_index(ssc->dev->of_node, "firmware-name", 0,
					    &fw_name);

	/* Load sensor registry */
	dev_dbg(ssc->dev, "Loading sensor registry from %s\n", fw_name);
	ret = request_firmware(&ssc->sns_reg, fw_name, ssc->dev);
	if (ret) {
		dev_err(ssc->dev, "Failed to load sensor registry: %d\n", ret);
		return ret;
	}

	/* Initialize sensor registry server */
	ret = qmi_handle_init(&ssc->sns_reg_hdl, 0,
				NULL, qcom_ssc_sns_reg_msg_handlers);
	if (ret) {
		dev_err(dev,
			"Failed to initialize sensor registry handle: %d\n",
			ret);
		return ret;
	}

	ret = qmi_add_server(&ssc->sns_reg_hdl, SNS_REG_QMI_SVC_ID,
			SNS_REG_QMI_SVC_V1, SNS_REG_QMI_INS_ID);
	if (ret) {
		dev_err(dev, "Failed to add sensor registry server: %d\n", ret);
		return ret;
	}

	/* Initialize sensor manager client */
	ret = qmi_handle_init(&ssc->sns_smgr_hdl,
				SNS_SMGR_SENSOR_INFO_RESP_MAX_LEN,
				&qcom_ssc_sns_smgr_ops,
				qcom_ssc_sns_smgr_msg_handlers);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to initialize sensor manager handle: %d\n",
			ret);
		return ret;
	}

	ret = qmi_add_lookup(&ssc->sns_smgr_hdl, SNS_SMGR_QMI_SVC_ID,
			SNS_SMGR_QMI_SVC_V1, SNS_SMGR_QMI_INS_ID);
	if (ret < 0) {
		dev_err(ssc->dev,
			"Failed to add lookup for sensor manager: %d\n",
			ret);
		qmi_handle_release(&ssc->sns_smgr_hdl);
	}

	ret = devm_iio_device_register(ssc->dev, indio_dev);
	if (ret)
		dev_err(ssc->dev, "Failed to register IIO device: %d\n", ret);

	return ret;
}

static int qcom_ssc_remove(struct platform_device *pdev) {
	struct qcom_ssc *ssc = platform_get_drvdata(pdev);

	qmi_handle_release(&ssc->sns_reg_hdl);
	qmi_handle_release(&ssc->sns_smgr_hdl);

	release_firmware(ssc->sns_reg);

	return 0;
}

static const struct of_device_id qcom_ssc_of_match[] = {
	{ .compatible = "qcom,ssc", },
	{ },
};

MODULE_DEVICE_TABLE(of, qcom_ssc_of_match);

static struct platform_driver qcom_ssc_driver = {
	.probe = qcom_ssc_probe,
	.remove = qcom_ssc_remove,
	.driver	= {
		.name = "qcom_ssc",
		.of_match_table = qcom_ssc_of_match,
	},
};

module_platform_driver(qcom_ssc_driver);

MODULE_AUTHOR("Yassine Oudjana <y.oudjana@protonmail.com>");
MODULE_DESCRIPTION("Qualcomm SSC driver");
MODULE_LICENSE("GPL");
