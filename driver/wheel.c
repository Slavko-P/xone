// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2021 Severin von Wnuck <severinvonw@outlook.de>
 */

#include <linux/module.h>
#include <linux/input.h>

#include "common.h"

#define GIP_WL_NAME "Microsoft X-Box One wheel"

enum gip_wheel_button {
	GIP_WL_BTN_MENU = BIT(2),
	GIP_WL_BTN_VIEW = BIT(3),
	GIP_WL_BTN_A = BIT(4),
	GIP_WL_BTN_B = BIT(5),
	GIP_WL_BTN_X = BIT(6),
	GIP_WL_BTN_Y = BIT(7),
	GIP_WL_BTN_DPAD_U = BIT(8),
	GIP_WL_BTN_DPAD_D = BIT(9),
	GIP_WL_BTN_DPAD_L = BIT(10),
	GIP_WL_BTN_DPAD_R = BIT(11),
	GIP_WL_BTN_BUMPER_L = BIT(12),
	GIP_WL_BTN_BUMPER_R = BIT(13),
};

struct gip_wheel_pkt_input {
	__le16 buttons;
	__le16 steering;
	__le16 accelerator;
	__le16 brake;
	__le16 clutch;
} __packed;

struct gip_wheel {
	struct gip_client *client;
	struct gip_led led;
	struct gip_input input;
};

static int gip_wheel_init_input(struct gip_wheel *wheel)
{
	struct input_dev *dev = wheel->input.dev;
	int err;

	input_set_capability(dev, EV_KEY, BTN_MODE);
	input_set_capability(dev, EV_KEY, BTN_START);
	input_set_capability(dev, EV_KEY, BTN_SELECT);
	input_set_capability(dev, EV_KEY, BTN_A);
	input_set_capability(dev, EV_KEY, BTN_B);
	input_set_capability(dev, EV_KEY, BTN_X);
	input_set_capability(dev, EV_KEY, BTN_Y);
	input_set_capability(dev, EV_KEY, BTN_TL);
	input_set_capability(dev, EV_KEY, BTN_TR);
	input_set_abs_params(dev, ABS_X, 0, 65535, 16, 128);
	input_set_abs_params(dev, ABS_Y, 0, 1023, 0, 0);
	input_set_abs_params(dev, ABS_Z, 0, 1023, 0, 0);
	input_set_abs_params(dev, ABS_RZ, 0, 1023, 0, 0);
	input_set_abs_params(dev, ABS_HAT0X, -1, 1, 0, 0);
	input_set_abs_params(dev, ABS_HAT0Y, -1, 1, 0, 0);

	err = input_register_device(dev);
	if (err)
		dev_err(&wheel->client->dev, "%s: register failed: %d\n",
			__func__, err);

	return err;
}

static int gip_wheel_op_guide_button(struct gip_client *client, bool down)
{
	struct gip_wheel *wheel = dev_get_drvdata(&client->dev);

	input_report_key(wheel->input.dev, BTN_MODE, down);
	input_sync(wheel->input.dev);

	return 0;
}

static int gip_wheel_op_input(struct gip_client *client, void *data, int len)
{
	struct gip_wheel *wheel = dev_get_drvdata(&client->dev);
	struct gip_wheel_pkt_input *pkt = data;
	struct input_dev *dev = wheel->input.dev;
	u16 buttons = le16_to_cpu(pkt->buttons);

	if (len < sizeof(*pkt))
		return -EINVAL;

	input_report_key(dev, BTN_START, buttons & GIP_WL_BTN_MENU);
	input_report_key(dev, BTN_SELECT, buttons & GIP_WL_BTN_VIEW);
	input_report_key(dev, BTN_A, buttons & GIP_WL_BTN_A);
	input_report_key(dev, BTN_B, buttons & GIP_WL_BTN_B);
	input_report_key(dev, BTN_X, buttons & GIP_WL_BTN_X);
	input_report_key(dev, BTN_Y, buttons & GIP_WL_BTN_Y);
	input_report_key(dev, BTN_TL, buttons & GIP_WL_BTN_BUMPER_L);
	input_report_key(dev, BTN_TR, buttons & GIP_WL_BTN_BUMPER_R);
	input_report_abs(dev, ABS_X, le16_to_cpu(pkt->steering));
	input_report_abs(dev, ABS_Y, le16_to_cpu(pkt->accelerator));
	input_report_abs(dev, ABS_Z, le16_to_cpu(pkt->brake));
	input_report_abs(dev, ABS_RZ, le16_to_cpu(pkt->clutch));
	input_report_abs(dev, ABS_HAT0X, !!(buttons & GIP_WL_BTN_DPAD_R) -
					 !!(buttons & GIP_WL_BTN_DPAD_L));
	input_report_abs(dev, ABS_HAT0Y, !!(buttons & GIP_WL_BTN_DPAD_D) -
					 !!(buttons & GIP_WL_BTN_DPAD_U));
	input_sync(dev);

	return 0;
}

static int gip_wheel_probe(struct gip_client *client)
{
	struct gip_wheel *wheel;
	int err;

	wheel = devm_kzalloc(&client->dev, sizeof(*wheel), GFP_KERNEL);
	if (!wheel)
		return -ENOMEM;

	wheel->client = client;

	err = gip_init_input(&wheel->input, client, GIP_WL_NAME);
	if (err)
		return err;

	err = gip_wheel_init_input(wheel);
	if (err)
		return err;

	err = gip_set_power_mode(client, GIP_PWR_ON);
	if (err)
		return err;

	err = gip_init_led(&wheel->led, client);
	if (err)
		return err;

	err = gip_complete_authentication(client);
	if (err)
		return err;

	dev_set_drvdata(&client->dev, wheel);

	return 0;
}

static void gip_wheel_remove(struct gip_client *client)
{
	dev_set_drvdata(&client->dev, NULL);
}

static struct gip_driver gip_wheel_driver = {
	.name = "xone-gip-wheel",
	.class = "Windows.Xbox.Input.Wheel",
	.ops = {
		.guide_button = gip_wheel_op_guide_button,
		.input = gip_wheel_op_input,
	},
	.probe = gip_wheel_probe,
	.remove = gip_wheel_remove,
};
module_gip_driver(gip_wheel_driver);

MODULE_ALIAS("gip:Windows.Xbox.Input.Wheel");
MODULE_ALIAS("gip:Microsoft.Xbox.Input.Wheel");
MODULE_AUTHOR("Severin von Wnuck <severinvonw@outlook.de>");
MODULE_DESCRIPTION("xone GIP wheel driver");
MODULE_VERSION("#VERSION#");
MODULE_LICENSE("GPL");
