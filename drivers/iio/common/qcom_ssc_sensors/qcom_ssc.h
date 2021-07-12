/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/soc/qcom/qmi.h>

/* Sensor registry service */
#define SNS_REG_QMI_SVC_ID			0x010f
#define SNS_REG_QMI_SVC_V1			2
#define SNS_REG_QMI_INS_ID			0
#define SNS_REG_MSG_ID				0x4
#define SNS_REG_DATA_MAX_LEN			0x100
#define SNS_REG_DATA_RESP_MAX_LEN		0x10f

/* Sensor Manager service */
#define SNS_SMGR_QMI_SVC_ID			0x0100
#define SNS_SMGR_QMI_SVC_V1			1
#define SNS_SMGR_QMI_INS_ID			50

// TODO: Figure out what this request is used for
//#define SNS_SMGR_UNKNOWN_MSG_ID			0x1
#define SNS_SMGR_AVAIL_SENSORS_MSG_ID		0x5
#define SNS_SMGR_SENSOR_INFO_MSG_ID		0x6
#define SNS_SMGR_ENABLE_MSG_ID			0x21
#define SNS_SMGR_SENSOR_DATA_MSG_ID		0x22

// #define SNS_SMGR_UNKNOWN_RESP_MAX_LEN		0x11
#define SNS_SMGR_AVAIL_SENSORS_REQ_MAX_LEN	0x0
#define SNS_SMGR_AVAIL_SENSORS_RESP_MAX_LEN	0x3e // might be more
#define SNS_SMGR_SENSOR_INFO_REQ_MAX_LEN	0x4
#define SNS_SMGR_SENSOR_INFO_RESP_MAX_LEN	0x110 // 0x101, + 0xa for good measure. might be more
#define SNS_SMGR_ENABLE_REQ_MAX_LEN		0x30 // probably less
#define SNS_SMGR_ENABLE_RESP_MAX_LEN		0x1e // probably less

#define SNS_SMGR_AVAIL_SENSORS_MAX_NUM		0xf // might be more
#define SNS_SMGR_SENSOR_TYPE_MAX_LEN		0xf // might be more
#define SNS_SMGR_SENSOR_INFO_NAME_MAX_LEN	0xff
#define SNS_SMGR_SENSOR_CHANNELS_MAX_NUM	0x2 // might be more but unlikely

#define SNS_SMGR_SENSOR_INFO_UNKNOWN1_MAX_LEN	9
#define SNS_SMGR_SENSOR_INFO_UNKNOWN2_MAX_LEN	17
#define SNS_SMGR_SENSOR_INFO_UNKNOWN3_MAX_LEN	22
#define SNS_SMGR_SENSOR_INFO_UNKNOWN4_MAX_LEN	9

enum sns_smgr_sensor_type {
	SNS_SMGR_SENSOR_TYPE_UNKNOWN,
	SNS_SMGR_SENSOR_TYPE_ACCEL,
	SNS_SMGR_SENSOR_TYPE_GYRO,
	SNS_SMGR_SENSOR_TYPE_MAG,
	SNS_SMGR_SENSOR_TYPE_PROX_LIGHT,
	SNS_SMGR_SENSOR_TYPE_PRESSURE,
	SNS_SMGR_SENSOR_TYPE_HALL_EFFECT,

	SNS_SMGR_SENSOR_TYPE_COUNT
};

struct sns_smgr_sensor_channel {
	const char *name;
	const char *vendor;
};

struct sns_smgr_sensor {
	u8 id;
	enum sns_smgr_sensor_type type;
	u8 channel_count;
	struct sns_smgr_sensor_channel channels[SNS_SMGR_SENSOR_CHANNELS_MAX_NUM];
};

/*
 * Requests and Responses
 */
/* Sensor registry */
struct qcom_ssc_sns_reg_req {
	u16 key;
};

static struct qmi_elem_info qcom_ssc_sns_reg_req_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_2_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u16),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x01,
		.offset     = offsetof(struct qcom_ssc_sns_reg_req, key),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct qcom_ssc_sns_reg_data {
	size_t len;
	u8 buf[SNS_REG_DATA_MAX_LEN];
};

static struct qmi_elem_info qcom_ssc_sns_reg_data_ei[] = {
	{
		.data_type  = QMI_DATA_LEN,
		.elem_len   = 1,
		.elem_size  = sizeof(size_t),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct qcom_ssc_sns_reg_data, len),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = SNS_REG_DATA_MAX_LEN,
		.elem_size  = sizeof(u8),
		.array_type = VAR_LEN_ARRAY,
		.offset     = offsetof(struct qcom_ssc_sns_reg_data, buf),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct qcom_ssc_sns_reg_resp {
	u16 result;
	u16 key;
	struct qcom_ssc_sns_reg_data data;
};

static struct qmi_elem_info qcom_ssc_sns_reg_resp_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_2_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u16),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x02,
		.offset     = offsetof(struct qcom_ssc_sns_reg_resp, result),
	},
	{
		.data_type  = QMI_UNSIGNED_2_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u16),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x03,
		.offset     = offsetof(struct qcom_ssc_sns_reg_resp, key),
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = 1,
		.elem_size  = sizeof(struct qcom_ssc_sns_reg_data),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x04,
		.offset     = offsetof(struct qcom_ssc_sns_reg_resp, data),
		.ei_array   = qcom_ssc_sns_reg_data_ei,
	},
	{
		.data_type = QMI_EOTI,
	},
};

/* Sensor manager */
// struct sns_smgr_unknown_resp {
// 	u16 result;
// 	u32 unknown1;
// 	u16 unknown2;
// };

// static struct qmi_elem_info ssc_unknown_resp_ei[] = {
// 	{
// 		.data_type  = QMI_UNSIGNED_2_BYTE,
// 		.elem_len   = 1,
// 		.elem_size  = sizeof(u16),
// 		.array_type = NO_ARRAY,
// 		.tlv_type   = 0x02,
// 		.offset     = offsetof(struct sns_smgr_unknown_resp, result),
// 	},
// 	{
// 		.data_type  = QMI_UNSIGNED_4_BYTE,
// 		.elem_len   = 1,
// 		.elem_size  = sizeof(u32),
// 		.array_type = NO_ARRAY,
// 		.tlv_type   = 0x03,
// 		.offset     = offsetof(struct sns_smgr_unknown_resp, unknown1),
// 	},
// 	{
// 		.data_type  = QMI_UNSIGNED_2_BYTE,
// 		.elem_len   = 1,
// 		.elem_size  = sizeof(u16),
// 		.array_type = NO_ARRAY,
// 		.tlv_type   = 0x04,
// 		.offset     = offsetof(struct sns_smgr_unknown_resp, unknown2),
// 	},
// 	{
// 		.data_type = QMI_EOTI,
// 	},
// };

struct sns_smgr_avail_sensor {
	u8 id;
	u8 type_len;
	char type[SNS_SMGR_SENSOR_TYPE_MAX_LEN];
};

static struct qmi_elem_info sns_smgr_avail_sensor_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_avail_sensor, id),
	},
	{
		.data_type  = QMI_DATA_LEN,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_avail_sensor, type_len),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = SNS_SMGR_AVAIL_SENSORS_MAX_NUM,
		.elem_size  = sizeof(char),
		.array_type = VAR_LEN_ARRAY,
		.offset     = offsetof(struct sns_smgr_avail_sensor, type),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_avail_sensors {
	u8 count;
	struct sns_smgr_avail_sensor info[SNS_SMGR_AVAIL_SENSORS_MAX_NUM];
};

static struct qmi_elem_info sns_smgr_avail_sensors_ei[] = {
	{
		.data_type  = QMI_DATA_LEN,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_avail_sensors, count),
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = SNS_SMGR_AVAIL_SENSORS_MAX_NUM,
		.elem_size  = sizeof(struct sns_smgr_avail_sensor),
		.array_type = VAR_LEN_ARRAY,
		.offset     = offsetof(struct sns_smgr_avail_sensors, info),
		.ei_array = sns_smgr_avail_sensor_ei,
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_avail_sensors_resp {
	u16 result;
	struct sns_smgr_avail_sensors avail_sensors;
};

static struct qmi_elem_info sns_smgr_avail_sensors_resp_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_2_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u16),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x02,
		.offset     = offsetof(struct sns_smgr_avail_sensors_resp,
					result),
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = 1,
		.elem_size  = sizeof(struct sns_smgr_avail_sensors),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x03,
		.offset     = offsetof(struct sns_smgr_avail_sensors_resp,
					avail_sensors),
		.ei_array = sns_smgr_avail_sensors_ei,
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_sensor_info_req {
	u8 id;
};

static struct qmi_elem_info sns_smgr_sensor_info_req_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x01,
		.offset     = offsetof(struct sns_smgr_sensor_info_req, id),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sensor_channel_info {
	u8 sensor_id;
	u8 channel_id;
	u8 name_len;
	char name[SNS_SMGR_SENSOR_INFO_NAME_MAX_LEN];
	u8 vendor_len;
	char vendor[SNS_SMGR_SENSOR_INFO_NAME_MAX_LEN];
	u8 unknown1[18];
};

static struct qmi_elem_info ssc_sensor_channel_info_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, sensor_id),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, channel_id),
	},
	{
		.data_type  = QMI_DATA_LEN,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, name_len),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = SNS_SMGR_SENSOR_INFO_NAME_MAX_LEN,
		.elem_size  = sizeof(u8),
		.array_type = VAR_LEN_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, name),
	},
	{
		.data_type  = QMI_DATA_LEN,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, vendor_len),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = SNS_SMGR_SENSOR_INFO_NAME_MAX_LEN,
		.elem_size  = sizeof(u8),
		.array_type = VAR_LEN_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, vendor),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 18,
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.offset     = offsetof(struct sensor_channel_info, unknown1),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sensor_info {
	u8 channel_count;
	struct sensor_channel_info channels[SNS_SMGR_SENSOR_CHANNELS_MAX_NUM];
};

static struct qmi_elem_info ssc_sensor_info_ei[] = {
	{
		.data_type  = QMI_DATA_LEN,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sensor_info, channel_count),
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = SNS_SMGR_SENSOR_CHANNELS_MAX_NUM,
		.elem_size  = sizeof(struct sensor_channel_info),
		.array_type = VAR_LEN_ARRAY,
		.offset     = offsetof(struct sensor_info, channels),
		.ei_array = ssc_sensor_channel_info_ei,
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_sensor_info_resp {
	u16 result;
	struct sensor_info info;
	u8 unknown1[SNS_SMGR_SENSOR_INFO_UNKNOWN1_MAX_LEN];
	u32 bus;
	u8 unknown2[SNS_SMGR_SENSOR_INFO_UNKNOWN2_MAX_LEN];
	u8 unknown3_valid;
	u8 unknown3[SNS_SMGR_SENSOR_INFO_UNKNOWN3_MAX_LEN];
	u8 unknown4[SNS_SMGR_SENSOR_INFO_UNKNOWN4_MAX_LEN];
};

static struct qmi_elem_info sns_smgr_sensor_info_resp_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_2_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u16),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x02,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, result),
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = 0xff,
		.elem_size  = sizeof(struct sensor_info),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x03,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, info),
		.ei_array = ssc_sensor_info_ei,
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		/* elem_len is dynamically set according to sensor type */
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.tlv_type   = 0x10,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, unknown1),
	},
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x11,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp,
				       bus),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		/* elem_len is dynamically set according to sensor type */
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.tlv_type   = 0x12,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, unknown2),
	},
	{
		.data_type  = QMI_OPT_FLAG,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x13,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, unknown3_valid),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		/* elem_len is dynamically set according to sensor type */
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.tlv_type   = 0x13,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, unknown3),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		/* elem_len is dynamically set according to sensor type */
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.tlv_type   = 0x14,
		.offset     = offsetof(struct sns_smgr_sensor_info_resp, unknown4),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_enable_req {
	u8 unknown1;
	u8 unknown2;
	u32 unknown3;
	u8 unknown4[9];
	u8 unknown5[5];
};

static struct qmi_elem_info sns_smgr_enable_req_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x01,
		.offset     = offsetof(struct sns_smgr_enable_req, unknown1),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x02,
		.offset     = offsetof(struct sns_smgr_enable_req, unknown2),
	},
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x03,
		.offset     = offsetof(struct sns_smgr_enable_req, unknown3),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 9,
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.tlv_type   = 0x04,
		.offset     = offsetof(struct sns_smgr_enable_req, unknown4),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 5,
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.tlv_type   = 0x10,
		.offset     = offsetof(struct sns_smgr_enable_req, unknown5),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_enable_resp {
	u16 result;
	u8 unknown1;
	u8 unknown2;
};

static struct qmi_elem_info sns_smgr_enable_resp_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_2_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u16),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x02,
		.offset     = offsetof(struct sns_smgr_enable_resp, result),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x10,
		.offset     = offsetof(struct sns_smgr_enable_resp, unknown1),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x11,
		.offset     = offsetof(struct sns_smgr_enable_resp, unknown2),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_sensor_data_metadata {
	u32 unknown1;
	u32 timestamp;
	u8 unknown2[5];
};

static struct qmi_elem_info sns_smgr_sensor_data_metadata_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_metadata, unknown1),
	},
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_metadata, timestamp),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 5,
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_metadata, unknown2),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_sensor_data_data {
	u8 unknown1;
	s32 x;
	s32 y;
	s32 z;
	u8 unknown2[4];
};

static struct qmi_elem_info sns_smgr_sensor_data_data_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_data, unknown1),
	},
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_data, x),
	},
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_data, y),
	},
	{
		.data_type  = QMI_UNSIGNED_4_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u32),
		.array_type = NO_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_data, z),
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 4,
		.elem_size  = sizeof(u8),
		.array_type = STATIC_ARRAY,
		.offset     = offsetof(struct sns_smgr_sensor_data_data, unknown2),
	},
	{
		.data_type = QMI_EOTI,
	},
};

struct sns_smgr_sensor_data_ind {
	u8 unknown1;
	struct sns_smgr_sensor_data_metadata metadata;
	struct sns_smgr_sensor_data_data data;
	u8 unknown2;
};

static struct qmi_elem_info sns_smgr_sensor_data_ind_ei[] = {
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x01,
		.offset     = offsetof(struct sns_smgr_sensor_data_ind, unknown1),
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = 1,
		.elem_size  = sizeof(struct sns_smgr_sensor_data_metadata),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x02,
		.offset     = offsetof(struct sns_smgr_sensor_data_ind, metadata),
		.ei_array = sns_smgr_sensor_data_metadata_ei,
	},
	{
		.data_type  = QMI_STRUCT,
		.elem_len   = 1,
		.elem_size  = sizeof(struct sns_smgr_sensor_data_data),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x03,
		.offset     = offsetof(struct sns_smgr_sensor_data_ind, data),
		.ei_array   = sns_smgr_sensor_data_data_ei,
	},
	{
		.data_type  = QMI_UNSIGNED_1_BYTE,
		.elem_len   = 1,
		.elem_size  = sizeof(u8),
		.array_type = NO_ARRAY,
		.tlv_type   = 0x10,
		.offset     = offsetof(struct sns_smgr_sensor_data_ind, unknown2),
	},
	{
		.data_type = QMI_EOTI,
	},
};

/*
 * Sensor registry map
 */
struct qcom_ssc_sns_reg_entry {
	u16 key;
	u16 addr;
	size_t size;
};

static const struct qcom_ssc_sns_reg_entry qcom_ssc_sns_reg_map[] = {
	{ 0x000, 0x0000, 0x018 },
	{ 0x00a, 0x0800, 0x018 },
	{ 0x3e8, 0x0a00, 0x003 },
	{ 0x3f2, 0x0c00, 0x003 },
	{ 0x3fc, 0x0d00, 0x003 },
	{ 0x410, 0x0100, 0x080 },
	{ 0x7d0, 0x0200, 0x010 },
	{ 0x7d2, 0x0400, 0x018 },
	{ 0x802, 0x1100, 0x00c },
	{ 0xa3c, 0x0e00, 0x024 },
	{ 0xa46, 0x0f00, 0x018 },
	{ 0xa50, 0x1000, 0x00a },
	{ 0xa6e, 0x1500, 0x010 },
	{ 0xa82, 0x1700, 0x100 },
	{ 0xa84, 0x1800, 0x100 },
	{ 0xa85, 0x1900, 0x100 },
	{ 0xa86, 0x1a00, 0x100 },
	{ 0xa87, 0x1b00, 0x100 },
	{ 0xa88, 0x1c00, 0x100 },
	{ 0xa8a, 0x2700, 0x100 },
	{ 0xa8b, 0x2d00, 0x100 },
	{ 0xa8c, 0x1d00, 0x0e0 },
	{ 0xaf0, 0x1f00, 0x022 },
	{ 0xb54, 0x2000, 0x004 },
	{ 0xb5e, 0x2100, 0x004 },
	{ 0xb68, 0x2200, 0x004 },
	{ 0xb72, 0x2300, 0x004 },
	{ 0xb7c, 0x2400, 0x024 },
	{ 0xb86, 0x2500, 0x008 },
	{ 0xb90, 0x2800, 0x004 },
	{ 0xbb8, 0x2e00, 0x100 },
	{ 0xbc2, 0x3100, 0x100 },
	{ 0xbcc, 0x3500, 0x100 },
	{ 0xbe0, 0x3a00, 0x014 },
	{ 0xbfe, 0x3c00, 0x00c },
	{ 0xc08, 0x3f00, 0x05a },
	{ 0xc12, 0x6000, 0x014 },
	{ 0xce4, 0x4200, 0x00e },
	{ 0xce5, 0x4300, 0x00e },
	{ 0xce6, 0x4400, 0x00e },
	{ 0xce7, 0x4500, 0x00e },
	{ 0xce8, 0x4600, 0x00e },
	{ 0xce9, 0x4700, 0x00e },
	{ 0xcea, 0x4800, 0x00e },
	{ 0xceb, 0x4900, 0x00e },
	{ 0xcec, 0x4a00, 0x00e },
	{ 0xced, 0x4b00, 0x00e },
	{ 0xcee, 0x4c00, 0x00e },
	{ 0xcef, 0x4d00, 0x00e },
	{ 0xcf0, 0x4e00, 0x00e },
	{ 0xcf1, 0x4f00, 0x00e },
	{ 0xcf2, 0x5000, 0x00e },
	{ 0xcf3, 0x5100, 0x00e },
	{ 0xcf4, 0x5200, 0x00e },
	{ 0xcf5, 0x5300, 0x00e },
	{ 0xcf6, 0x5400, 0x00e },
	{ 0xcf7, 0x5500, 0x00e },
	{ 0xcf8, 0x5600, 0x00e },
	{ 0xcf9, 0x5700, 0x00e },
	{ 0xcfa, 0x5800, 0x00e },
	{ 0xcfb, 0x5900, 0x00e },
	{ 0xcfc, 0x5a00, 0x00e },
	{ 0xcfd, 0x5b00, 0x00e },
	{ 0xcfe, 0x5c00, 0x00e },
	{ 0xcff, 0x5d00, 0x00e },
	{ 0xd00, 0x5e00, 0x00e },
	{ 0xd01, 0x5f00, 0x00e },
	{ 0xd48, 0x6100, 0x01c }
};

/*
 * Sensor manager constants
 */
static const char *sns_smgr_sensor_type_names[SNS_SMGR_SENSOR_TYPE_COUNT] = {
	[SNS_SMGR_SENSOR_TYPE_ACCEL]		= "ACCEL",
	[SNS_SMGR_SENSOR_TYPE_GYRO]		= "GYRO",
	[SNS_SMGR_SENSOR_TYPE_MAG]		= "MAG",
	[SNS_SMGR_SENSOR_TYPE_PROX_LIGHT]	= "PROX_LIGHT",
	[SNS_SMGR_SENSOR_TYPE_PRESSURE]		= "PRESSURE",
	[SNS_SMGR_SENSOR_TYPE_HALL_EFFECT]	= "HALL_EFFECT"
};

static const u8 sns_smgr_sensor_info_prop_tlv_types[4] = {
	0x10, 0x12, 0x13, 0x14
};

static const u8 sensor_info_prop_lengths[SNS_SMGR_SENSOR_TYPE_COUNT][4] = {
	[SNS_SMGR_SENSOR_TYPE_ACCEL]		= {0x05, 0x09, 0x16, 0x05},
	[SNS_SMGR_SENSOR_TYPE_GYRO]		= {0x05, 0x09, 0x16, 0x05},
	[SNS_SMGR_SENSOR_TYPE_MAG]		= {0x05, 0x09, 0x0a, 0x05},
	[SNS_SMGR_SENSOR_TYPE_PROX_LIGHT]	= {0x09, 0x11, 0x07, 0x09},
	[SNS_SMGR_SENSOR_TYPE_PRESSURE]		= {0x09, 0x11, 0x00, 0x09},
	[SNS_SMGR_SENSOR_TYPE_HALL_EFFECT]	= {0x05, 0x09, 0x04, 0x05}
};
