// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace ftebroom::browser {
inline std::string_view typedPath(std::string_view text) {
  const auto first=text.find_first_not_of(" \t\r\n");
  if(first==text.npos)return {};
  text=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
  if(text.size()>=2&&text.front()=='"'&&text.back()=='"')text=text.substr(1,text.size()-2);
  return text;
}
struct Segment { std::string label; std::filesystem::path path; };
inline std::vector<Segment> ancestors(const std::filesystem::path& folder) {
  std::vector<Segment> result;auto path=folder.root_path();
  if(!path.empty())result.push_back({path.u8string(),path});
  for(const auto& part:folder.relative_path()){path/=part;auto label=part.u8string();if(label.empty())continue;
    if(!result.empty()&&path==result.back().path)continue;
    result.push_back({label,path});}
  return result;
}
inline int zoom(int size,float wheel,int low,int high) {
  if(!std::isfinite(wheel))return size;
  return std::clamp(size+int(std::round(std::clamp(wheel,-8.f,8.f)*8)),low,high);
}
}
