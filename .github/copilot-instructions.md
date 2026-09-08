---
description: "Repository guidance for the Modbus ACAP application: C code, AXParameter and AOA event contracts, settings UI, packaging, builds, and validation."
applyTo: "**"
---

# Modbus ACAP

## Scope and architecture

- This is an ACAP v4 native application. Always write `AXIS OS`; use `ACAP` for Axis Camera Application Platform applications and packages.
- This is a prototype and boilerplate application, not a production-ready Modbus service. It exports AXIS Object Analytics (AOA) stateful events over Modbus/TCP.
- [`modbusacap.c`](../modbusacap.c) owns application startup and shutdown, [AXParameter](https://developer.axis.com/acap/acap-native-sdk-version-12/api/src/api/axparameter/html/index.html) callbacks, AOA subscriptions, mode changes, and the GLib main loop.
- [`modbus_client.c`](../modbus_client.c) owns the outgoing libmodbus TCP context and writes the configured coil when an AOA event changes state.
- [`modbus_server.c`](../modbus_server.c) owns the Modbus/TCP server thread and request handling.
- [`modbusacap_common.h`](../modbusacap_common.h) provides the shared `LOG_I` and `LOG_E` logging macros.

## Parameter and behavior contracts

- Keep [`manifest.json`](../manifest.json), the executable name and Makefile `PROG` value, the [AXParameter](https://developer.axis.com/acap/acap-native-sdk-version-12/api/src/api/axparameter/html/index.html) group, and the settings UI aligned on `modbusacap`/`Modbusacap` naming.
- When adding or renaming a parameter, update all applicable surfaces together: [`manifest.json`](../manifest.json) `paramConfig`, parameter registration and callback handling in [`modbusacap.c`](../modbusacap.c), and the controls plus `/axis-cgi/param.cgi` reads and writes in [`html/config.html`](../html/config.html) and [`html/modbusconfig.js`](../html/modbusconfig.js).
- Preserve parameter constraints across every layer: `ModbusAddress` is `0..65535`; `Mode` is `0` for Server and `1` for Client; `Port` is `1024..65535`; `Scenario` is at least `1`; and `Server` is a hostname or IP address.
- A `Mode`, `Port`, or `Server` change stops the current Modbus role before initializing the new role. Keep these operations protected by `lock` and preserve controlled thread shutdown through `modbus_server_stop()`.
- Keep the AOA event topic and subscription behavior aligned with `Device1Scenario<N>` and `Device1Scenario<N>Threshold`. In server mode events are subscribed to but not forwarded; in client mode state is written through Modbus.
- Validate external parameter values, events, network data, and libmodbus results before use. Do not assume externally supplied input is valid.

## C conventions

- Build with the project flags, including `-Wall`, `-Werror`, `-Wformat=2`, and strict prototype checks. Treat warnings as errors.
- Follow existing C style: 4-space indentation, Allman braces, declarations at the start of a block, `NULL != value` comparisons, braces around single-line blocks, and explicit error paths.
- Name file-static and global variables with a trailing underscore. Keep function parameters and local variables unsuffixed unless an established convention requires otherwise.
- Prefer direct expressions and local ownership handling when they are clear. Add a helper function or intermediate variable only when it removes meaningful duplication, enforces an invariant, or materially improves readability; do not add wrappers that only relocate a few lines of code.
- Always set `const` on anything that can be `const`.
- Always assert function parameters at the start of a function, then validate external input before relying on it. Retain assertions for internal invariants and use explicit return-value, errno, and `GError` handling for external failures.
- Never dereference a pointer without first establishing that it is not `NULL`.
- Preserve GLib and AXIS API types at their API boundaries. Free `GError` values after handling and release dynamically allocated GLib or AXIS objects according to their ownership rules.
- New C headers and sources use the existing Apache-2.0 Axis copyright and license header. Keep the existing include guards in C headers.
- Use `LOG_I` and `LOG_E`, retaining the existing `__FILE__/__FUNCTION__` context pattern for failures.

## UI, packaging, and validation

- The settings page is static HTML, CSS, and vanilla JavaScript. Preserve the existing tab indentation and asynchronous `fetch` style in [`html/modbusconfig.js`](../html/modbusconfig.js).
- [`Dockerfile`](../Dockerfile) cross-compiles `aarch64` and `armv7hf` ACAP packages with the native SDK and builds static libmodbus. Keep libmodbus version and SHA256 updates paired; Renovate manages routine dependency updates.
- Do not hand-edit generated root artifacts: `*.eap`, `*_LICENSE.txt`, `modbusacap`, object files, or `pa*.conf`. Regenerate packages through the container build.
- Build both architectures with `make -j "$(nproc)" dockerbuild` or `make -j "$(nproc)" podmanbuild`. Use `make aarch64.docker`, `make armv7hf.docker`, or the matching Podman targets for focused builds.
- There is no automated test suite. For C, manifest, Dockerfile, or dependency changes, run the relevant container build. [`LINT.md`](../LINT.md) documents local Super-Linter commands for formatting, Markdown, JSON, Dockerfile, and YAML changes.
- Validate every altered cross-file contract before finishing, and leave unrelated generated packages and dependency pins untouched.