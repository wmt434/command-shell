#include <cstdlib>
#include <iostream>
#include <string>

int main() {
  std::string s;
  std::cin >> s;
  if (getenv(s.c_str()) != NULL) {
    std::cout << getenv(s.c_str()) << std::endl;
  }
  return 0;
}
