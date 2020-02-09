#include <iostream>
#include <string>

int main() {
  std::string s;
  std::cin >> s;
  std::cout << "The stdout should be " << s << std::endl;
  std::cerr << "This is the error message" << std::endl;
}
