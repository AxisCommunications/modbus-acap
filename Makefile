.PHONY: %.eap dockerbuild 3rd-party-clean clean very-clean

PROG = modbusacap
OBJS = $(PROG).o modbus_client.o modbus_server.o
STRIP ?= strip

PKGS = gio-2.0 glib-2.0 axevent axparameter
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

# libmodbus
LIBMODBUS_VERSION = 3.1.7
LIBMODBUS_DIR = $(CURDIR)/libmodbus-$(LIBMODBUS_VERSION)
LIBMODBUS = $(LIBMODBUS_DIR)/lib/libmodbus.a
CFLAGS += -I $(LIBMODBUS_DIR)/include/modbus
LDLIBS += $(LIBMODBUS)

CFLAGS += -Wformat=2 -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Wdisabled-optimization -Wall -Werror

# main targets
all: $(PROG)
	$(STRIP) $(PROG)

$(OBJS): $(LIBMODBUS)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

# libmodbus targets
$(LIBMODBUS_DIR):
	curl -L https://libmodbus.org/releases/libmodbus-$(LIBMODBUS_VERSION).tar.gz | tar xz

$(LIBMODBUS_DIR)/Makefile: | $(LIBMODBUS_DIR)
	cd $(LIBMODBUS_DIR) && \
	./configure --host=$(ARCH) --prefix=$(LIBMODBUS_DIR)

$(LIBMODBUS): $(LIBMODBUS_DIR)/Makefile
	make -C $(LIBMODBUS_DIR) -j install

# docker build container targets
%.eap:
	DOCKER_BUILDKIT=1 docker build --build-arg ARCH=$(@:.eap=) -o type=local,dest=. "$(CURDIR)"

dockerbuild: armv7hf.eap aarch64.eap

# clean targets
3rd-party-clean:
	make -C $(LIBMODBUS_DIR) clean

clean:
ifneq (,$(wildcard ./package.conf.orig))
	mv package.conf.orig package.conf
endif
	rm -f $(PROG) *.o *.eap *LICENSE.txt

very-clean: clean 3rd-party-clean
	rm -rf *.eap *.eap.old eap $(LIBMODBUS_DIR)
