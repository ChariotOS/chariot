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
	@tools/build_toolchain.sh
	@mkdir -p $(BUILD)
	@cd $(BUILD); cmake -GNinja $(CMAKE_ROOT)
	@cd $(BUILD); ninja install | c++filt
	@cp $(BUILD)/compile_commands.json .

default: kernel

run:
	@tools/run.sh

clean:
	@rm -rf build
