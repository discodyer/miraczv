/*
 * Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <math.h>
#include <miraczv/hid_report.h>
#include <miraczv/platform/hid_controller.h>
#include <string.h>
#include <usbh_ch9.h>
#include <usbh_class.h>
#include <usbh_class_api.h>
#include <usbh_desc.h>
#include <usbh_device.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/usb/usbh.h>

#define MIRACZV_HID_REPORT_BYTES 64U
#define HID_REPORT_DESC_MAX      1024U
#define MIRACZV_HID_BUFFER_BYTES 512U
#define MIRACZV_HID_BUFFER_COUNT 2U
#define HID_AXIS_X               0x30U
#define HID_AXIS_Y               0x31U

LOG_MODULE_REGISTER(miraczv_hid, CONFIG_MIRACZV_HID_LOG_LEVEL);

USBH_CONTROLLER_DEFINE(miraczv_host, DEVICE_DT_GET(DT_NODELABEL(usb0)));
NET_BUF_POOL_FIXED_DEFINE(miraczv_hid_pool, MIRACZV_HID_BUFFER_COUNT, MIRACZV_HID_BUFFER_BYTES, 0,
			  NULL);

struct hid_input_state {
	struct usb_device *udev;
	uint8_t ep;
	uint16_t ep_mps;
	uint8_t iface;
	uint16_t report_desc_len;
	struct uhc_transfer *xfer;
	struct net_buf *buf;
	struct k_sem report_sem;
	struct k_mutex lock;
	atomic_t connected;
	atomic_t report_pending;
	uint8_t report[MIRACZV_HID_REPORT_BYTES];
	size_t report_len;
	int report_status;
	uint32_t sequence;
	int64_t last_report_ms;
	struct miraczv_hid_report report_map;
};

static struct hid_input_state hid;
static bool host_initialized;
static bool host_enabled;
static int64_t last_retry_ms;
static int last_error;

static struct net_buf *hid_buffer_alloc(size_t length)
{
	struct net_buf *buf;

	if (length > MIRACZV_HID_BUFFER_BYTES) {
		return NULL;
	}

	buf = net_buf_alloc(&miraczv_hid_pool, K_NO_WAIT);
	if (buf != NULL) {
		net_buf_reset(buf);
	}
	return buf;
}

static bool hid_report_has_axis(uint32_t usage)
{
	for (uint8_t i = 0U; i < hid.report_map.axis_count; ++i) {
		if (hid.report_map.usage[i] == usage) {
			return true;
		}
	}

	return false;
}

static void hid_release_xfer(void)
{
	if (hid.buf != NULL) {
		net_buf_unref(hid.buf);
		hid.buf = NULL;
	}
	if (hid.xfer != NULL) {
		(void)uhc_xfer_unref(hid.xfer);
		hid.xfer = NULL;
	}
	atomic_clear(&hid.report_pending);
}

static void hid_release_completed_xfer(struct uhc_transfer *xfer)
{
	if (hid.buf != NULL) {
		net_buf_unref(hid.buf);
		hid.buf = NULL;
	}
	hid.xfer = NULL;
	atomic_clear(&hid.report_pending);
	(void)uhc_xfer_unref(xfer);
}

static int hid_completion(struct usb_device *udev, struct uhc_transfer *xfer)
{
	ARG_UNUSED(udev);
	bool current;

	k_mutex_lock(&hid.lock, K_FOREVER);
	current = xfer == hid.xfer;

	if (!current || !atomic_get(&hid.connected)) {
		if (current) {
			hid_release_completed_xfer(xfer);
		}
		atomic_clear(&hid.report_pending);
		k_mutex_unlock(&hid.lock);
		return 0;
	}

	hid.report_status = xfer->err;
	hid.report_len = xfer->buf != NULL ? MIN(xfer->buf->len, sizeof(hid.report)) : 0U;
	if (hid.report_len > 0U) {
		memcpy(hid.report, xfer->buf->data, hid.report_len);
	}
	hid_release_completed_xfer(xfer);
	atomic_set(&hid.report_pending, 0);
	k_mutex_unlock(&hid.lock);
	k_sem_give(&hid.report_sem);
	return 0;
}

static void hid_cancel_pending(void)
{
	struct uhc_transfer *xfer = hid.xfer;

	if ((xfer == NULL) || (hid.udev == NULL)) {
		return;
	}

	if (usbh_xfer_dequeue(hid.udev, xfer) != 0) {
		LOG_DBG("HID transfer dequeue returned an error");
	}
}

static int hid_probe(struct usbh_class_data *const c_data, struct usb_device *const udev,
		     const uint8_t iface)
{
	const struct usb_if_descriptor *desc = usbh_desc_get_iface(udev, iface);
	const struct usb_cfg_descriptor *cfg = udev->cfg_desc;
	const uint8_t *cursor;
	const uint8_t *end;

	ARG_UNUSED(c_data);
	if ((desc == NULL) || (udev->state != USB_STATE_CONFIGURED) ||
	    (desc->bInterfaceClass != USB_BCC_HID)) {
		return -ENOTSUP;
	}

	cursor = (const uint8_t *)desc + desc->bLength;
	end = (const uint8_t *)cfg + sys_le16_to_cpu(cfg->wTotalLength);
	uint8_t interrupt_ep = 0U;
	uint16_t interrupt_mps = 0U;
	uint16_t report_desc_len = 0U;

	while (cursor + sizeof(struct usb_desc_header) <= end) {
		const struct usb_desc_header *header = (const struct usb_desc_header *)cursor;
		const struct usb_ep_descriptor *ep = (const struct usb_ep_descriptor *)cursor;

		if ((header->bLength < sizeof(*header)) || (cursor + header->bLength > end) ||
		    (header->bDescriptorType == USB_DESC_INTERFACE)) {
			break;
		}
		if ((header->bDescriptorType == USB_DESC_HID) && (header->bLength >= 9U)) {
			report_desc_len = sys_get_le16(&cursor[7]);
		}
		if ((header->bDescriptorType == USB_DESC_ENDPOINT) &&
		    (header->bLength >= sizeof(*ep)) && USB_EP_DIR_IS_IN(ep->bEndpointAddress) &&
		    ((ep->bmAttributes & USB_EP_TRANSFER_TYPE_MASK) == USB_EP_TYPE_INTERRUPT)) {
			interrupt_ep = ep->bEndpointAddress;
			interrupt_mps = USB_MPS_EP_SIZE(sys_le16_to_cpu(ep->wMaxPacketSize));
		}
		cursor += header->bLength;
	}
	if (interrupt_ep == 0U || interrupt_mps == 0U || report_desc_len == 0U) {
		return -ENOENT;
	}

	k_mutex_lock(&hid.lock, K_FOREVER);
	hid.udev = udev;
	hid.ep = interrupt_ep;
	hid.ep_mps = interrupt_mps;
	hid.iface = iface;
	hid.report_desc_len = report_desc_len;
	memset(&hid.report_map, 0, sizeof(hid.report_map));
	hid.last_report_ms = k_uptime_get();
	atomic_set(&hid.connected, 1);
	k_mutex_unlock(&hid.lock);
	return 0;
}

static int hid_class_init(struct usbh_class_data *const c_data)
{
	ARG_UNUSED(c_data);
	k_mutex_init(&hid.lock);
	k_sem_init(&hid.report_sem, 0, 1);
	return 0;
}

static int hid_removed(struct usbh_class_data *const c_data)
{
	ARG_UNUSED(c_data);
	k_mutex_lock(&hid.lock, K_FOREVER);
	atomic_clear(&hid.connected);
	hid.udev = NULL;
	hid.ep = 0U;
	hid.ep_mps = 0U;
	hid.iface = 0U;
	hid.report_desc_len = 0U;
	memset(&hid.report_map, 0, sizeof(hid.report_map));
	k_sem_reset(&hid.report_sem);
	k_mutex_unlock(&hid.lock);
	return 0;
}

static struct usbh_class_api hid_api = {
	.init = hid_class_init,
	.probe = hid_probe,
	.removed = hid_removed,
};

USBH_DEFINE_CLASS(miraczv_hid, &hid_api, NULL, NULL);

static int hid_load_report_descriptor(void)
{
	uint16_t length = MIN(hid.report_desc_len, HID_REPORT_DESC_MAX);
	size_t allocation_length = ROUND_UP(length, 32U);
	struct usb_device *udev = hid.udev;
	struct net_buf *buf;
	int ret;

	if ((udev == NULL) || (allocation_length > MIRACZV_HID_BUFFER_BYTES)) {
		return -ENODEV;
	}
	/* The controller may touch the final cache line beyond wLength. */
	buf = hid_buffer_alloc(allocation_length);
	if (buf == NULL) {
		return -ENOMEM;
	}
	ret = usbh_req_setup(udev, 0x81U, USB_SREQ_GET_DESCRIPTOR, USB_DESC_HID_REPORT << 8U,
			     hid.iface, length, buf);
	if (ret == 0) {
		ret = miraczv_hid_report_parse(buf->data, buf->len, &hid.report_map);
	}
	if (ret == 0) {
		LOG_INF("HID report descriptor: %u bytes, %u axes", buf->len,
			hid.report_map.axis_count);
		if (!hid_report_has_axis(HID_AXIS_X) || !hid_report_has_axis(HID_AXIS_Y)) {
			LOG_ERR("HID gamepad does not expose X/Y axes");
			memset(&hid.report_map, 0, sizeof(hid.report_map));
			ret = -ENOTSUP;
		}
	}
	net_buf_unref(buf);
	return ret;
}

static int hid_axis_index(uint32_t usage)
{
	for (uint8_t i = 0U; i < hid.report_map.axis_count; ++i) {
		if (hid.report_map.usage[i] == usage) {
			return i;
		}
	}
	return -ENOENT;
}

static float normalize_axis(uint8_t index, int32_t value)
{
	int64_t minimum = hid.report_map.minimum[index];
	int64_t maximum = hid.report_map.maximum[index];
	float center = (minimum + maximum) * 0.5f;
	float range = maximum - minimum;
	float normalized;

	if (range <= 0.0f) {
		return 0.0f;
	}
	normalized = CLAMP((value - center) / (range * 0.5f), -1.0f, 1.0f);
	return fabsf(normalized) < (CONFIG_MIRACZV_HID_AXIS_DEADZONE_PERMILLE / 1000.0f)
		       ? 0.0f
		       : normalized;
}

static int hid_ensure_host(void)
{
	int ret;

	if (!device_is_ready(miraczv_host.dev)) {
		return -ENODEV;
	}

	if (!host_initialized) {
		ret = usbh_init(&miraczv_host);
		if (ret != 0) {
			return ret;
		}
		host_initialized = true;
	}
	if (!host_enabled) {
		ret = usbh_enable(&miraczv_host);
		if (ret != 0) {
			return ret;
		}
		host_enabled = true;
	}

	return 0;
}

static int hid_poll(struct miraczv_command *command)
{
	int ret;
	int64_t now_ms;

	ret = hid_ensure_host();
	if (ret != 0) {
		now_ms = k_uptime_get();

		if ((ret != last_error) ||
		    ((now_ms - last_retry_ms) >= CONFIG_MIRACZV_HID_RETRY_PERIOD_MS)) {
			LOG_ERR("USB host unavailable: %d", ret);
			last_error = ret;
			last_retry_ms = now_ms;
		}
		return ret;
	}

	if (k_mutex_lock(&hid.lock, K_NO_WAIT) != 0) {
		return -EAGAIN;
	}
	if (!atomic_get(&hid.connected) || (hid.udev == NULL)) {
		if ((hid.xfer != NULL) && !atomic_get(&hid.report_pending)) {
			hid_release_xfer();
		}
		k_mutex_unlock(&hid.lock);
		return -EAGAIN;
	}
	if (!hid.report_map.parsed) {
		ret = hid_load_report_descriptor();
		if (ret != 0) {
			LOG_ERR("Unsupported HID report descriptor: %d", ret);
			last_error = ret;
			k_mutex_unlock(&hid.lock);
			return ret;
		}
	}

	if (hid.xfer == NULL) {
		hid.xfer = usbh_xfer_alloc(hid.udev, hid.ep, hid_completion, NULL);
		if (hid.xfer == NULL) {
			k_mutex_unlock(&hid.lock);
			return -ENOMEM;
		}
		hid.buf = hid_buffer_alloc(hid.ep_mps);
		if (hid.buf == NULL) {
			hid_release_xfer();
			k_mutex_unlock(&hid.lock);
			return -ENOMEM;
		}
		hid.buf->len = 0;
		atomic_set(&hid.report_pending, 1);
		hid.xfer->buf = hid.buf;
		ret = usbh_xfer_enqueue(hid.udev, hid.xfer);
		if (ret != 0) {
			hid_release_xfer();
			k_mutex_unlock(&hid.lock);
			return ret;
		}
	}

	if (k_sem_take(&hid.report_sem, K_NO_WAIT) != 0) {
		now_ms = k_uptime_get();
		if (atomic_get(&hid.report_pending) &&
		    ((now_ms - hid.last_report_ms) > CONFIG_MIRACZV_HID_REPORT_TIMEOUT_MS)) {
			hid_cancel_pending();
			hid.last_report_ms = now_ms;
			k_mutex_unlock(&hid.lock);
			return -EAGAIN;
		}
		k_mutex_unlock(&hid.lock);
		return -EAGAIN;
	}
	now_ms = k_uptime_get();

	if (hid.report_status != 0) {
		ret = -EIO;
	} else {
		int32_t x_raw;
		int32_t y_raw;
		int x_axis = hid_axis_index(HID_AXIS_X);
		int y_axis = hid_axis_index(HID_AXIS_Y);
		float throttle;
		float steer;

		if ((x_axis < 0) || (y_axis < 0) ||
		    miraczv_hid_report_axis(&hid.report_map, x_axis, hid.report, hid.report_len,
					    &x_raw) != 0 ||
		    miraczv_hid_report_axis(&hid.report_map, y_axis, hid.report, hid.report_len,
					    &y_raw) != 0) {
			ret = -EBADMSG;
		} else {
			LOG_DBG("HID input: len=%u x=%d y=%d", hid.report_len, x_raw, y_raw);
			steer = normalize_axis(x_axis, x_raw);
			throttle = normalize_axis(y_axis, y_raw);
			if (IS_ENABLED(CONFIG_MIRACZV_HID_INVERT_X)) {
				steer = -steer;
			}
			if (IS_ENABLED(CONFIG_MIRACZV_HID_INVERT_Y)) {
				throttle = -throttle;
			}
			command->linear_mps = throttle * (CONFIG_MIRACZV_MAX_LINEAR_MPS / 1000.0f);
			command->angular_radps =
				steer * (CONFIG_MIRACZV_MAX_ANGULAR_MRADPS / 1000.0f);
			command->sequence = ++hid.sequence;
			command->timestamp_ms = now_ms;
			command->valid = true;
			ret = 0;
		}
	}

	if (hid.buf != NULL) {
		hid.buf->len = 0;
	}
	hid_release_xfer();
	hid.last_report_ms = now_ms;
	k_mutex_unlock(&hid.lock);

	if (ret == 0) {
		last_error = 0;
	}
	return ret;
}

static int hid_init(void)
{
	last_retry_ms = k_uptime_get();

	return 0;
}

const struct miraczv_controller_ops miraczv_hid_controller_ops = {
	.name = "hid-host",
	.init = hid_init,
	.poll = hid_poll,
};
