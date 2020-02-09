#include <unistd.h>

#include <cstdlib>
#include <iostream>

int main() {
  std::cout << "I'm waiting...";
  sleep(300);
  std::cout << "I'm done!";
  return 0;
}
