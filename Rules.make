unexport DIRS
unexport OBJS
unexport DIR-TARGETS
unexport OBJDIR
unexport DEPS
unexport TARGET
unexport C-SRC
unexport ASM-SRC
unexport EXCLUDE-DIRS
unexport EXCLUDE-FILES
unexport RELATIVE-TO-BASE
unexport OWN-SRC-LIST
unexport OWN-DIRS-LIST

.PHONY: $(DIR-TARGETS)

RELATIVE-TO-BASE :=  $(shell pwd | sed 's:^$(TOPDIR)/::')

# Absolute path to the current o-files directory
OBJDIR := $(TOPDIR)/obj/$(RELATIVE-TO-BASE)

ifeq ($(OWN-DIRS-LIST), )

# Subdirectories
DIRS := $(filter-out $(addsuffix /, $(EXCLUDE-DIRS)), $(shell ls -d */ 2>/dev/null))
DIRS := $(DIRS:%/=%)

endif

ifeq ($(OWN-SRC-LIST), )

# Sources listed in directory
C-SRC := $(filter-out $(EXCLUDE-FILES), $(shell ls *.c 2>/dev/null | tr '\n' ' '))
ASM-SRC := $(filter-out $(EXCLUDE-FILES), $(shell ls *.S 2>/dev/null | tr '\n' ' '))

endif

OBJS := $(addprefix $(OBJDIR)/, $(C-SRC:%.c=%.o) $(ASM-SRC:%.S=%.o))
DEPS := $(OBJS:%.o=%.d)
TARGET := $(OBJDIR).o
DIR-TARGETS := $(addsuffix .o, $(DIRS))

# Create directory for o-files
$(shell $(MKDIR) $(OBJDIR))

all: $(TARGET)

$(TARGET): $(DIR-TARGETS) $(OBJS)
	@echo "   LINK `echo $@ | sed 's:^$(TOPDIR)/::'`"
	$(LD) -r $(ARCH-LDFLAGS) -o $(TARGET) $(OBJDIR)/*.o -nostdlib

$(DIR-TARGETS):
	@cd $(@:%.o=%) && $(MAKE)

$(OBJDIR)/%.o: %.c
	@echo "   CC `echo $@ | sed 's:^$(TOPDIR)/::'`"
	$(CC) -c -o $@ $< $(CFLAGS) -MMD

$(OBJDIR)/%.o: %.S
	@echo "   CC `echo $@ | sed 's:^$(TOPDIR)/::'`"
	$(CC) -c -o $@ $< $(CFLAGS) -MMD

-include $(DEPS)
