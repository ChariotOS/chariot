

default:
	@mkdir -p build
	@if [ ! -d build/kernel ]; then  cd build; cmake -GNinja ../ ; fi
	@cd build; ninja install
	@if command -v compdb >/dev/null; then compdb -p build/ list > compile_commands.json & fi



# build the completion db
comp:
	@compdb -p build/ list > compile_commands.json

clean:
	@rm -rf build
