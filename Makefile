.PHONY: ports kernel all
.DEFAULT_GOAL := default


ports:
	@cd ports && ./build.sh

kernel:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then cd build; cmake -GNinja ../ ; fi
	@cd build; ninja install
	@cp build/compile_commands.json .

default: ports kernel

clean:
	@rm -rf build
