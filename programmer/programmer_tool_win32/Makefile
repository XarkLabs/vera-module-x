# MSYS2 mingw32 makefile for Vera programmer_tool_win32

PREFIX      = i686-w64-mingw32
CC          = $(PREFIX)-gcc
STRIP       = strip
WINDRES     = windres

INC_DIRS    += .

CFLAGS      += -std=c11 -Os -g -Wall -Wextra -Wshadow -Winit-self -Wfloat-conversion -Wdouble-promotion -Wmissing-include-dirs
LFLAGS      = -L. ftd2xx.lib
OUT         = $(OBJ_DIR)/programmer_tool_win32.exe
OBJ_DIR     ?= obj

CFLAGS      += -MD $(addprefix -I,$(INC_DIRS))

#-----------------------------------------------------------------------------
# sources
#-----------------------------------------------------------------------------
C_SRCS      += $(wildcard *.c)

#-----------------------------------------------------------------------------
# object files
#-----------------------------------------------------------------------------
C_OBJS      := $(addprefix $(OBJ_DIR)/, $(C_SRCS:.c=.o))
OBJS        := $(C_OBJS)
RES			:= $(OBJ_DIR)/icon.res
DEPS        := $(OBJS:.o=.d)

#-----------------------------------------------------------------------------
# rules
#-----------------------------------------------------------------------------

.PHONY: all clean run debug

all: $(OUT)

$(OUT): $(OBJ_DIR) $(OBJS) $(RES)
	@echo Linking $@
	$(CC) $(OBJS) $(RES) $(CFLAGS) $(LFLAGS) -o $@
	$(STRIP) -S $@

$(C_OBJS): $(OBJ_DIR)/%.o: %.c
	@echo Compiling $<
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJ_DIR):
	@mkdir -p $(sort $(dir $(C_OBJS)))

$(RES): programmer_tool.ico icon.rc
	$(WINDRES) icon.rc -O coff -F pe-i386 -o $@

run: $(OUT)
	$(OUT) $(RUN_PARAMS)

debug: $(OUT)
	gdb -ex "b main" -ex "run" -q --tui --args $(OUT) $(RUN_PARAMS)

clean:
	@echo Cleaning...
	rm -f $(OUT)
	rm -rf $(OBJ_DIR)

.DEFAULT_GOAL = all

-include $(DEPS)
