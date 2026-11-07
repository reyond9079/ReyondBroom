// SPDX-License-Identifier: GPL-3.0-or-later
#include "files.h"
#include "fs/DiskIO.h"
#include "fs/PathMatcher.h"
#include "fs/PathInfo.h"
#include "fs/TraversalMode.h"
#include "kd/functional.h"
#include "kd/path_utils.h"
#include <stdexcept>
namespace ftebroom::profile {
std::filesystem::path parsePath(std::string text){return kdl::parse_utf8_path(std::move(text));}
namespace {
template<class T>T checked(tb::Result<T> result){T value{};result.transform([&](auto&& v){value=std::move(v);}).transform_error([](const auto& e){throw std::runtime_error(e.msg);});return value;}
void checked(tb::Result<void> result){result.transform_error([](const auto& e){throw std::runtime_error(e.msg);});}
std::filesystem::path absolute(const std::string& text,const std::filesystem::path& work){auto p=parsePath(text);return p.is_absolute()?p:work/p;}
}
FilePlan planFiles(int kind,const std::string& source,const std::string& target,const std::filesystem::path& working) {
 FilePlan p{kind,{},absolute(target,working)};auto from=absolute(source,working);
 if(kind==3){p.sources.push_back(from);return p;}
 auto matcher=kdl::logical_and(tb::fs::makePathInfoPathMatcher({tb::fs::PathInfo::File}),tb::fs::makeFilenamePathMatcher(from.filename().string()));
 p.sources=checked(tb::fs::Disk::find(from.parent_path(),kind==4?tb::fs::TraversalMode::Recursive:tb::fs::TraversalMode::Flat,matcher));
 return p;
}
void executeFiles(const FilePlan& p) {
 if(p.kind==2){checked(tb::fs::Disk::createDirectory(p.target));for(const auto& s:p.sources)checked(tb::fs::Disk::copyFile(s,p.target));}
 else if(p.kind==3){checked(tb::fs::Disk::createDirectory(p.target.parent_path()));checked(tb::fs::Disk::moveFile(p.sources.at(0),p.target));}
 else for(const auto& s:p.sources){
  // TB 2026.2 wraps the deletion fold in transform rather than and_then. A
  // per-file deletion failure ends that fold, but the task still emits end().
  // Preserve the observed reference behavior; directory/traversal errors above
  // still fail the task. See the recorded readonly-folder oracle experiment.
  if(tb::fs::Disk::deleteFile(s).is_error())break;
 }
}
}
