// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_version.h"
#include <cassert>
#include <chrono>
#include <iostream>
int main() {
  auto path=std::filesystem::temp_directory_path()/
    ("reyond-version-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  assert(ftebroom::readEditorVersion(path)=="0.0.0");
  for(auto version:{"0.5.0-alpha.127","1.2.3"}){
    {std::ofstream file(path);file<<"ReyondBroom "<<version<<"\nPlatform: Windows-x64\n";}
    assert(ftebroom::readEditorVersion(path)==version);
  }
  for(auto line:{"Other 1.2.3","ReyondBroom 1.2.3 extra","ReyondBroom 1.2.3;bad","ReyondBroom abc"}){
    {std::ofstream file(path);file<<line<<'\n';}
    assert(ftebroom::readEditorVersion(path)=="0.0.0");
  }
  std::filesystem::remove(path);
  std::cout<<"PASS packaged release version and invalid/missing metadata fallback\n";
}
