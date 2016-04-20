EMULATION := elf_i386
OUTPUT-FORMAT := elf32-i386

ifeq ($(HOST), FreeBSD)
OUTPUT-FORMAT := $(addsuffix -freebsd, $(OUTPUT-FORMAT))
EMULATION := $(addsuffix _fbsd, $(EMULATION))
else ifeq ($(HOST), SunOS)
OUTPUT-FORMAT := $(addsuffix -sol2, $(OUTPUT-FORMAT))
EMULATION := $(addsuffix _sol2, $(EMULATION))
LD := $(shell which 2>/dev/null gld)
endif

ARCH-CFLAGS := -m32
ARCH-LDFLAGS := --oformat $(OUTPUT-FORMAT) -m32 -m $(EMULATION) -nostdlib

qemu-cpu-$(CONFIG_M386) 	:= -cpu qemu32
qemu-cpu-$(CONFIG_M486) 	:= -cpu 486
qemu-cpu-$(CONFIG_M586)		:= -cpu pentium
qemu-cpu-$(CONFIG_M686)		:= -cpu pentium2
qemu-cpu-$(CONFIG_COREDUO)	:= -cpu coreduo
qemu-cpu-$(CONFIG_CORE2DUO) := -cpu core2duo
QEMU-CPU := $(qemu-cpu-y)

QEMU := $(shell which 2>/dev/null qemu-system-i386 || which 2>/dev/null qemu-system-x86_64) $(QEMU-CPU) -serial stdio -m 256
