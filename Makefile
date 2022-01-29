CC = gcc
LD = ld


SRCDIR := c-src
OBJDIR := objects
MD4CDIR := submodules/md4c/src

CFLAGS = -I./$(MD4CDIR) -std=c99 -Wall -Werror -Wextra -Wpedantic
MD4C_FLAGS = -Wno-overflow

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

CUSTOM_C_FILES = $(call rwildcard,$(SRCDIR),*.c)
MD4C_C_FILES = $(call rwildcard,$(MD4CDIR),*.c)

CFILES = $(CUSTOM_C_FILES) $(MD4C_C_FILES)

CUSTOM_OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(CUSTOM_C_FILES))
MD4C_OBJS = $(patsubst $(MD4CDIR)/%.c, $(OBJDIR)/md4c/%.o, $(MD4C_C_FILES))

OBJS = $(CUSTOM_OBJS) $(MD4C_OBJS)

DIRS = $(wildcard $(SRCDIR)/*)

all: $(CUSTOM_OBJS) $(MD4C_OBJS) link

setup:
	@ mkdir objects
	@ mkdir objects/md4c


$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@ echo [CC] $^
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJDIR)/md4c/%.o: $(MD4CDIR)/%.c
	@ echo [MD4C-CC] $^
	$(CC) $(CFLAGS) -c $^ -o $@ $(MD4C_FLAGS)

link:
	@echo [LD]
	$(CC) $(OBJS) -o gen-blog
