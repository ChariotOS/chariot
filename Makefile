

default:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake -GNinja ../ ; fi
	@cd build; ninja install
	@cp build/compile_commands.json .

clean:
	@rm -rf build
