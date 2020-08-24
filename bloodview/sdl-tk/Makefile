VARIANT = release

BUILDDIR = build/$(VARIANT)

CFLAGS = -Wall -Wextra -pedantic --std=gnu11 -Iinclude -I../..
LDFLAGS =

CFLAGS += $(shell pkg-config sdl2 SDL2_ttf --cflags)
LDFLAGS += $(shell pkg-config sdl2 SDL2_ttf --libs)

ifeq ($(VARIANT), release)
	CFLAGS += -O2 -DNDEBUG
else
	CFLAGS += -O0 -g
	LDFLAGS += -g
endif

MKDIR =	mkdir -p

LIB_SRC = \
	src/text.c \
	src/colour.c

LIB_OBJ = $(patsubst %.c,%.o, $(addprefix $(BUILDDIR)/,$(LIB_SRC)))

all: sdl-tk.a
clean:
	rm -rf build sdl-tk.a

sdl-tk.a: $(LIB_OBJ)
	$(AR) -rcs $@ $^

$(LIB_OBJ): $(BUILDDIR)/%.o : %.c
	@$(MKDIR) $(BUILDDIR)/src
	$(CC) $(CFLAGS) -c -o $@ $<
