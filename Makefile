include ./config.mk

##############################################################
# Global Variables
##############################################################
TOPDIR          := $(CURDIR)

export TOPDIR

ifeq ($(OS),Windows_NT)
TOPDIR_WIN := $(shell cygpath -m $(TOPDIR))
else
TOPDIR_WIN := $(TOPDIR)
endif

export TOPDIR_WIN

##############################################################
# Local Variables
##############################################################
IMAGE_DIR       := image
DOT_CONFIG      := .config
CONFIG_DIR      := defconfig
CONFIG_TARGET   := defconfig
MAKE_LOG        := make.log
LINT_DEF_FILE   := $(TOPDIR)/defs.lnt
LINT_SRC_FILE   := $(TOPDIR)/all_src.lnt
LINT_OUT        := lint_out.txt
STRIP_SH        := $(TOPDIR)/scripts/strip_dmsg.sh
BIN2H_SH        := bin2h.sh
VERSION_SH      := $(TOPDIR)/ver_info.sh

export LINT_SRC_FILE

conf_exist      := $(shell [ ! -f "$(DOT_CONFIG)" ] || echo "y" )

ifeq ($(TARGET),)
ifeq ($(conf_exist),y)
TARGET          := $(shell cat $(DOT_CONFIG) | \
                     grep -e '^CONFIG_TARGET[[:blank:]]*=[[:blank:]]*' | \
                     sed 's/=/ /g' | awk '{ print $$2 }')
HCI_TYPE	:= $(shell cat $(DOT_CONFIG) | \
		     grep -e '^CONFIG_HCI[[:blank:]]*=[[:blank:]]*' | \
		     sed 's/=/ /g' | awk '{ print $$2 }')
IMAGE_NAME	:= $(addprefix $(IMAGE_DIR)/$(TARGET)-, $(HCI_TYPE))
endif
endif


CONFIG_FILES    := $(addprefix $(CONFIG_DIR)/, \
                   $(subst $(CONFIG_TARGET),,$(shell ls -1 $(TOPDIR)/$(CONFIG_DIR))))
TARGET_LIST     := $(shell cat $(CONFIG_FILES) | \
                     grep -e '^CONFIG_TARGET[[:blank:]]*=[[:blank:]]*' | \
                     sed 's/=/ /g' | awk '{ print $$2 }')


##############################################################
# Source directories to Make
##############################################################
SRC_DIRS        := bsp
SRC_DIRS        += apps
SRC_DIRS        += rtos
SRC_DIRS        += driver
SRC_DIRS        += lib
SRC_DIRS	      += cli



.PHONY: all config target_list target_check

all: target_check $(if $(TARGET), $(sort $(SRC_DIRS)), )
	@if [ "$(TARGET)" = "" ] ; then \
	    echo "No Target is specified !!"; \
	    exit 0; \
	fi
	@echo $(OS)
	@echo "Try linking ...\n" $(AIRKISSLIBS) $(shell cat .build)
	@$(CC) $(CFLAGS) $(LDFLAGS) $(shell cat .build) $(LIBS)
	@$(OBJCOPY) -O binary $(IMAGE_NAME).elf $(IMAGE_NAME).bin
	@$(OBJDUMP) -D -S $(IMAGE_NAME).elf > $(IMAGE_NAME).asm
	@${BIN2MIF} $(IMAGE_NAME).bin $(IMAGE_NAME).mif 32
	@cp $(BIN2H_SH) $(IMAGE_DIR)
	@echo "Done.\n"
	@cd $(IMAGE_DIR) && ./$(BIN2H_SH) $(TARGET)_uart_bin.h	
	
scratch: .config
	@$(RM) $(MAKE_LOG)
	@echo "Cleaning ..."
	@$(MAKE) clean
	@echo "Building ..."
	@echo "CFLAGS = $(CFLAGS)"
	@echo $(INCLUDE) $(GLOBAL_DEF) > defs.lnt
	@$(MAKE) all 2>&1 | $(LOG_CC) $(MAKE_LOG)
	@$(MAKE) install


.config: config

config:
	@if [ "$(TARGET)" != "" ] ; then \
	    for f in $(CONFIG_FILES) ; do \
		if [ "$$f" = "$(CONFIG_DIR)/$(TARGET)" ] ; then \
		    echo "copy $$f to $(DOT_CONFIG) ..."; \
		    $(CP) $$f $(DOT_CONFIG); \
		    $(CP) $$f defconfig/defconfig; \
		fi; \
	    done ; \
	elif [ -f $(CONFIG_DIR)/$(CONFIG_TARGET) ] ; then \
	    $(CP) $(CONFIG_DIR)/$(CONFIG_TARGET) $(DOT_CONFIG); \
	    echo "Copy $(CONFIG_DIR)/$(CONFIG_TARGET) to $(DOT_CONFIG) ..."; \
	else \
	    echo "Usage: make config TARGET=<TARGET_NAME)"; \
	    echo "       OR"; \
	    echo "       copy your target config file to $(CONFIG_DIR)/$(CONFIG_TARGET)"; \
	fi


target_list:
	@$(TOPDIR)/scripts/show-targets.sh $(TOPDIR)/defconfig/

target_check:
	@if [ "$(TARGET)" = "" ]; then \
	    echo "Please 'make config' first!!"; \
	    exit 0; \
	fi
	@$(RM) -rf .build
	@echo "make target '$(TARGET)'"
	@echo "Update FW version.\n"
	@$(VERSION_SH)
	@$(CC) $(CFLAGS) -c -o version.o version.c
	@echo -n "./version.o " >> $(TOPDIR)/.build 
	@wait

mrproper: clean
	@$(RM) .config
	@$(RM) $(MAKE_LOG) 

install:
	@if [ "$(TARGET_DIR)" != "" -a -d "$(TARGET_DIR)" ]; then \
	    echo "Copy $(IMAGE_NAME).bin to $(TARGET_DIR)"; \
	    cp $(IMAGE_NAME).bin $(TARGET_DIR); \
	fi

lint:
	@$(RM) $(LINT_OUT) $(LINT_DEF_FILE)
	
	
	@echo $(GLOBAL_DEF) >  $(LINT_DEF_FILE)
	@for inc in $(subst -I,,$(INCLUDE)); do \
	    echo -n '-I' >> $(LINT_DEF_FILE); \
	    $(CYGPATH) $$inc >> $(LINT_DEF_FILE); \
	done


#ifdef SHOW_LINT
#	$(LINT) $(LINT_OPT)
#else
#	$(LINT) $(LINT_OPT) -os"($(LINT_OUT))" 
#endif

	$(LINT) $(LINT_FILE_LIST) -os"($(LINT_OUT))" > 1.txt
##############################################################
# Make rules
##############################################################
include ./rules.mk

