.PHONY: ports kernel all
.DEFAULT_GOAL := default


CMAKE_ROOT=$(shell pwd)
BUILD:=build/

ports:
	@cd ports && ./build.sh


cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py

$(BUILD)/build.ninja:
	@mkdir -p $(BUILD)
	@cd $(BUILD); cmake -GNinja ../

kernel: .config $(BUILD)/build.ninja
	@tools/build_toolchain.sh
	@cd $(BUILD); ninja install
	@cp $(BUILD)/compile_commands.json .

default: kernel

run:
	@tools/run.sh

clean:
	@rm -rf build
