.PHONY: ports kernel all
.DEFAULT_GOAL := default


ARCH:=x86_64
CMAKE_ROOT=$(shell pwd)
BUILD:=build/$(ARCH)/

ports:
	@cd ports && ./build.sh


menuconfig:
	@MENUCONFIG_STYLE="monochrome" python3 tools/menuconfig.py

kernel: .config
	@mkdir -p $(BUILD)
	@cd $(BUILD); cmake -GNinja $(CMAKE_ROOT)
	@cd $(BUILD); ninja libc libstdc++ install
	@#cd $(BUILD); ninja libc
	@#cd $(BUILD); ninja libstdc++
	@#cd $(BUILD); ninja install
	@cp $(BUILD)/compile_commands.json .

default: ports kernel

clean:
	@rm -rf build
