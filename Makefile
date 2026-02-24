CC      := gcc
CFLAGS  := -O2 -march=native -Wall -Wextra -Wpedantic -std=c17 -D_GNU_SOURCE \
           -Isrc -MMD -MP
LDFLAGS := -lssl -lcrypto -lpthread
TARGET  := nproxy
SRCDIR  := src
BUILDDIR := build

SRCS := $(shell find $(SRCDIR) -name '*.c')
OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean debug format

all: $(TARGET)

debug: CFLAGS += -g -O0 -DDEBUG_BUILD -fsanitize=address -fsanitize=undefined
debug: LDFLAGS += -fsanitize=address -fsanitize=undefined
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

format:
	clang-format -i $(SRCS) $(shell find src -name '*.h')
