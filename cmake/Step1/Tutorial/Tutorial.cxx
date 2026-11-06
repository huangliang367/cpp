// A simple program that computes the square root of a number
#include <cmath>
#include <iostream>
#include <string>

#include <MathFunctions.h>

// TODO8: Include the MathFunctions header

int main(int argc, char* argv[])
{
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " number" << std::endl;
    return 1;
  }

  // convert input to double
  double const inputValue = std::stod(argv[1]);

  // TODO9: Use the mathfunctions::sqrt function
  // calculate square root
  double const outputValue = mathfunctions::sqrt(inputValue);
  std::cout << "The square root of " << inputValue << " is " << outputValue
            << std::endl;
}
