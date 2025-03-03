.PHONY: %.docker %.podman dockerbuild podmanbuild clean very-clean

PROG = modbusacap
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
STRIP ?= strip
RM ?= rm -f
ARCHS = aarch64 armv7hf

PKGS = gio-2.0 glib-2.0 axevent axparameter libmodbus
CFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

CFLAGS += -Wformat=2 -Wpointer-arith -Wbad-function-cast -Wstrict-prototypes -Wdisabled-optimization -Wall -Werror

# main targets
all: $(PROG)
	$(STRIP) $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ $(LIBS) $(LDLIBS) -o $@

# container build targets
%.docker %.podman:
	DOCKER_BUILDKIT=1 $(patsubst .%,%,$(suffix $@)) build --build-arg ARCH=$(*F) -o type=local,dest=. "$(CURDIR)"

dockerbuild: $(addsuffix .docker,$(ARCHS))
podmanbuild: $(addsuffix .podman,$(ARCHS))

# clean targets
clean:
	$(RM) $(PROG) *.o *.eap *LICENSE.txt pa*conf*

very-clean: clean
	$(RM) *.old
