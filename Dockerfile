ARG ARCH=aarch64
ARG ACAP_SDK_VERSION=1.15
ARG SDK_IMAGE=axisecp/acap-native-sdk
ARG LIBMODBUS_VERSION=3.1.11
ARG BUILD_DIR=/usr/local/src
ARG ACAP_BUILD_DIR="$BUILD_DIR"/app
ARG LIBMODBUS_BUILD_DIR="$BUILD_DIR"/libmodbus

FROM $SDK_IMAGE:$ACAP_SDK_VERSION-$ARCH AS builder
ARG ACAP_BUILD_DIR
ARG LIBMODBUS_BUILD_DIR
ARG LIBMODBUS_VERSION
ENV DEBIAN_FRONTEND=noninteractive

# Install additional build dependencies
# hadolint ignore=DL3008
RUN apt-get update && apt-get install -y -f --no-install-recommends \
    autoconf \
    automake \
    libtool

# Build libmodbus
WORKDIR "$LIBMODBUS_BUILD_DIR"
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN curl -L https://github.com/stephane/libmodbus/archive/refs/tags/v$LIBMODBUS_VERSION.tar.gz | tar --strip-components=1 -xz
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN . /opt/axis/acapsdk/environment-setup* && \
    ./autogen.sh && \
    ./configure --host="$ARCH" --prefix=/usr --enable-static=yes --enable-shared=no && \
    make -j "$(nproc)" install prefix="$SDKTARGETSYSROOT"/usr

# Build ACAP package
WORKDIR "$ACAP_BUILD_DIR"
COPY html/ ./html
ADD https://code.jquery.com/jquery-3.7.1.min.js ./html/js/jquery.min.js
COPY LICENSE \
     Makefile \
     manifest.json \
     *.c \
     *.h \
     ./
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
RUN . /opt/axis/acapsdk/environment-setup* && \
    acap-build .

FROM scratch
ARG ACAP_BUILD_DIR
COPY --from=builder "$ACAP_BUILD_DIR"/*eap "$ACAP_BUILD_DIR"/*LICENSE.txt /
