#include <iostream>
#include "hello.h"

int main()
{
#ifdef HELLO_TEST
	say_hello();
#else
	std::cout << "HELLO_TEST is not defined" << std::endl;
#endif
	return 0;
}