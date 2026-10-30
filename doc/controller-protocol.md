<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Controller abstraction

The application only depends on:

```c
struct miraczv_controller_ops {
	const char *name;
	int (*init)(void);
	int (*poll)(struct miraczv_command *command);
};
```

`init()` is called once after a source is registered and activated. `poll()`
returns 0 for a fresh command, `-EAGAIN` while no update is available, and a
negative error code for a disconnected or failed source. `valid` must be true
for a successful poll. The command is expressed in vehicle coordinates:

- `linear_mps`: forward velocity
- `angular_radps`: positive yaw rate

The current HID source fills these fields from a gamepad X/Y axis report.
Another source such as DDS, MSP or a planner can fill the same structure
without changing the drive controller. A source should report its acquisition
timestamp and monotonically increasing sequence number when available.

The control loop runs every `CONFIG_MIRACZV_CONTROL_PERIOD_MS`, converts the
vehicle command to left/right wheel setpoints, reads both encoders, updates
independent PID controllers and writes TB6612 PWM plus direction outputs. The
differential conversion is:

```text
left  = linear_mps - angular_radps * track_width_m / 2
right = linear_mps + angular_radps * track_width_m / 2
```

A command that becomes older than `CONFIG_MIRACZV_COMMAND_TIMEOUT_MS` is
discarded, both motor outputs coast, and the PID state is reset. TB6612 standby
remains asserted for normal timeout recovery; dropping standby is reserved for
an explicit emergency disable.
