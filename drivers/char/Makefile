OWN-SRC-LIST := y

src-y					:= dummy.c
src-$(CONFIG_TTY) 		+= vga.c tty.c keyboard.c
src-$(CONFIG_SERIAL) 	+= serial.c

C-SRC := $(src-y)

include $(TOPDIR)/Rules.make
