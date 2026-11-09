<!-- SPDX-FileCopyrightText: (C) 2026 Deskflow Developers -->
<!-- SPDX-License-Identifier: MIT -->

# Desktop sharing experience: draft integration

This change collects a privately developed macOS/Windows sharing experience into
one reviewable branch based on upstream master. It is a large experimental change;
it is not ready to merge. Code and this technical inventory were prepared with AI.

## Scope

- A common connection overview, keyboard supplier/receiver roles, stable status
  areas, concise controls, Chinese translations and responsive widget layouts.
- macOS permission onboarding with rechecking and a clear paused state.
- Scoped LAN discovery, saved computers, identity comparison and authenticated
  peer bindings; explicit pairing remains necessary for unknown peers.
- Address and connection recovery, sleep/wake signals and explicit pause handling.
- Native display topology, per-display routing messages and compatibility fallback.
- A shared display workspace with dragging, snapping, overlap validation,
  editable drafts, persistence, and read-only applied topology on the receiver.
- Isolated Candidate settings, packaging identities, startup/recovery providers,
  credential storage and synthetic regression fixtures.

## Build boundaries and remaining work

The default Deskflow build and the separately named Candidate build are different
configurations of the same source tree. Several network-probe and startup paths
remain Candidate-only. Default Deskflow builds must not be described as having
passed the Candidate installation and physical-device checks.

`DESKFLOW_CANDIDATE_BUILD=ON` selects the independently named Candidate and its
isolated settings. `DESKFLOW_LAB_BUILD=ON` disables input/core execution for
synthetic GUI tests. `DESKFLOW_UI_REVIEW_BUILD=ON` additionally provides an inert
UI review entry point. These modes are mutually constrained by CMake. Native
installers and startup persistence need platform-specific review; macOS Candidate
packages are built with `BUILD_INSTALLER=OFF`.

Linux has fallback behavior for unsupported native discovery/display providers.
A Linux compile/test result does not establish Linux feature parity. The
fullscreen interactions mentioned during development have not been validated;
this draft does not claim to fix them. Secure desktops, macOS secure input and
system authorization prompts are not bypassed.

The protocol extension and credential/startup changes need particular maintainer
review, including compatibility with unmodified peers, topology updates during
reconnects and migration into the normal upstream runtime. The UI still has
workflow and polish work outstanding. No project issue is claimed fixed solely
from the local user report.

## Reproducing checks

Configure the normal build with `BUILD_TESTS=ON`, `SKIP_BUILD_TESTS=ON` and
`BUILD_INSTALLER=OFF`, then build and run:

```sh
ctest --test-dir build/src/unittests --output-on-failure
```

The standard suite includes the connection overview, topology protocol,
IPC cancellation/fragmentation and runtime-profile fixtures. GUI tests need Qt's
SVG image plugin; headless runs use `QT_QPA_PLATFORM=offscreen`. Additional isolated
fixtures are kept under `native-tests/`; their CMake projects reference only files
inside this repository. Lab-only lifecycle tests use synthetic peers and isolated
settings, and must not be represented as physical keyboard/mouse verification.

A locally installed Candidate was used on macOS and Windows 11. The user reported
that the two computers were connected and input sharing was working. This is
limited user acceptance evidence, not exhaustive coverage of every feature or
both input directions. Upstream integration checks are recorded independently.
