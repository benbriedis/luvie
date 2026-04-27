LV2_INSTALL_DIR ?= $(HOME)/.lv2

.PHONY: all standalone lv2 install uninstall install-lv2 clean

all: standalone lv2

standalone:
	$(MAKE) -C src

lv2: standalone
	$(MAKE) -C lv2

install: standalone
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

install-lv2: lv2
	mkdir -p $(LV2_INSTALL_DIR)/luvie.lv2
	cp lv2/luvie_dsp.so  $(LV2_INSTALL_DIR)/luvie.lv2/
	cp lv2/luvie_ui.so   $(LV2_INSTALL_DIR)/luvie.lv2/
	cp lv2/manifest.ttl  $(LV2_INSTALL_DIR)/luvie.lv2/
	cp lv2/luvie_dsp.ttl $(LV2_INSTALL_DIR)/luvie.lv2/
	cp lv2/luvie_ui.ttl  $(LV2_INSTALL_DIR)/luvie.lv2/
	@echo "Installed to $(LV2_INSTALL_DIR)/luvie.lv2/"

clean:
	$(MAKE) -C src clean
	$(MAKE) -C lv2 clean
