// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/world_architect/legacy.h"
#include "../plugins/autolevel/al_brush.cpp"
#include "../plugins/autolevel/al_storage.cpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace ftebroom::worldArchitect;
int main(int argc,char** argv)try{
  if(argc!=2)throw std::runtime_error("output-directory");std::filesystem::path out=argv[1];std::filesystem::create_directories(out);int count=0;
  auto check=[&](bool value,const char* text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';count++;};
  al::Repo repo;repo.uid="authored";repo.name="Author's original library";al::BlockAsset block;block.uid="author-block";block.name="Original brush module";block.geometry=al::writeMap({{al::box({0,0,0},{96,64,32},"legacy/original")},{{{{"classname","light"},{"origin","48 32 64"},{"light","240"}}}}});repo.blocks.push_back(block);repo.materialSlots["al_wall"]="legacy/original";
  auto original=al::serialize(repo);original+="\r\n";auto migrated=migrateLegacy(original,"../../authored repo.txt");
  check(migrated.report["status"]=="partial"&&migrated.files.at("backup/original.txt")==original,"repository migration retains the exact source bytes, including line endings");
  check(migrated.files.at("modules/block-1.map")==block.geometry,"authored module MAP and point-entity text are extracted without reserialization");
  check(migrated.report["convertedModules"][0]["brushes"]==1&&migrated.report["convertedModules"][0]["pointEntities"]==1,"small extracted modules pass actual convex brush parsing");
  check(migrated.report["convertedModules"][0]["materialDependencies"]==Json::array({"legacy/original"}),"migration records original material dependencies instead of inventing replacement assets");
  for(const auto& [path,bytes]:migrated.files)check(!std::filesystem::path(path).is_absolute()&&path.find("..")==path.npos,"migration output paths do not derive from untrusted source names");
  al::Builder builder;builder.uid="authored-builder";builder.repoUid=repo.uid;builder.seed=23;builder.lightScale=1.75;builder.moduleWeightMultipliers[block.uid]=2.5;al::resize(builder,{1,2,0},{3,2,2},1);auto builderSource=al::serialize(builder);auto migratedBuilder=migrateLegacy(builderSource,"builder.txt");
  check(migratedBuilder.seed==23&&migratedBuilder.files.at("backup/original.txt")==builderSource,"builder migration exposes an explicit seed suggestion and preserves the original");
  check(al::readBuilder(migratedBuilder.files.at("parsed/builder.txt")).moduleWeightMultipliers.at(block.uid)==2.5,"unsupported solver/module weights remain recoverable in canonical legacy data");
  check(migratedBuilder.report["builder"]["lightScale"]==1.75&&!migratedBuilder.report["unsupported"].empty(),"unmapped builder settings receive an explicit migration report");
  al::EditorSettings settings;settings.repoUids={repo.uid};settings.builderUids={builder.uid};settings.activeRepo=repo.uid;settings.activeBuilder=builder.uid;auto settingsSource=al::serialize(settings);auto migratedSettings=migrateLegacy(settingsSource,"settings.txt");
  check(migratedSettings.report["settings"]["repositories"]==Json::array({repo.uid})&&!migratedSettings.seed,"settings inventory retains references without pretending to load linked files");
  auto unknown=migrateLegacy("AUTOLEVEL_REPO 99\nunknown\n","future.txt");check(unknown.report["status"]=="failed"&&unknown.files.at("backup/original.txt")=="AUTOLEVEL_REPO 99\nunknown\n","unknown versions produce a failure report and lossless backup");
  auto malformed=migrateLegacy("AUTOLEVEL_REPO 1\nblock {\n","broken.txt");check(malformed.report["status"]=="failed"&&!malformed.seed,"truncated storage never becomes a successful conversion");
  auto largeRepo=repo;largeRepo.blocks[0].geometry=std::string(65537,'x');auto large=migrateLegacy(al::serialize(largeRepo),"large.txt");check(large.report["convertedModules"][0]["planeBrushValidation"]=="not-run-budget"&&large.files.at("modules/block-1.map")==largeRepo.blocks[0].geometry,"geometry inspection has a bounded budget while preserving larger source modules");
  for(const auto& [name,bytes]:std::map<std::string,std::string>{{"repository.txt",original},{"builder.txt",builderSource},{"settings.txt",settingsSource}})std::ofstream(out/name,std::ios::binary).write(bytes.data(),bytes.size());
  std::ofstream(out/"expected.json")<<Json{{"repositoryBytes",original.size()},{"builderSeed",23},{"moduleMap",block.geometry}}.dump(2);std::cout<<count<<" legacy migration contracts passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
