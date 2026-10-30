#include <iostream>

void print()
{
	int v1[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	int v[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

	for (auto i = 0; i < 10; i++) {
		std::cout << v1[i] << ' ';
	}

	std::cout << "\n==================================\n";
	for (auto& x : v) {
		++x;
		std::cout << x << ' ';
	}

	std::cout << "\n==================================\n";
	for (auto x : {10, 21, 32, 43, 54, 65}) {
		std::cout << x << ' ';
	}
	std::cout << "\n==================================\n";
}

void ref()
{
	int x = 2;
	int y = 3;
	int& a = x;
	int& b = y;

	a = y;

	std::cout << "x = " << x << '\n';
	std::cout << "y = " << y << '\n';
	std::cout << "a = " << a << '\n';
	std::cout << "b = " << b << '\n';
}

int main()
{
	std::cout << "Hello, World!\n";
	print();
	ref();
}