#pragma once

#include <stdexcept>

template <typename T>
class Vector {
public:
	explicit Vector(int s) : elem(nullptr), sz(s)
	{
		if (s < 0)
			throw std::length_error("Vector constructor: negative size");
		elem = new T[s];
	}

	~Vector() { delete[] elem; }

	T& operator[](int i)
	{
		if (i < 0 || size() <= i)
			throw std::out_of_range("Vector::operator[]");
		return elem[i];
	}

	const T& operator[](int i) const
	{
		if (i < 0 || size() <= i)
			throw std::out_of_range("Vector::operator[]");
		return elem[i];
	}

	T* begin() { return elem; }
	const T* begin() const { return elem; }
	T* end() { return elem + sz; }
	const T* end() const { return elem + sz; }
	int size() const { return sz; }

private:
	T* elem;
	int sz;
};
