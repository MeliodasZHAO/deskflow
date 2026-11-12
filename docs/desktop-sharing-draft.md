<!-- SPDX-FileCopyrightText: (C) 2026 Deskflow Developers -->
<!-- SPDX-License-Identifier: MIT -->

# Desktop sharing workspace: integration notes

This branch is a large draft, not a merge-ready release. Code and these technical
notes were prepared with AI; the contributor is responsible for understanding and
explaining the change. The PR description and community messages must be written
by its human contributor.

## Changes intended for the normal application

- A shared connection overview with explicit input supplier/receiver roles,
  concise actions, Chinese translations, stable reconnect status height and
  responsive layouts.
- macOS input-permission onboarding before the main window or input core starts.
- Scoped LAN discovery, saved peers and explicit identity comparison. Unknown
  devices are never trusted merely because they advertise a familiar name.
- Address recovery uses the configured TLS identity and the saved peer's SHA-256
  certificate pin. It binds to the selected local address, verifies an actual TLS
  handshake and certificate lifetime, sends no Deskflow client name or input
  messages, and stops when the selected interface changes. The ordinary build no
  longer depends on the Candidate credential-staging helper for this operation.
- Native display topology, a validated workspace model, display arrangement drafts
  and per-display routing messages negotiated with compatible encrypted peers.
  The input supplier owns the arrangement; receivers show the applied snapshot.
  Older peers retain the compatibility computer arrangement.
- Explicit pause/stop remains authoritative. Queued recovery callbacks cannot
  resume a stopped session. A duplicate client name receives three four-second
  retries for old-session expiry, followed by an explicit manual retry action.
  Recovery never evicts another client, renames the computer or changes trust.

## Compatibility and draft boundaries

The workspace and correlated recovery are designed around TLS and peer checking.
Existing insecure configurations need explicit compatibility review; successful
compilation does not establish that every old non-TLS workflow is preserved.
The protocol extension, IPC lifecycle and identity changes require maintainer
review before merging.

The Candidate/Lab build options are optional evaluation variants, not separate
implementations of the UI. Candidate settings, credentials, service identities and
packages remain isolated. Lab fixtures disable input/core execution. Candidate
startup recovery plans and mutation providers are not a verified migration path
for normal upstream startup settings. macOS startup mutations stay off by default.
The Linux native discovery fallback reports unsupported functionality and retains
manual connection; a Linux compile result does not establish full native feature
parity. Fullscreen interactions, secure-input boundaries and privileged desktops
have not been established as fixed and are not bypassed.

## Validation

Normal builds use `BUILD_TESTS=ON`, `SKIP_BUILD_TESTS=ON`, and
`BUILD_INSTALLER=OFF`. Keep translation editing separate with
`UPDATE_TRANSLATIONS=OFF` when reproducing a frozen validation tree.

```sh
cmake --build build --parallel 2
ctest --test-dir build/src/unittests --output-on-failure
```

Headless widget tests use `QT_QPA_PLATFORM=offscreen` and require the Qt SVG image
plugin. `ConfiguredTlsProbeTests` makes real TLS connections only to an isolated
loopback fixture with generated test identities. A test subclass permits that
loopback endpoint; the production policy separately rejects it. The suite checks
matching/mismatched pins, expired peers, cancellation and interface changes.

Visual comparisons distinguish real application captures, fixed widget examples
and missing evidence. Synthetic connected-state screenshots are not evidence of
physical input delivery. Keyboard/mouse acceptance is recorded separately from
unit-test counts, and neither screenshot totals nor QtTest setup/cleanup entries
are presented as business coverage.

Before requesting reopening, each proposed commit must compile, the final tree
must be built/tested on macOS, Windows and Linux, and current results must replace
old snapshot results. An earlier candidate installation or test run is not proof
that a newly reorganized normal build passes. The external contributor must
perform the final PR edits and community request in accordance with AGENTS.md.

## Offline integration suites

The optional Lab UI review can be built with `DESKFLOW_LAB_BUILD=ON`,
`DESKFLOW_UI_REVIEW_BUILD=ON`, `BUILD_TESTS=OFF`, `BUILD_INSTALLER=OFF`, and
`UPDATE_TRANSLATIONS=OFF`. Run its registered tests with
`ctest --test-dir build --output-on-failure`. Each suite gets a new private
profile; input/core launch gates remain disabled.

The old `FullWindowTests` scaffold referenced the removed `mainContentScroll`
view and was retired. Current home/navigation behavior is exercised by
`ConnectionHomeTests`, `LabHomeFlowTests`, `LabFirstSetupFlowTests` and
`DesktopProductFlowTests`; core stop/retry/late-event behavior is exercised by
`IpcLifecycleTests`, `ClientAppRecoveryTests`, `LabCoreSessionTests` and
`LabAutomaticRecoveryTests`. Wide windows use the sidebar; compact windows
allow content scrolling, and tests bring controls into view before asserting
keyboard reachability. Test fixtures do not assert that all content is visible
at once in a small window.
