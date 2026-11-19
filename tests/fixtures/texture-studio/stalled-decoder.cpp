// SPDX-License-Identifier: GPL-3.0-or-later
#include <chrono>
#include <thread>

int main() {
  std::this_thread::sleep_for(std::chrono::seconds(90));
  return 1;
}
