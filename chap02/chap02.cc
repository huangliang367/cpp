#include <iostream>
#include "Vector.h"

Traffic_light& operator++(Traffic_light& t)
{
	switch (t) {
	case Traffic_light::green: return t = Traffic_light::yellow;
	case Traffic_light::yellow: return t = Traffic_light::red;
	case Traffic_light::red: return t = Traffic_light::green;
	}

	return t;
}

int main()
{

	auto signal = Traffic_light::red;
	std::cout << "signal: " << static_cast<int>(signal) << std::endl;

	Traffic_light next = ++signal;

	std::cout << "signal: " << static_cast<int>(signal) << std::endl;
	std::cout << "next: " << static_cast<int>(next) << std::endl;

	return 0;	
}