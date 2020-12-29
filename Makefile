.PHONY: ports kernel all
.DEFAULT_GOAL := default


CMAKE_ROOT=$(shell pwd)
BUILD:=build/

ports:
	@cd ports && ./build.sh


cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py

kernel: .config
	@mkdir -p $(BUILD)
	@cd $(BUILD); cmake -GNinja $(CMAKE_ROOT)
	@cd $(BUILD); ninja install
	@#cd $(BUILD); ninja libc libstdc++ install
	@#cd $(BUILD); ninja libc
	@#cd $(BUILD); ninja libstdc++
	@#cd $(BUILD); ninja install
	@cp $(BUILD)/compile_commands.json .

default: kernel

clean:
	@rm -rf build
