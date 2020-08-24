VARIANT = release

BUILDDIR = build/$(VARIANT)

CFLAGS = -Wall -Wextra -pedantic --std=gnu11 -Isdl-tk/include
LDFLAGS = -pthread

CFLAGS += $(shell pkg-config sdl2 SDL2_ttf --cflags)
LDFLAGS += $(shell pkg-config sdl2 SDL2_ttf --libs)

ifeq ($(VARIANT), release)
	CFLAGS += -O2 -DNDEBUG
else
	CFLAGS += -O0 -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover
	LDFLAGS += -g -fsanitize=address -fsanitize=undefined -fno-sanitize-recover
	BLOODVIEW_ENV += LSAN_OPTIONS=suppressions=resources/lsan-suppr:print_suppressions=0
endif

MKDIR =	mkdir -p

BV_SRC = \
	src/bloodview.c \
	src/main-menu.c \
	src/data-avg.c \
	src/graph.c \
	src/data.c \
	src/sdl.c

BV_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(BV_SRC)))

all: bloodview
clean:
	rm -rf build bloodview
	make -C sdl-tk clean

sdl-tk/sdl-tk.a:
	make -BC sdl-tk VARIANT=$(VARIANT)

bloodview: $(BV_OBJ) sdl-tk/sdl-tk.a
	$(CC) -o $@ $^ $(LDFLAGS)

$(BV_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(BUILDDIR)/src
	$(CC) $(CFLAGS) -c -o $@ $<

# We need to run with sudo to open the device.
run: bloodview
	@sudo $(BLOODVIEW_ENV) ./bloodview
