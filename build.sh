#!/bin/sh
# Copyright (c) 2026 Cody Gu <gujiaqi@iscas.ac.cn>
#
# SPDX-License-Identifier: Apache-2.0

set -eu

APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$APP/.." && pwd)
BUILD=${BUILD_DIR:-"$APP/build"}

if [ -d "$ROOT/modules/hal/hpm_sdk" ]; then
  export HPM_SDK_BASE="$ROOT/modules/hal/hpm_sdk"
elif [ -d "$ROOT/hpm_sdk" ]; then
  export HPM_SDK_BASE="$ROOT/hpm_sdk"
fi

if [ -d "$ROOT/modules/hpm-zephyr" ]; then
  export ZEPHYR_EXTRA_MODULES="$ROOT/modules/hpm-zephyr"
elif [ -d "$ROOT/hpm-zephyr" ]; then
  export ZEPHYR_EXTRA_MODULES="$ROOT/hpm-zephyr"
fi

west build -p always -b hpm5300evk/hpm5361 -d "$BUILD" "$APP" -- \
	-DDTC_OVERLAY_FILE=boards/hpm5300evk_miraczv.overlay
