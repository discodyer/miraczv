<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# MiracZV

[English](README.md) | [简体中文](README_CN.md)

MiracZV (Miracle + Zephyr + RISC-V) is a compact differential-drive
controller for the HPM5361. It starts from the MiracZ7 application model but
uses Zephyr v4.5.0, the upstream USB Host stack and a smaller set of validated
on-chip peripherals:

- TB6612FNG dual motor PWM and direction control
- Generic encoder abstraction with an HPMicro QEIV2 backend
- USB Host HID input using the report descriptor for X/Y axis discovery
- Controller abstraction for future DDS, MSP or autonomous command sources
- 100 Hz differential-drive kinematics and independent wheel-speed PID loops
- Hardware single- and double-precision floating point
- DDS, BMI088, display and advanced vehicle modules intentionally omitted

## Hardware

Target board: HPM5300EVK RevD with HPM5361ICB1.

| Function | MCU signal | Header |
|---|---|---|
| Left motor PWM | PA26 / PWM0.P2 | J7-11 |
| Right motor PWM | PA27 / PWM0.P3 | J7-12 |
| Left motor IN1 | PA10 / GPIOA10 | P1-12 |
| Left motor IN2 | PA11 / GPIOA11 | P1-16 |
| Right motor IN1 | PB10 / GPIOB10 | P1-36 |
| Right motor IN2 | PB11 / GPIOB11 | P1-40 |
| Motor standby | PA12 / GPIOA12 | P1-11 |
| Left encoder A | PA20 / QEI0.A | J7-1 |
| Left encoder B | PA21 / QEI0.B | J7-3 |
| Right encoder A | PB04 / QEI1.A | P1-31 |
| Right encoder B | PB05 / QEI1.B | P1-32 |
| USB Host | USB0 | J1 USB_OTG |
| Console | UART0 | J8 FT2232 |

TB6612 VM must come from an external motor supply with a common ground. Do not
connect motor power to an EVK header. The EVK header only carries the 3.3 V
logic signals listed above.

## Build

The repository manifest pins Zephyr, HPM SDK and `hpm-zephyr`:

```sh
west init -l miraczv
west update
cd miraczv
./build.sh
```

For the existing workspace layout, the equivalent explicit command is:

```sh
export HPM_SDK_BASE=/workdir/zephyr-workspace/hpm_sdk
export ZEPHYR_EXTRA_MODULES=/workdir/zephyr-workspace/hpm-zephyr

west build -p always -b hpm5300evk/hpm5361 -d build/miraczv \
  miraczv -- \
  -DDTC_OVERLAY_FILE=boards/hpm5300evk_miraczv.overlay
```

The application does not modify Zephyr, `hpm_sdk` or `hpm-zephyr`.

## USB Host input

Connect an OTG-aware adapter to J1 and attach a USB HID gamepad or its wireless
receiver. MiracZV finds the HID report descriptor, locates the Generic Desktop
X and Y usages, and uses the descriptor's Report ID, bit offset, field width,
signedness and logical range. Current mapping:

- X axis: steering
- Y axis: throttle, inverted by default so pushing the stick forward drives forward
- X/Y inversion can be changed with `CONFIG_MIRACZV_HID_INVERT_X` and
  `CONFIG_MIRACZV_HID_INVERT_Y`
- Dead zone: `CONFIG_MIRACZV_HID_AXIS_DEADZONE_PERMILLE`
- Linear limit: `CONFIG_MIRACZV_MAX_LINEAR_MPS`
- Yaw-rate limit: `CONFIG_MIRACZV_MAX_ANGULAR_MRADPS`

The HID class uses a private fixed net_buf pool. The generic UHC buffer pool is
kept for control requests and hub enumeration, while gamepad descriptor and
interrupt reports are bounded by `MIRACZV_HID_BUFFER_BYTES`. This avoids class
traffic growing or fragmenting the global UHC heap during hot-plug. The initial
dynamic UHC buffer path was observed to corrupt the HPM5361 heap after the
descriptor transfer; moving HID class buffers to the fixed pool resolved that
failure on hardware.

On 2026-10-30, MiracZV enumerated the wireless USB HID receiver, fetched and
parsed its 123-byte report descriptor, and received continuous 9-byte reports.
The decoded X/Y values changed with gamepad input while the neutral position
was 128:128. This validates the host input path through axis decoding; motor and
encoder closed-loop behavior remain separate hardware tests.

## Control architecture

```text
USB Host HID gamepad
        |
        v
miraczv_controller_ops (pluggable command source)
        |
        v
differential_drive (v, omega -> left/right wheel setpoints)
        |
        +--> left PID  --> TB6612 channel A
        +--> right PID --> TB6612 channel B
        ^
        |
generic encoder API (QEIV2 backend)
```

The encoder API reports position and both mechanical speed units. The QEIV2
backend uses a 20 ms duration window, reads the actual QEI clock rate, and uses
the devicetree `counts-per-revolution` value. A complete window with no edges is
reported as a valid zero speed so the closed loop can start from standstill.
Invalid windows reset the corresponding PID and command that wheel to coast.

The controller abstraction can later accept DDS, MSP, a planner or an
autonomous path without changing the chassis loop. Another source only needs to
provide vehicle-coordinate `linear_mps` and `angular_radps`.

## Safety defaults

- A HID command expires after `CONFIG_MIRACZV_COMMAND_TIMEOUT_MS`.
- Command timeout coasts both motors but keeps TB6612 enabled, so a later valid
  command can recover without reinitializing the chassis.
- Neutral input stops PWM and releases both direction pins.
- Invalid or incomplete encoder speed windows reset the corresponding PID and
  command that wheel to zero output until a fresh window is available.
- Setpoints are clamped by the configured wheel and remote-control limits.
- `miraczv_chassis_disable()` is the explicit emergency path that also drops
  TB6612 STBY.

## Current limits

- Motorbus voltage, encoder polarity and physical CPR must be checked on the
  actual vehicle.
- HID input currently uses X/Y axes. Trigger, hat and button mappings are not
  implemented yet.
- HID enumeration, descriptor parsing, X/Y report traffic and continuous input
  have board coverage. Hot-plug recovery still needs an explicit unplug/replug
  test.
- BMI088, DDS, MSP, odometry, trajectory planning and advanced kinematics are
  not included.
- MiracZV has booted on the board and the wireless gamepad HID path is
  validated. Complete motor/encoder closed-loop validation is still pending.

## Tests

The descriptor parser and PID core have native test suites:

```sh
west twister -p native_sim -T tests
```

## License

- Firmware source code, build scripts, and configuration: Apache-2.0.
- Documentation: CC-BY-4.0.
- Hardware design files (not yet published): CERN-OHL-P-2.0.
- Third-party components retain their upstream licenses and notices.

See [LICENSE](LICENSE), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and
[CONTRIBUTION.md](CONTRIBUTION.md).
