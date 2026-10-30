#ifndef __VECTAOR_H__
#define __VECTAOR_H__

class Vector {
public:
	Vector(int s) : elem{new double[s]}, sz{s} {}
	double& operator[](int i) {return elem[i];}
	int size() {return sz;}
private:
	double* elem;
	int sz;
};

enum class Color {red, blue, green};
enum class Traffic_light {green, yellow, red};

#endif /* __VECTAOR_H__ */