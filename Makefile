
ninja:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake -GNinja ../ ; fi
	@cd build; ninja install
	cp build/compile_commands.json .

cmake:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake ../ ; fi
	@cd build; make install
	cp build/compile_commands.json .


all: ninja

clean:
	@rm -rf build
