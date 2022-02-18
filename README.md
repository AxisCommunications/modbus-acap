*Copyright (C) 2022, Axis Communications AB, Lund, Sweden. All Rights Reserved.*

# Modbus ACAP

[![Build ACAPs](https://github.com/AxisCommunications/modbus-acap/actions/workflows/build.yml/badge.svg)](https://github.com/AxisCommunications/modbus-acap/actions/workflows/build.yml)
[![GitHub Super-Linter](https://github.com/AxisCommunications/modbus-acap/actions/workflows/super-linter.yml/badge.svg)](https://github.com/AxisCommunications/modbus-acap/actions/workflows/super-linter.yml)

This repository contains the source code to build a small prototype ACAP
application that exports events from
[AXIS Object Analytics](https://www.axis.com/products/axis-object-analytics)
(AOA) over
[Modbus](https://en.wikipedia.org/wiki/Modbus) using
[libmodbus](https://libmodbus.org/). The ACAP can be run in either server or
client mode, meaning two Axis devices can be used to showcase it.

*The purpose of this repo is to serve as boilerplate code and keep things
simple, hence it uses basic Modbus/TCP without TLS and such.*

## Build

The build step creates `eap` (embedded application package) packages that can
then be deployed on the target Axis device e.g. via the device's web UI.

*For more information about the `eap` files, their content, and other ways to
deploy, please see the
[ACAP Developer Guide](https://help.axis.com/acap-3-developer-guide).*

### On host with ACAP SDK installed

```sh
# With the environment initialized, use:
acap-build .
```

### Using ACAP SDK build container and Docker

The handling of this is integrated in the [Makefile](Makefile), so if you have
Docker on your computer all you need to do is:

```sh
make dockerbuild
```

or perhaps build in parallel:

```sh
make -j dockerbuild
```

If you do have Docker but no `make` on your system:

```sh
# 32-bit ARM, e.g. ARTPEC-6- and ARTPEC-7-based devices
DOCKER_BUILDKIT=1 docker build --build-arg ARCH=armv7hf -o type=local,dest=. .
# 64-bit ARM, e.g. ARTPEC-8-based devices
DOCKER_BUILDKIT=1 docker build --build-arg ARCH=aarch64 -o type=local,dest=. .
```

## Run

The ACAP can be run in either client mode or server mode (default), configured
with the application parameter `Mode`:

In client mode, it will subscribe to
[AXIS Object Analytics](https://www.axis.com/products/axis-object-analytics)
(AOA) events for the AOA scenario specified in the application parameter
`Scenario` (default: Scenario 1) and send the trigger status (active/inactive)
over Modbus (TCP) to the Modbus server running on the host set in the ACAP's
parameter `Server`. libmodbus' `MODBUS_TCP_DEFAULT_PORT` (502) is used.

In server mode, it will listen for incoming TCP requests on the libmodbus
`MODBUS_TCP_DEFAULT_PORT` (502) and print incoming AOA status updates to the
application log.

## License

[Apache 2.0](./LICENSE)