CROSS_COMPILE = /home/hl/toolchains/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
SRCS = perf.cc sdk/perfetto.cc trace_categories.cc
OBJS = $(SRCS:.cc=.o)
TARGET = perf
INCLUDES = -Isdk
CXXFLAGS = -std=c++17 -pthread
LDFLAGS =

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cc
	@echo "Compiling $@ ..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
