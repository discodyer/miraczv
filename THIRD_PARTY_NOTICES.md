<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Third-Party Notices

MiracZV does not vendor the Zephyr RTOS, HPM SDK, or `hpm-zephyr` module in this
repository. They are fetched as west projects or supplied by the development
workspace and remain subject to their upstream licenses and notices.

| Component | Version / repository | License | Distribution |
|---|---|---|---|
| Zephyr RTOS | v4.5.0, `zephyrproject-rtos/zephyr` | Apache-2.0 | External west project |
| HPM SDK | v1.13.0, `hpmicro/hpm_sdk` | BSD-3-Clause | External west project |
| hpm-zephyr | `discodyer/hpm-zephyr` | Apache-2.0 | External west project |
| Zephyr SDK / toolchain | Zephyr SDK 1.0.1 | Toolchain and component licenses | Build environment |

The HPM SDK is not part of MiracZV and is not relicensed by this project. Its
BSD-3-Clause text and copyright notices are retained in the upstream checkout.
Zephyr and `hpm-zephyr` retain their Apache-2.0 notices.

If a future change adds a vendored third-party component, its source files must
retain their original SPDX identifiers and license texts, and this table must be
updated with its version, source, license, and local path.
