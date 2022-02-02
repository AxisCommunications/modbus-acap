# Modbus ACAP

This repository contains the source code to build a small ACAP application that
exports events from
[AXIS Object Analytics](https://www.axis.com/products/axis-object-analytics)
(AOA) over
[Modbus](https://en.wikipedia.org/wiki/Modbus) using
[libmodbus](https://libmodbus.org/). The ACAP can be run in either server or
client mode, meaning two Axis devices can be used to showcase it.

## Build

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
# 32-bit ARM
DOCKER_BUILDKIT=1 docker build --build-arg ARCH=armv7hf -o type=local,dest=. .
# 64-bit ARM
DOCKER_BUILDKIT=1 docker build --build-arg ARCH=aarch64 -o type=local,dest=. .
```

## Run

The ACAP can be run in either client mode or server mode (default):

In client mode, it will subscribe to
[AXIS Object Analytics](https://www.axis.com/products/axis-object-analytics)
(AOA) events and send the trigger status (active/inactive) over Modbus (TCP) to
the Modbus server running on the host set in the ACAP's parameter `server`.
libmodbus' `MODBUS_TCP_DEFAULT_PORT` (502) is used.

In server mode, it will listen for incoming TCP requests on the libmodbus
`MODBUS_TCP_DEFAULT_PORT` (502) and print incoming AOA status updates to the
application log.
