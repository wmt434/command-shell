#include <iostream>
#include <string>

int main(int argc, char * argv[]) {
  for (int i = 1; i < argc; i++) {
    std::cout << "This is #" << argc << " argv " << argv[i] << std::endl;
  }
  std::string s;
  std::cin >> s;
  std::cout << "Here prints the string read from stdin " << s << std::endl;
  std::cerr << "Here is the error message of ./both" << std::endl;
  return 0;
}
