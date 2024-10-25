.PHONY: %.eap dockerbuild 3rd-party-clean clean very-clean

PROG = modbusacap
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
STRIP ?= strip
RM ?= rm -f

PKGS = gio-2.0 glib-2.0 axevent axparameter libmodbus
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

CFLAGS += -Wformat=2 -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Wdisabled-optimization -Wall -Werror

# main targets
all: $(PROG)
	$(STRIP) $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

# docker build container targets
%.eap:
	DOCKER_BUILDKIT=1 docker build --build-arg ARCH=$(basename $@) -o type=local,dest=. "$(CURDIR)"

dockerbuild: armv7hf.eap aarch64.eap

# clean targets
clean:
	$(RM) $(PROG) *.o *.eap *LICENSE.txt pa*conf*

very-clean: clean
	$(RM) *.old
