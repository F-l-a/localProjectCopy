# Toolchain Setup
CC      = arm-none-eabi-gcc
QEMU    = qemu-system-arm

# Project Folders
SRC_DIR      = src
FREERTOS_DIR = FreeRTOS-Kernel
BUILD_DIR    = build

# Compilation Flags
# Configured for Cortex-M3 (Stellaris LM3S6965)
# Includes support for Semihosting (printf via QEMU)
CPUFLAGS = -mcpu=cortex-m3 -mthumb
CFLAGS   = $(CPUFLAGS) -g -O0 -Wall \
           -I./$(SRC_DIR) \
           -I./$(FREERTOS_DIR)/include \
           -I./$(FREERTOS_DIR)/ARM_CM3
LDFLAGS  = -T gcc_arm.ld --specs=rdimon.specs -lc -lrdimon

# Source Files
APP_SOURCES = $(SRC_DIR)/main.c \
              $(SRC_DIR)/precise_scheduler.c \
              startup_ARMCM3.c

FREERTOS_SOURCES = $(FREERTOS_DIR)/tasks.c \
                   $(FREERTOS_DIR)/list.c \
                   $(FREERTOS_DIR)/queue.c \
                   $(FREERTOS_DIR)/timers.c \
                   $(FREERTOS_DIR)/MemMang/heap_4.c \
                   $(FREERTOS_DIR)/ARM_CM3/port.c

SOURCES = $(APP_SOURCES) $(FREERTOS_SOURCES)
OBJECTS = $(SOURCES:.c=.o)
TARGET  = qemu_test.elf

# Scenario Selection (Default: STANDARD)
SCENARIO ?= STANDARD

# Build Rules

.PHONY: all clean run standard edge stress

all: $(TARGET)

# Linker
$(TARGET): $(OBJECTS)
	@echo "Linking: $@"
	@$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@

# Compiler (adds the flag to the chosen scenario)
%.o: %.c
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -DTEST_SCENARIO_$(SCENARIO) -c $< -o $@

# Testing Shortcuts

standard:
	@$(MAKE) clean
	@$(MAKE) SCENARIO=STANDARD all

edge:
	@$(MAKE) clean
	@$(MAKE) SCENARIO=EDGE all

stress:
	@$(MAKE) clean
	@$(MAKE) SCENARIO=STRESS all

# Execution

run:
	@echo "Avvio QEMU (Premi CTRL+C per terminare)..."
	@$(QEMU) -machine lm3s6965evb -nographic -semihosting -kernel $(TARGET)

clean:
	@echo "Pulizia file binari..."
	@rm -f $(TARGET) $(OBJECTS)