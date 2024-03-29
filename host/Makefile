VARIANT = release

BUILDDIR = build/$(VARIANT)

CFLAGS = -Wall -Wextra -pedantic --std=gnu11 -I..

CFLAGS += -MMD -MP

CFLAGS += $(shell pkg-config libudev --cflags)
LDFLAGS += $(shell pkg-config libudev --libs)

REVISION ?= 1
CFLAGS += -DBL_REVISION=$(REVISION)
CFLAGS += -DBL_COMMIT_SHA=\"$(shell git rev-parse --verify HEAD)\"

BV_CFLAGS = -Ibloodview/sdl-tk/include
BV_LDFLAGS = -pthread

BV_CFLAGS += $(shell pkg-config sdl2 SDL2_ttf --cflags)
BV_LDFLAGS += $(shell pkg-config sdl2 SDL2_ttf --libs)

BV_CFLAGS += $(shell pkg-config yaml-0.1 --cflags)
BV_LDFLAGS += $(shell pkg-config yaml-0.1 --libs)

BV_CFLAGS += -Ibloodview/libcyaml/include
BV_LDFLAGS += -Lbloodview/libcyaml/$(BUILDDIR)/ -lcyaml

BLOODVIEW_ENV += XDG_RUNTIME_DIR=$(XDG_RUNTIME_DIR)

ifeq ($(VARIANT), release)
	CFLAGS += -O2 -DNDEBUG
else
	CFLAGS += -O0 -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover
	LDFLAGS += -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover
	BLOODVIEW_ENV += LSAN_OPTIONS=suppressions=../bloodview/resources/lsan-suppr:print_suppressions=0
endif

MKDIR =	mkdir -p

BV_DPP_SRC = \
	bloodview/src/dpp/filter/derivative.c \
	bloodview/src/dpp/filter/average.c \
	bloodview/src/dpp/filter.c \
	bloodview/src/dpp/param.c \
	bloodview/src/dpp/file.c \
	bloodview/src/dpp/dpp.c

BV_SRC = $(BV_DPP_SRC) \
	bloodview/src/data-invert.c \
	bloodview/src/derivative.c \
	bloodview/src/bloodview.c \
	bloodview/src/main-menu.c \
	bloodview/src/data-avg.c \
	bloodview/src/data-cal.c \
	bloodview/src/device.c \
	bloodview/src/graph.c \
	bloodview/src/util.c \
	bloodview/src/data.c \
	bloodview/src/sdl.c

BV_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(BV_SRC)))
BV_DEP = $(patsubst %.c,%.d, $(addprefix $(BUILDDIR)/,$(BV_SRC)))

BV_DEPS = \
	bloodview/libcyaml/$(BUILDDIR)/libcyaml.a \
	bloodview/sdl-tk/sdl-tk.a

COMMON_SRC = \
	common/device.c \
	common/fifo.c \
	common/msg.c \
	common/sig.c

COMMON_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(COMMON_SRC)))
COMMON_DEP = $(patsubst %.c,%.d, $(addprefix $(BUILDDIR)/,$(COMMON_SRC)))

TOOLS_SRC = \
	tools/bl.c \
	tools/bpm.c \
	tools/fft.c \
	tools/util.c \
	tools/convert.c \
	tools/calibrate.c \
	tools/normalize.c

TOOLS_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(TOOLS_SRC)))
TOOLS_DEP = $(patsubst %.c,%.d, $(addprefix $(BUILDDIR)/,$(TOOLS_SRC)))

all: tools build/bloodview
clean:
	rm -rf build
	make -C bloodview/sdl-tk clean
	make -C bloodview/libcyaml clean

tools: build/bl \
	build/bpm \
	build/fft \
	build/convert \
	build/calibrate \
	build/normalize

bloodview/sdl-tk/sdl-tk.a:
	make -BC bloodview/sdl-tk VARIANT=$(VARIANT)

bloodview/libcyaml/$(BUILDDIR)/libcyaml.a:
	make -BC bloodview/libcyaml VARIANT=$(VARIANT)

build/bloodview: $(BV_OBJ) $(BV_DEPS) $(COMMON_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(BV_LDFLAGS)

$(BV_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) $(BV_CFLAGS) -c -o $@ $<

$(COMMON_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TOOLS_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

build/bl: $(BUILDDIR)/tools/bl.o $(BUILDDIR)/tools/util.o $(COMMON_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

build/convert: $(BUILDDIR)/tools/convert.o $(COMMON_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

build/calibrate: $(BUILDDIR)/tools/calibrate.o $(COMMON_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

build/normalize: $(BUILDDIR)/tools/normalize.o $(BUILDDIR)/tools/util.o $(COMMON_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

build/fft: $(BUILDDIR)/tools/fft.o $(BUILDDIR)/tools/util.o $(COMMON_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) -lfftw3 -lm

build/bpm: $(BUILDDIR)/tools/bpm.o $(BUILDDIR)/tools/util.o $(COMMON_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# We need to run with sudo to open the device.
run: build/bloodview
	@sudo $(BLOODVIEW_ENV) build/bloodview \
			-R bloodview/resources \
			-C bloodview/config \
			$(BV_ARGS)

# Run variant that load the default conifg for the device
rund: build/bloodview
	@sudo $(BLOODVIEW_ENV) build/bloodview -d \
			-R bloodview/resources \
			-C bloodview/config \
			$(BV_ARGS)

# Run variant that reloads the previous config
runp: build/bloodview
	@sudo $(BLOODVIEW_ENV) build/bloodview -p \
			-R bloodview/resources \
			-C bloodview/config \
			$(BV_ARGS)

docs:
	$(MKDIR) build/docs
	doxygen bloodview/docs/doxygen.conf

-include $(BV_DEP) $(COMMON_DEP) $(TOOLS_DEP)

.PHONY: all clean docs
