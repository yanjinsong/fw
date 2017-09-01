
ifeq ($(OS),Windows_NT)
CROSS_COMPILER = $(GNU_ARM_ROOT)/bin/arm-none-eabi-
# Export GNU_ARM_ROOT_WINDOWS for Lint to search includes files of the tool chain.
GNU_ARM_ROOT_WINDOWS = $(shell cygpath -m $(GNU_ARM_ROOT))
export GNU_ARM_ROOT_WINDOWS 
ifdef PC_LINT_ROOT
PC_LINT_ROOT_WINDOWS = $(shell cygpath -m $(PC_LINT_ROOT))
export PC_LINT_ROOT_WINDOWS
endif
else
CROSS_COMPILER = /opt/toolchain/gcc-arm-none-eabi-4_6-2012q1/bin/arm-none-eabi-
endif

SHOW_LINT = 1
################################################################
# Define the make variables
################################################################
AS	 = $(CROSS_COMPILER)as
LD	 = $(CROSS_COMPILER)ld
CC	 = $(CROSS_COMPILER)gcc
CPP	 = $(CC) -E
AR	 = $(CROSS_COMPILER)ar
STRIP	 = $(CROSS_COMPILER)strip
OBJDUMP	 = $(CROSS_COMPILER)objdump
OBJCOPY	 = $(CROSS_COMPILER)objcopy
RANLIB	 = $(CROSS_COMPILER)RANLIB
RM	 = rm -rf
CP	 = cp -rf
MKDIR	 = mkdir -p
ifeq ($(OS),Windows_NT)
BIN2MIF = utility/bin2mif.exe
CYGPATH = cygpath -w 
else
BIN2MIF = utility/bin2mif
CYGPATH =  winepath -w  
endif
LOG_CC   = $(TOPDIR)/utility/log-cc
#LINT     = ./utility/lin_cabrio.bat
LINT     = $(TOPDIR)/scripts/lintwine
ifndef LINT_FILE_LIST
LINT_FILE_LIST = all_src
endif
LINT_OPT = -"format=%l %f %t %n: %m" defs CAB  $(LINT_FILE_LIST) 

INCLUDE += -I$(TOPDIR_WIN)
INCLUDE += -I$(TOPDIR_WIN)/include 
INCLUDE += -I$(TOPDIR_WIN)/driver
INCLUDE += -I$(TOPDIR_WIN)/rtos/FreeRTOS/Source/include
INCLUDE += -I$(TOPDIR_WIN)/cli
#INCLUDE += -I$(TOPDIR_WIN)/../host/include
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/supplicant/common
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/supplicant/utils/
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/supplicant/crypto/
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/supplicant/drivers/
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/supplicant/rsn_supp
INCLUDE += -I$(TOPDIR_WIN)/apps/mac80211/supplicant/
INCLUDE += -I$(TOPDIR_WIN)/smartcfg



ARFLAGS	 = crv
DBGFLAGS = -g
OPTFLAGS = -Os -fomit-frame-pointer
GLOBAL_DEF = -D__SSV6200__ 
ifeq ($(OS),Windows_NT)
LDSCRIPT = $(TOPDIR_WIN)/bsp/ldscript.ld
else
LDSCRIPT = $(TOPDIR)/bsp/ldscript.ld
endif
CPPFLAGS = $(INCLUDE) $(OPTFLAGS) $(GLOBAL_DEF)
CFLAGS   = $(CPPFLAGS) -Wall -Werror -Wno-trigraphs -mcpu=arm7 -fno-builtin #-msoft-float

##ifeq ($(OS),Windows_NT)
CFLAGS  += -mno-thumb-interwork
##endif

ifeq ($(shell uname),Linux)
GLOBAL_DEF += -DAIRKISS_ENABLE
#LIBS = $(TOPDIR)/smartcfg/libairkiss_log.a
LIBS = $(TOPDIR)/smartcfg/libairkiss.a
endif

#LDFLAGS	 = -Bstatic -T $(LDSCRIPT) #--whole-archive -Tbss 10 -Tdata 20
LDFLAGS  = -Bstatic -T $(LDSCRIPT) -Xlinker -o$(IMAGE_NAME).elf 
LDFLAGS += -Xlinker -M -Xlinker -Map=$(IMAGE_NAME).map -nostartfiles #-nostdlib
AFLAGS   = $(INCLUDE) #-msoft-float

################################################################
export AS LD CC CPP AR STRIP OBJDUMP RANLIB RM
export ARFLAG LDSCRIPT CPPFLAGS LDFLAGS CFLAGS
export AFLAGS
################################################################

