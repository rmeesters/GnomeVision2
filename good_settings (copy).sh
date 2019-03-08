#!/bin/bash
v4l2-ctl -d /dev/video$1 \
--set-ctrl brightness=128 \
--set-ctrl contrast=128 \
--set-ctrl saturation=128 \
--set-ctrl white_balance_temperature_auto=1 \
--set-ctrl white_balance_temperature=4000 \
--set-ctrl power_line_frequency=2 \
--set-ctrl sharpness=128 \
--set-ctrl backlight_compensation=0 \
--set-ctrl exposure_auto=3 \
--set-ctrl exposure_absolute=250
