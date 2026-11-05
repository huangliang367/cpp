
CXX = g++
CXXFLAGS = -std=c++20 -fmodules-ts
BMI = gcm.cache/Vector.gcm

.PHONY: all clean

all: chap07

chap07: chap07.o Vector.o
	$(CXX) $(CXXFLAGS) chap07.o Vector.o -o chap07

chap07.o: chap07.cc $(BMI)
	$(CXX) $(CXXFLAGS) -c chap07.cc

Vector.o $(BMI): Vector.cc
	$(CXX) $(CXXFLAGS) -c Vector.cc

clean:
	rm -f chap07 *.o
	rm -rf gcm.cache
