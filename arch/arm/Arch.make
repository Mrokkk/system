USE-GCC-AS-LD := y

EMULATION := armelf
OUTPUT-FORMAT := elf32-littlearm

arch-cflags-$(CONFIG_VERSATILE) := -mcpu=arm926ej-s  -mfloat-abi=soft -mthumb-interwork
arch-cflags-$(CONFIG_RASPI) 	:= -march=armv6zk -mtune=arm1176jzf-s -mthumb-interwork

qemu-mach-$(CONFIG_VERSATILE)	:= -M versatilepb
qemu-mach-$(CONFIG_RASPI)		:= -M raspi2
QEMU-MACH := $(qemu-mach-y)

ARCH-CFLAGS := $(arch-cflags-y) -fpic
ARCH-LDFLAGS := 

QEMU := $(shell which 2>/dev/null qemu-system-arm) -nographic $(QEMU-MACH) -m 256

