MAKE ?= make
MV ?= mv
RM ?= rm
MKDIR ?= mkdir
CD ?= cd
CURRENT_PATH := $(shell pwd)

.PHONY: all update_submodule libdivsufsort ddelta clean

all: update_submodule libdivsufsort ddelta

update_submodule:
	@echo "submodule update..."
	@git submodule update --init --recursive

libdivsufsort:
	@echo "build libdivsufsort..."
	@$(MKDIR) -p libdivsufsort/build
	
	@$(CD) libdivsufsort && cmake -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX=$(CURRENT_PATH)
	@$(MAKE) -C libdivsufsort
	@$(MAKE) -C libdivsufsort install

ddelta:
	@echo "gen ddelta tools..."
	@$(MAKE) CFLAGS="-I $(CURRENT_PATH)/include -L $(CURRENT_PATH)/lib" -C esp_gen_patch
	$(MV) esp_gen_patch/ddelta_apply $(CURRENT_PATH)/
	$(MV) esp_gen_patch/ddelta_generate $(CURRENT_PATH)/

clean:
	$(RM) -rf ddelta_apply ddelta_generate
	@$(MAKE) -C libdivsufsort clean
	@$(MAKE) -C libdivsufsort uninstall