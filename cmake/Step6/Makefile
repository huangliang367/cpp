.PHONY: all clean

export CMAKE_EXPORT_COMPILE_COMMANDS=ON
CMAKE_FILE = build/CMakeCache.txt
ROOT_PATH = $(shell pwd)

all: $(CMAKE_FILE)
	cmake --build build
	cp build/compile_commands.json $(ROOT_PATH)

$(CMAKE_FILE):
	cmake --preset tutorial

clean:
	rm -rf build
	rm -rf compile_commands.json

run: all
	./build/Tutorial/Tutorial 20
