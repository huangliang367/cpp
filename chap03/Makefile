
CXX = g++
CXXFLAGS = -std=c++20 -fmodules-ts
BMI = gcm.cache/Vector.gcm

.PHONY: all clean

all: chap03

chap03: chap03.o Vector.o
	$(CXX) $(CXXFLAGS) chap03.o Vector.o -o chap03

chap03.o: chap03.cc $(BMI)
	$(CXX) $(CXXFLAGS) -c chap03.cc

Vector.o $(BMI): Vector.cc
	$(CXX) $(CXXFLAGS) -c Vector.cc

clean:
	rm -f chap03 *.o
	rm -rf gcm.cache
