VERSION-MAJ := 0
VERSION-MIN := 0
NAME := kernel

KERNEL-RELEASE := $(VERSION-MAJ).$(VERSION-MIN)

.SILENT:

ifeq ($(MAKECMDGOALS), )
MAKECMDGOALS := all
endif

$(info Makefile script for $(NAME) $(KERNEL-RELEASE))
$(info Target(s): $(MAKECMDGOALS))

CONFIG := config

ifneq ($(MAKECMDGOALS), $(filter $(MAKECMDGOALS), clean clean-all help dist tar cloc iso configure qemu qemu-iso qemu-kernel))
ifneq ($(CONFIG), $(wildcard $(CONFIG)))
$(error No configuration file. Run '$(MAKE) configure' first)
endif
endif

HOST = $(shell uname -s)

-include $(CONFIG)

-include arch/Arch.make

-include arch/$(ARCH)/Arch.make

# Tools definitions
CC := $(shell which 2>/dev/null $(CROSS-COMPILE)gcc)
CPP := $(shell which 2>/dev/null $(CROSS-COMPILE)g++)
LD := $(shell which 2>/dev/null $(CROSS-COMPILE)ld)
AS := $(shell which 2>/dev/null $(CROSS-COMPILE)as)
AR := $(shell which 2>/dev/null $(CROSS-COMPILE)ar)
SIZE := $(shell which 2>/dev/null $(CROSS-COMPILE)size)
READELF := $(shell which 2>/dev/null $(CROSS-COMPILE)readelf)
OBJDUMP := $(shell which 2>/dev/null $(CROSS-COMPILE)objdump)
RM := rm -f
RMDIR := rm -rf
MKDIR := mkdir -p
MKISOFS := $(shell which 2>/dev/null mkisofs || which 2>/dev/null genisoimage)
TAR := tar

STAGE2-MIRROR = ftp://ftp.free.org/mirrors/rsync.frugalware.org/frugalware-testing/boot/grub/stage2_eltorito
# STAGE2-MIRROR = ftp://ftp.free.org/mirrors/rsync.frugalware.org/frugalware-current/boot/grub/stage2_eltorito
# STAGE2-MIRROR = ftp://ftp.fsn.hu/pub/linux/distributions/frugalware/frugalware-testing/boot/grub/stage2_eltorito

# Directories
TOPDIR := $(shell pwd)
OBJDIR := $(TOPDIR)/obj
BINDIR := $(TOPDIR)/bin
INCDIR := $(TOPDIR)/include

# Generated header files
VERSION-H := include/$(NAME)/version.h
COMPILE-H := include/$(NAME)/compile.h
CONFIG-H := include/$(NAME)/config.h

KERNEL-BIN := $(BINDIR)/kernel

# Subdirectories to be build
DIRS := arch fs kernel lib drivers
ifeq ($(CONFIG_TESTS), y)
DIRS += tests
endif

LINKER-SCRIPT := $(TOPDIR)/arch/$(ARCH)/linker.ld

C-STD := gnu90
DEFINES := -D__KERNEL__ -DARCH=$(ARCH)
INCLUDES := -I $(INCDIR) -I $(TOPDIR)/arch/$(ARCH)/include 

# C compiler flags
CFLAGS := -std=$(C-STD) -ffreestanding -fno-builtin -Wall -Wextra -Werror
CFLAGS += -nostdinc -g3 $(DEFINES) -fno-asynchronous-unwind-tables
CFLAGS += -O2 -fomit-frame-pointer -fno-strength-reduce
CFLAGS += $(ARCH-CFLAGS) $(INCLUDES)

# Linker settings
ifeq ($(USE-GCC-AS-LD), y)
LD := $(CC)
LDFLAGS := -Wl,-T$(LINKER-SCRIPT) -Wl,-Map=$(BINDIR)/kernel.map
else
LDFLAGS := -T$(LINKER-SCRIPT) -Map=$(BINDIR)/kernel.map
endif

LDFLAGS += $(ARCH-LDFLAGS) -nostartfiles

ifneq ($(MAKECMDGOALS), $(filter $(MAKECMDGOALS), clean clean-all help dist tar cloc iso configure qemu qemu-iso qemu-kernel))

ifneq ($(findstring $(OUTPUT-TARGET), $(shell $(LD) --help | grep "supported targets")), $(OUTPUT-TARGET))
$(error Your GCC does not support $(OUTPUT-TARGET) target. Use proper cross-compiler)
endif

ifneq ($(OUTPUT-TARGET), $(shell $(LD) --print-output-format))

ifneq ($(findstring $(EMULATION), $(shell $(LD) -V)), $(EMULATION))
$(error Your GCC does not support $(EMULATION) emulation. Use proper cross-compiler)
endif

endif

$(info Host system: $(HOST))
$(info Host machine: $(shell uname -m))
$(info Target architecture: $(ARCH))
$(info Target format: $(OUTPUT-FORMAT))
ifneq ($(EMULATION), )
$(info Target emulation: $(EMULATION))
endif
$(info GCC version: $(shell $(CC) -dumpversion))

endif

ifneq ($(filter qemu-iso qemu, $(MAKECMDGOALS)), )
ifeq ($(QEMU), )
$(error Not found QEMU [qemu-system-i386 or qemu-system-x86_64]. If you have QEMU, specify path to it by adding QEMU=path_to_qemu)
endif
endif

ifneq ($(filter qemu-iso iso, $(MAKECMDGOALS)), )
ifeq ($(MKISOFS), )
$(error Not found mkisofs. If you have mkisofs or genisoimage, specify its path by adding MKISOFS=path_to_mkisofs)
endif
endif

ifneq ($(filter tar dist, $(MAKECMDGOALS)), )
ifeq ($(TAR), )
$(error Not found tar)
endif
endif

export ARCH-CFLAGS ARCH-LDFLAGS

.EXPORT_ALL_VARIABLES:

unexport DIRS

.PHONY: all clean $(DIRS) configure

all: announce create-dirs build-system
	@echo
	@echo "Size: "
	@$(SIZE) $(KERNEL-BIN)
	@nm -S $(KERNEL-BIN) | awk '{print $$4,$$1,$$2,$$3}' > $(BINDIR)/kernel.sym
	@echo

announce:
	@echo
	@echo "Building kernel..."

create-dirs:
	@$(MKDIR) $(OBJDIR)
	@$(MKDIR) $(BINDIR)

$(VERSION-H): 
	@echo "  Creating version.h header..."
	@echo \#define VERSION_MAJ $(VERSION-MAJ) >> $(VERSION-H)
	@echo \#define VERSION_MIN $(VERSION-MIN) >> $(VERSION-H)

$(COMPILE-H):
	@echo "  Creating compile.h header...";
	echo \#define COMPILE_TIME \"`date +%X`\" >> $(COMPILE-H)
	echo \#define COMPILE_DATE \"`date +%x`\" >> $(COMPILE-H)
	echo \#define COMPILE_SYSTEM \"`uname -s`\" >> $(COMPILE-H)
	echo \#define COMPILE_ARCH \"`uname -m`\" >> $(COMPILE-H)
	echo \#define COMPILE_BY \"`whoami`\" >> $(COMPILE-H)
	echo \#define COMPILE_HOST \"`hostname`\" >> $(COMPILE-H)
	echo \#define COMPILER \"`$(CC) -v 2>&1 | tail -1`\" >> $(COMPILE-H)

build-system: $(DIRS)
	@echo "  LINK $(KERNEL-BIN)"
	$(LD) $(LDFLAGS) -o $(KERNEL-BIN) $(OBJDIR)/*.o

$(DIRS): arch-check $(VERSION-H) $(COMPILE-H)
	@echo "  Building $@..."
	@$(MKDIR) $(OBJDIR)/$@
	@cd $@ && $(MAKE)

configure:
	bash scripts/configure.sh

clean:
	-$(RMDIR) $(OBJDIR)
	-$(RMDIR) $(BINDIR)
	-$(RM) $(VERSION-H)
	-$(RM) $(COMPILE-H)
	-$(RMDIR) iso
	-$(RM) *.iso
	-$(RM) *.gz

clean-all: clean
	-$(RM) $(CONFIG-H)
	-$(RM) $(CONFIG)

tar dist:
	@echo "Creating tar.gz archive..."
	@$(TAR) -zcf ../system.tar.gz ../system
	@echo "Archive: ../system.tar.gz"

arch-check:
	@if [ -z $(ARCH) ]; then \
		echo error: Architecture is not set; \
		exit 1; \
	fi

newversion:
	@if [ ! -f .version ]; then \
		echo 1 > .version; \
	else \
		expr 0`cat .version` + 1 > .version; \
	fi

cloc:
	@echo "Source:"
	@echo -n "  files: "
	find -name "*.[c]" | wc -l
	@echo -n "  lines: "
	find -name "*.[c]" | xargs wc -l | tail -1 | grep -o "[0-9]*"
	@echo "Headers:"
	@echo -n "  files: "
	find -name "*.[h]" | wc -l
	@echo -n "  lines: "
	find -name "*.[h]" | xargs wc -l | tail -1 | grep -o "[0-9]*"
	@echo "Assembly:"
	@echo -n "  files: "
	find -name "*.[S]" | wc -l
	@echo -n "  lines: "
	find -name "*.[S]" | xargs wc -l | tail -1 | grep -o "[0-9]*"

iso: iso/boot/grub iso/boot/grub/stage2_eltorito iso/boot/grub/menu.lst
	@echo Creating ISO image with $(MKISOFS)...
	@cp $(KERNEL-BIN) iso/
	@cp bin/kernel.sym iso/symbols
	@$(MKISOFS) -R -b boot/grub/stage2_eltorito -no-emul-boot -boot-load-size 4 -boot-info-table -o os.iso iso 2> /dev/null
	@echo ISO created to os.iso

iso/boot/grub:
	$(MKDIR) iso/boot/grub

iso/boot/grub/stage2_eltorito:
	@if test -n "$$STAGE2"; then \
		echo "Got stage2_eltorito in $(STAGE2)"; \
		cp $(STAGE2) iso/boot/grub/stage2_eltorito; \
	else \
		echo Downloading bootloader stage2 from $(STAGE2-MIRROR)...; \
		wget -q $(STAGE2-MIRROR); \
		mv stage2_eltorito iso/boot/grub/stage2_eltorito; \
	fi

iso/boot/grub/menu.lst:
	@echo Creating menu.lst
	@echo default 0 >> iso/boot/grub/menu.lst
	@echo timeout 0 >> iso/boot/grub/menu.lst
	@echo title My kernel >> iso/boot/grub/menu.lst
	@echo kernel /kernel >> iso/boot/grub/menu.lst
	@echo module /symbols >>  iso/boot/grub/menu.lst
	@echo boot >> iso/boot/grub/menu.lst

qemu-iso: iso
	$(QEMU) -cdrom os.iso $(QEMU-FLAGS)

qemu qemu-kernel:
	$(QEMU) -kernel $(KERNEL-BIN) $(QEMU-FLAGS)

help:
	@echo "Configuring:"
	@echo "  configure - configure build - it must be run first"
	@echo "Build targets:"
	@echo "  all - build whole kernel"
	@echo "  dir - build only 'dir' directory"
	@echo "Creating archives:"
	@echo "  tar, dist - create *.tar.gz archive"
	@echo "Cleaning targets:"
	@echo "  clean - clean all objs and bins"
	@echo "Tools:"
	@echo "  cloc - count source code lines"
	@echo "  iso - create bootable ISO image"
	@echo "  qemu, qemu-kernel - run QEMU directly loading kernel"
	@echo "  qemu-iso - create ISO and run QEMU"
	@echo "    To pass additional options to QEMU, set QEMU-FLAGS to them"
	@echo "Architectures supported:"
	@echo "  x86 [i386 i486 i586 i686]"
	@echo "Cross-compiling:"
	@echo "  To compile kernel using cross-compiler, type 'make CROSS-COMPILE=x',"
	@echo "  where 'x' should be selected toolchain prefix (eg. i686-elf-)."
	@echo "Supported compilers:"
	@echo "  gcc (tested: 4.8.2, 4.9.3, 5.3.0, 6.0.0, 6.1)"
	@echo "System can be compiled on the following systems with GNU tools"
	@echo "and GCC compiler:"
	@echo "  Linux (tested: Debian, Slackware, Arch)"
	@echo "  FreeBSD (tested: v.10.2)"
	@echo "  Solaris (tested: v.11.3)"
	@echo "System can be run on the following VMs:"
	@echo "  QEMU (tested on: 2.1.2, 2.4.1, 2.5.1, 2.5.92)"
	@echo "  Bochs (tested on: 2.6.8)"
	@echo "  VirtualBox (tested on: 3.0.14)"
	@echo "  VirtualPC"
	@echo "  VMware Workstation 12"

