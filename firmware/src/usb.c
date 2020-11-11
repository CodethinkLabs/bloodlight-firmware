/*
 * Copyright 2020 Codethink Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <stdint.h>

#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#if (BL_REVISION >= 2)
#include <libopencm3/stm32/crs.h>
#endif

#include "common/error.h"
#include "common/util.h"

#include "msg.h"
#include "mq.h"
#include "usb.h"

struct {
	usbd_device *handle;
} usb_g;

#define BL_USB_BUF_LEN 64

enum usb_bl_strings {
	BL_USB_STRING_MANUFACTURER,
	BL_USB_STRING_PRODUCT,
	BL_USB_STRING_SERIAL_NUMBER,
};

static const char *bl_usb_strings[] = {
	[BL_USB_STRING_MANUFACTURER]  = BL_STR_MANUFACTURER,
	[BL_USB_STRING_PRODUCT]       = BL_STR_PRODUCT,
	[BL_USB_STRING_SERIAL_NUMBER] = BL_STR_SERIAL_NUM,
};

static const struct usb_device_descriptor device_descriptor = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor  = 0x0483, /* TODO: Codethink vendor ID? */
	.idProduct = 0x5740, /* TODO: Codethink product ID process? */
	.bcdDevice = 0x0200,
	.iManufacturer = 1 + BL_USB_STRING_MANUFACTURER,
	.iProduct      = 1 + BL_USB_STRING_PRODUCT,
	.iSerialNumber = 1 + BL_USB_STRING_SERIAL_NUMBER,
	.bNumConfigurations = 1,
};

static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 },
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors),
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config_descriptor = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes bl_usb__cdcacm_control_request(
		usbd_device *usbd_dev,
		struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
		usbd_control_complete_callback *complete)
{
	BL_UNUSED(buf);
	BL_UNUSED(complete);
	BL_UNUSED(usbd_dev);

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		usbd_ep_write_packet(usbd_dev, 0x83, buf, 10);
		return USBD_REQ_HANDLED;
		}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding))
			return USBD_REQ_NOTSUPP;
		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NOTSUPP;
}

static bool usb_response_used;
static union bl_msg_data usb_response;

static void bl_usb__cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	static uint8_t buf[BL_USB_BUF_LEN];
	union bl_msg_data *msg;
	enum bl_error error;
	uint16_t len;

	len = usbd_ep_read_packet(usbd_dev, ep, buf, sizeof(buf));
	if (len == 0) {
		return;
	}

	msg = bl_msg_decode(buf, len);
	if (msg == NULL) {
		usb_response.response.type        = BL_MSG_RESPONSE;
		usb_response.response.response_to = bl_msg_get_type(buf);
		usb_response.response.error_code  = BL_ERROR_BAD_MESSAGE_LENGTH;
		usb_response_used = true;
		return;
	}

	usb_response_used = bl_msg_handle(msg, &usb_response);
}

static void bl_usb__cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	BL_UNUSED(wValue);

	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK,
			BL_USB_BUF_LEN, bl_usb__cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK,
			BL_USB_BUF_LEN, NULL);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
			usbd_dev,
			USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
			USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
			bl_usb__cdcacm_control_request);
}

static void bl_usb__setup(void)
{
#if (BL_REVISION >= 2)
	rcc_osc_on(RCC_HSI48);
	rcc_wait_for_osc_ready(RCC_HSI48);
	rcc_set_clock48_source(RCC_CCIPR_CLK48_HSI48);
#endif

	/* Enable clocks for GPIO port A and USB peripheral. */
	rcc_periph_clock_enable(RCC_USB);
	rcc_periph_clock_enable(RCC_GPIOA);

#if (BL_REVISION >= 2)
	/* Enable clock recovery system for USB clock. */
	crs_autotrim_usb_enable();
#endif

#if (BL_REVISION == 1)
	rcc_periph_clock_enable(RCC_GPIOB);

	/* Enable pull-up control. */
	gpio_mode_setup(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO4);
	gpio_set(GPIOB, GPIO4);
#endif

	/* Setup GPIO pins for USB D+/D-. */
#if (BL_REVISION == 1)
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF14, GPIO11 | GPIO12);
#else
	gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG,
			GPIO_PUPD_NONE, GPIO11 | GPIO12);
#endif

}

/* Exported function, documented in usb.h */
void bl_usb_init(void)
{
	bl_usb__setup();

	usb_g.handle = usbd_init(
#if (BL_REVISION == 1)
			&st_usbfs_v1_usb_driver,
#else
			&st_usbfs_v2_usb_driver,
#endif
			&device_descriptor, &config_descriptor,
			bl_usb_strings, BL_ARRAY_LEN(bl_usb_strings),
			usbd_control_buffer, sizeof(usbd_control_buffer));

	usbd_register_set_config_callback(usb_g.handle,
			bl_usb__cdcacm_set_config);
}

/**
 * Send a message to the host.
 *
 * \param[in]  msg  Message to send to host.
 * \return True on success.
 */
static bool bl_usb__send_message(union bl_msg_data *msg)
{
	uint16_t len = bl_msg_len(msg);
	return (len == usbd_ep_write_packet(usb_g.handle, 0x82, msg, len));
}

/* Exported function, documented in usb.h */
void bl_usb_poll(void)
{
	if (mq_pending != 0x00) {
		unsigned channel = bl_mq_pending_channel();

		union bl_msg_data *msg = bl_mq_peek(channel);
		if (msg && bl_usb__send_message(msg)) {
			bl_mq_release(channel);
		}
	} else if (usb_response_used) {
		if (bl_usb__send_message((union bl_msg_data *)&usb_response)) {
			usb_response_used = false;
		}
	}

	usbd_poll(usb_g.handle);
}
