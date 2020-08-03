
ninja:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake -GNinja ../ ; fi
	@cd build; ninja install

cmake:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake ../ ; fi
	@cd build; make -j install

default: cmake
	@cp build/compile_commands.json .

clean:
	@rm -rf build
