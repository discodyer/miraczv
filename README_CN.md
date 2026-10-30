<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# MiracZV

[English](README.md) | 简体中文

MiracZV（Miracle + Zephyr + RISC-V）是面向 HPM5361 的紧凑型差速小车控制器。
它参考 MiracZ7 的应用模型，但基于 Zephyr v4.5.0 和上游 USB Host 协议栈重新实现，
当前只保留一条较小、可验证的外设路径：

- TB6612FNG 双路电机 PWM 与方向控制
- 通用编码器抽象，以及 HPMicro QEIV2 后端
- 通过 HID 报告描述符自动识别 X/Y 轴的 USB Host 输入
- 可扩展的控制器抽象，后续可接入 DDS、MSP 或自主控制源
- 100 Hz 差速运动学与左右轮独立速度 PID
- 单精度、双精度硬件浮点
- 暂不包含 DDS、BMI088、显示屏和高级车辆模块

## 硬件

目标板为 HPM5300EVK RevD，SoC 为 HPM5361ICB1。

| 功能 | MCU 信号 | 排针 |
|---|---|---|
| 左电机 PWM | PA26 / PWM0.P2 | J7-11 |
| 右电机 PWM | PA27 / PWM0.P3 | J7-12 |
| 左电机 IN1 | PA10 / GPIOA10 | P1-12 |
| 左电机 IN2 | PA11 / GPIOA11 | P1-16 |
| 右电机 IN1 | PB10 / GPIOB10 | P1-36 |
| 右电机 IN2 | PB11 / GPIOB11 | P1-40 |
| 电机待机 | PA12 / GPIOA12 | P1-11 |
| 左编码器 A | PA20 / QEI0.A | J7-1 |
| 左编码器 B | PA21 / QEI0.B | J7-3 |
| 右编码器 A | PB04 / QEI1.A | P1-31 |
| 右编码器 B | PB05 / QEI1.B | P1-32 |
| USB Host | USB0 | J1 USB_OTG |
| 控制台 | UART0 | J8 FT2232 |

TB6612 的 VM 必须由独立电机电源供电，并与开发板共地。不可将电机电源接到
EVK 排针；排针只承载上表中的 3.3 V 逻辑信号。

## 构建

仓库清单固定 Zephyr、HPM SDK 和 `hpm-zephyr` 的版本：

```sh
west init -l miraczv
west update
cd miraczv
./build.sh
```

在当前开发工作区中，等价命令为：

```sh
export HPM_SDK_BASE=/workdir/zephyr-workspace/hpm_sdk
export ZEPHYR_EXTRA_MODULES=/workdir/zephyr-workspace/hpm-zephyr

west build -p always -b hpm5300evk/hpm5361 -d build/miraczv \
  miraczv -- \
  -DDTC_OVERLAY_FILE=boards/hpm5300evk_miraczv.overlay
```

应用不会修改 Zephyr、`hpm_sdk` 或 `hpm-zephyr`。

## USB Host 输入

将支持 OTG 的转接头接在 J1，再连接 USB HID 手柄或其无线接收器。MiracZV 会读取
HID 报告描述符，定位 Generic Desktop X/Y usage，并使用其中的 Report ID、位偏移、
字段宽度、符号属性及逻辑范围。当前映射如下：

- X 轴：转向
- Y 轴：油门，默认反向，使摇杆向前推时车辆前进
- 可用 `CONFIG_MIRACZV_HID_INVERT_X` 和
  `CONFIG_MIRACZV_HID_INVERT_Y` 调整轴方向
- 死区：`CONFIG_MIRACZV_HID_AXIS_DEADZONE_PERMILLE`
- 线速度限制：`CONFIG_MIRACZV_MAX_LINEAR_MPS`
- 角速度限制：`CONFIG_MIRACZV_MAX_ANGULAR_MRADPS`

HID 类驱动使用应用私有的固定 `net_buf` 池。通用 UHC 缓冲池仍用于控制请求和
Hub 枚举，手柄描述符与中断报告则由 `MIRACZV_HID_BUFFER_BYTES` 限定。这样可避免
热插拔时类驱动流量使全局 UHC 堆增长或碎片化。最初的动态 UHC 缓冲路径在
HPM5361 上于描述符传输后出现过堆损坏；改用手柄私有固定池后，该故障已在实机
验证中消失。

2026-10-30 的实机验证中，MiracZV 成功枚举无线 USB HID 接收器，读取并解析其
123 字节报告描述符，并持续接收到 9 字节报告。X/Y 值随手柄操作变化，回中值为
128:128。这验证了 USB Host 输入链路和轴解码；电机与编码器闭环仍需分别做实机测试。

## 控制架构

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

编码器 API 同时提供位置和两种机械速度单位。QEIV2 后端使用 20 ms 时间窗口，读取
实际 QEI 时钟频率，并使用 Devicetree 中的 `counts-per-revolution`。完整窗口内无
边沿时按有效的零速度处理，使闭环能够从静止起步；测速窗口无效时则复位对应 PID，
并让该轮滑行。

控制器抽象后续可接入 DDS、MSP、路径规划器或自主控制路径，而无需修改底盘控制
环。新控制源只需提供车辆坐标系下的 `linear_mps` 和 `angular_radps`。

## 安全默认值

- HID 命令在 `CONFIG_MIRACZV_COMMAND_TIMEOUT_MS` 后失效。
- 命令超时后两路电机滑行，但保持 TB6612 使能，使后续有效命令无需重新初始化底盘。
- 摇杆回中时停止 PWM 并释放两路方向引脚。
- 无效或未完成的编码器测速窗口会复位对应 PID，并将该轮输出置零，直到出现新窗口。
- 所有设定值均受轮速和遥控输入限制约束。
- `miraczv_chassis_disable()` 是显式紧急路径，同时释放 TB6612 STBY。

## 当前限制

- 实际车辆上必须确认电机电源电压、编码器极性和机械 CPR。
- HID 当前只使用 X/Y 轴，尚未实现扳机、帽键和按键映射。
- HID 枚举、描述符解析、X/Y 报告和持续输入已完成实机验证；热插拔恢复仍需
  单独的拔插测试。
- 暂不包含 BMI088、DDS、MSP、里程计、轨迹规划和高级运动学。
- MiracZV 已在开发板启动且无线手柄 HID 路径已通过实机验证；完整的电机/编码器
  闭环验证尚未完成。

## 测试

描述符解析器和 PID 核心具有 native 测试：

```sh
west twister -p native_sim -T tests
```

## 许可证

- 固件源代码、构建脚本和配置：Apache-2.0。
- 项目文档：CC-BY-4.0。
- 硬件设计文件（尚未上传）：CERN-OHL-P-2.0。
- 第三方组件沿用其原有许可证和声明。

详细信息参见 [LICENSE](LICENSE)、[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
和 [CONTRIBUTION.md](CONTRIBUTION.md)。
