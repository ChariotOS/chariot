.PHONY: ports kernel all
.DEFAULT_GOAL := default


CMAKE_ROOT=$(shell pwd)
BUILD:=build/

ports:
	@cd ports && ./build.sh


cfg: menuconfig
menuconfig:
	@python3 tools/menuconfig.py

$(BUILD)/Makefile:
	@mkdir -p $(BUILD)
	@cd $(BUILD); cmake ../

$(BUILD)/build.ninja:
	@mkdir -p $(BUILD)
	@cd $(BUILD); cmake -GNinja ../

kernel: .config $(BUILD)/Makefile
	@tools/build_toolchain.sh
	@# ninja -C build 
	@$(MAKE) -j $(NPROC) -C build install --no-print-directory
	@#ninja -C build install
	@#cd $(BUILD); cmake --install . &2>1 >/dev/null
	@cp $(BUILD)/compile_commands.json .

default: kernel

run:
	@tools/run.sh -s -nographic

clean:
	@rm -rf build
