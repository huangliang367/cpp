.PHONY: all clean test

all: build/CMakeCache.txt
	cmake --build build

build/CMakeCache.txt:
	cmake -S . -B build -G Ninja

test: all
	@./build/hello

clean:
	rm -rf build	