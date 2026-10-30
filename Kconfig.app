# Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
#
# SPDX-License-Identifier: Apache-2.0

menu "MiracZV application"

config MIRACZV
	bool
	default y

config MIRACZV_MOTOR_TB6612
	bool "TB6612 motor controller"
	default y
	depends on PWM && GPIO

config MIRACZV_ENCODER
	bool "Generic encoder with QEIV2 backend"
	default y
	depends on QEIV2_HPMICRO
	depends on MIRACZV_MOTOR_TB6612

config MIRACZV_HID_HOST
	bool "USB HID host controller input"
	default y
	depends on USB_HOST_STACK && UHC_DRIVER

config MIRACZV_HID_LOG_LEVEL
	int "HID host log level"
	default 3
	range 0 4
	help
	  0 = off, 1 = error, 2 = warning, 3 = info, 4 = debug.

config MIRACZV_CONTROL_PERIOD_MS
	int "Closed-loop control period in milliseconds"
	default 10
	range 1 100

config MIRACZV_WHEEL_RADIUS_MM
	int "Wheel radius in millimeters"
	default 32
	range 1 1000

config MIRACZV_TRACK_WIDTH_MM
	int "Wheel track width in millimeters"
	default 150
	range 1 5000

config MIRACZV_WHEELBASE_MM
	int "Vehicle wheelbase in millimeters"
	default 300
	range 1 10000
	help
	  Reserved for chassis models that need front-to-rear axle spacing.
	  The current differential drive uses MIRACZV_TRACK_WIDTH_MM instead.

config MIRACZV_MAX_WHEEL_MPS
	int "Absolute maximum wheel speed in millimeters per second"
	default 2000
	range 1 100000

config MIRACZV_MAX_LINEAR_MPS
	int "Maximum remote-control linear speed in millimeters per second"
	default 500
	range 1 100000

config MIRACZV_MAX_ANGULAR_MRADPS
	int "Maximum remote-control yaw rate in milliradians per second"
	default 6667
	range 1 100000

config MIRACZV_COMMAND_TIMEOUT_MS
	int "Controller command timeout in milliseconds"
	default 1000
	range 100 10000

config MIRACZV_HID_AXIS_DEADZONE_PERMILLE
	int "HID gamepad axis dead zone in per mille"
	default 60
	range 0 500

choice MIRACZV_HID_RIGHT_STICK_AXES
	prompt "Preferred HID right-stick axis encoding"
	default MIRACZV_HID_RIGHT_STICK_Z_RZ
	help
	  Select the Generic Desktop axes normally used by the right stick.
	  MiracZV falls back to the other pair when the preference is absent.

config MIRACZV_HID_RIGHT_STICK_Z_RZ
	bool "Z and RZ axes"

config MIRACZV_HID_RIGHT_STICK_RX_RY
	bool "RX and RY axes"

endchoice

config MIRACZV_HID_INVERT_THROTTLE
	bool "Invert the right-stick throttle axis"
	default y
	help
	  Most HID gamepads report the vertical axis with its minimum at the
	  top. Enable this so pushing the right stick forward drives forward.

config MIRACZV_HID_INVERT_STEERING
	bool "Invert the right-stick steering axis"
	default y
	help
	  Standard HID gamepads report left as the axis minimum. Inverting this
	  axis maps left stick movement to positive yaw, which turns left.

config MIRACZV_HID_REPORT_TIMEOUT_MS
	int "HID interrupt transfer timeout in milliseconds"
	default 250
	range 20 5000

config MIRACZV_HID_RETRY_PERIOD_MS
	int "HID host retry period in milliseconds"
	default 1000
	range 100 10000

config MIRACZV_PWM_PERIOD_US
	int "Expected motor PWM period in microseconds"
	default 20
	range 1 20000
	help
	  Must match the period encoded in the chassis pwms devicetree property.
	  The PWM hardware uses the devicetree period directly.

endmenu
