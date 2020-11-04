VARIANT = release

BUILDDIR = build/$(VARIANT)

CFLAGS = -Wall -Wextra -pedantic --std=gnu11 -Isdl-tk/include -I..
LDFLAGS = -pthread

CFLAGS += $(shell pkg-config sdl2 SDL2_ttf --cflags)
LDFLAGS += $(shell pkg-config sdl2 SDL2_ttf --libs)

CFLAGS += $(shell pkg-config libudev --cflags)
LDFLAGS += $(shell pkg-config libudev --libs)

CFLAGS += $(shell pkg-config yaml-0.1 --cflags)
LDFLAGS += $(shell pkg-config yaml-0.1 --libs)

CFLAGS += -Ilibcyaml/include
LDFLAGS += -Llibcyaml/$(BUILDDIR)/ -lcyaml

BLOODVIEW_ENV += XDG_RUNTIME_DIR=$(XDG_RUNTIME_DIR)

ifeq ($(VARIANT), release)
	CFLAGS += -O2 -DNDEBUG
else
	CFLAGS += -O0 -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover
	LDFLAGS += -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover
	BLOODVIEW_ENV += LSAN_OPTIONS=suppressions=resources/lsan-suppr:print_suppressions=0
endif

MKDIR =	mkdir -p

BV_SRC = \
	src/data-invert.c \
	src/derivative.c \
	src/bloodview.c \
	src/main-menu.c \
	src/data-avg.c \
	src/data-cal.c \
	src/device.c \
	src/graph.c \
	src/data.c \
	src/sdl.c

BV_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(BV_SRC)))

# TODO: improve how we get tool stuff
TOOL_OBJ = \
	../tools/device.o \
	../tools/msg.o \
	../tools/sig.o

all: bloodview
clean:
	rm -rf build bloodview
	make -C sdl-tk clean
	make -C libcyaml clean

sdl-tk/sdl-tk.a:
	make -BC sdl-tk VARIANT=$(VARIANT)

libcyaml/$(BUILDDIR)/libcyaml.a:
	make -BC libcyaml VARIANT=$(VARIANT)

bloodview: $(BV_OBJ) $(TOOL_OBJ) sdl-tk/sdl-tk.a libcyaml/$(BUILDDIR)/libcyaml.a
	$(CC) -o $@ $^ $(LDFLAGS)

$(BV_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(BUILDDIR)/src
	$(CC) $(CFLAGS) -c -o $@ $<

# We need to run with sudo to open the device.
run: bloodview
	@sudo $(BLOODVIEW_ENV) ./bloodview $(BV_ARGS)

docs:
	$(MKDIR) build/docs
	doxygen docs/doxygen.conf

.PHONY: all clean docs
