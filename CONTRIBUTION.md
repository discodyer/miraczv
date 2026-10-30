<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Contributing to MiracZV

## Code style

Firmware and tooling follow the Zephyr Project coding conventions:

- C uses the Zephyr C style, currently defined by `.clang-format` and
  `.editorconfig`.
- C/C++ lines are limited to 100 columns; indentation uses tabs with an
  eight-column tab width.
- Devicetree, Kconfig, CMake, Python, YAML, and shell changes follow the
  corresponding Zephyr rules.
- New files must contain an SPDX license identifier.
- Run `clang-format`, `checkpatch.pl`, and the relevant build and tests before
  submitting a change.

The top-level `LICENSE` file contains the Apache-2.0 text used for firmware.
See `LICENSING.md` for the license scope of documentation and hardware design
materials. Files with an explicit SPDX identifier are governed by that
identifier.

## Commit messages

This repository uses Conventional Commits:

```text
<type>(<scope>): <summary>

<body>

Signed-off-by: Name <email>
```

Allowed examples include `feat`, `fix`, `docs`, `build`, `test`, `refactor`,
`chore`, and `ci`. Keep the summary imperative and lower-case. Explain what
changed, why it changed, and how it was validated in the body. Do not use WIP,
fixup, squash, or merge commits in submitted history.

## Developer Certificate of Origin

Every commit must include a DCO sign-off:

```text
Signed-off-by: Your Name <your-email>
```

The sign-off must match the commit author identity and certifies the Developer
Certificate of Origin, version 1.1:

<https://developercertificate.org/>

Use `git commit -s` to add the sign-off. AI assistance does not replace the
human contributor's review, licensing confirmation, or DCO certification.

## Licensing contributions

By contributing, you agree that your contribution is submitted under the
license applicable to the affected material:

- Firmware source, build scripts, and configuration: Apache-2.0.
- Documentation and explanatory written material: CC-BY-4.0.
- Hardware design files, when added: CERN-OHL-P-2.0.

Only contribute material that you have the right to submit. Preserve all
third-party copyright and SPDX notices, and update `THIRD_PARTY_NOTICES.md`
when adding a dependency or vendored component.
