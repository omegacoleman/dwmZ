#include <iostream>
#include <thread>

#include "fcitxim/fcitxim.hpp"

int main(void) {
  fcitxim::fcitximc_t imc;

  imc.on_update([&imc]() {
    auto res = imc.get_im();
    std::cout << "im changed to " << res << std::endl;
  });
  imc.run();
  for(;;) {
    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
  }
  return 0;
}
