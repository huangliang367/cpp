#include <iostream>
#include <list>
#include <string>
#include "Vector.h"

int main()
{
	Vector<char> vc(200);
	Vector<std::string> vs(17);
	Vector<std::list<int>> list(45);

	for (auto s : vs)
		std::cout << s << std::endl;

	std::cout << "Vector size: " << vc.size() << std::endl;
	return 0;
}