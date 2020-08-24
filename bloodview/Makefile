VARIANT = release

BUILDDIR = build/$(VARIANT)

CFLAGS = -Wall -Wextra -pedantic --std=gnu11
LDFLAGS = -pthread

ifeq ($(VARIANT), release)
	CFLAGS += -O2 -DNDEBUG
else
	CFLAGS += -O0 -g
	LDFLAGS += -g
endif

MKDIR =	mkdir -p

BV_SRC = \
	src/bloodview.c

BV_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(BV_SRC)))

all: bloodview
clean:
	rm -rf build bloodview
	make -C sdl-tk clean

sdl-tk/sdl-tk.a:
	make -BC sdl-tk VARIANT=$(VARIANT)

bloodview: $(BV_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BV_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(BUILDDIR)/src
	$(CC) $(CFLAGS) -c -o $@ $<

# We need to run with sudo to open the device.
run: bloodview
	@sudo ./bloodview
