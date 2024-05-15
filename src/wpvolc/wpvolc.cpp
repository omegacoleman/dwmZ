#include "wpvol/wpvol.hpp"

#include <iostream>
#include <thread>

int main(void) {
  wpvol::wp_vol_t vol;
  vol.on_update([&vol]() {
    auto res = vol.get_result();
    if(res) {
      std::cout << "vol changed to " << res->volume << " " << (res->mute ? "[M]" : "[ ]") << std::endl;
    } else {
      std::cout << "vol becomes invalid" << std::endl;
    }
  });
  vol.run();
  for(;;) {
    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
  }
  return 0;
}
