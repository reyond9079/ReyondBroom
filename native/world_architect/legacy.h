// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "plan.h"
#include "../../plugins/autolevel/al_storage.h"
namespace ftebroom::worldArchitect {
struct LegacyMigration {Json report;std::map<std::string,std::string> files;std::optional<uint64_t> seed;};
// Pure conversion: never writes, follows referenced paths or modifies legacy
// state. The native UI explicitly publishes this complete bundle separately.
inline LegacyMigration migrateLegacy(const std::string& source,const std::string& originalName){
  if(source.empty()||source.size()>8*1024*1024)throw std::runtime_error("Legacy source must contain 1 byte..8 MiB");
  LegacyMigration result;auto& report=result.report;report={{"schemaVersion",1},{"sourceName",originalName},{"sourceBytes",source.size()},{"contentIdentity",std::to_string(hash(source))},{"status","failed"},{"convertedModules",Json::array()},{"unsupported",Json::array()}};
  result.files["backup/original.txt"]=source;
  try{
    std::istringstream header(source);std::string magic;int version=0;header>>magic>>version;report["sourceFormat"]=magic;report["sourceVersion"]=version;
    if(version!=1)throw std::runtime_error("Unsupported AutoLevel storage version; original bytes are retained");
    if(magic=="AUTOLEVEL_REPO"){
      auto repo=al::readRepo(source);report["repository"]={{"uid",repo.uid},{"name",repo.name},{"cellSize",vector(repo.cellSize)},{"blocks",repo.blocks.size()},{"bigBlocks",repo.bigBlocks.size()},{"materials",repo.materialSlots}};
      result.files["parsed/repository.txt"]=al::serialize(repo);size_t total=0,validationBytes=0;
      for(size_t i=0;i<repo.blocks.size();i++){
        const auto& block=repo.blocks[i];Json item={{"uid",block.uid},{"name",block.name},{"active",block.active},{"group",block.group},{"weightGroup",block.weightGroup},{"variants",block.variants.size()},{"walkable",block.walkable},{"walkableSides",block.walkableSides},{"hasSpawn",block.hasSpawn},{"spawn",vector(block.spawnPoint)}};
        if(block.geometry.empty()){item["status"]="empty";report["convertedModules"].push_back(item);continue;}
        auto path="modules/block-"+std::to_string(i+1)+".map";result.files[path]=block.geometry;item["file"]=path;item["status"]="preserved";total+=block.geometry.size();
        try{if(block.geometry.size()>65536||validationBytes+block.geometry.size()>262144){item["planeBrushValidation"]="not-run-budget";item["diagnostic"]="Inspect the preserved MAP in the editor; migration does not perform unbounded geometry validation.";}else{validationBytes+=block.geometry.size();auto geometry=al::parseMap(block.geometry);item["brushes"]=geometry.brushes.size();item["pointEntities"]=geometry.entities.size();std::set<std::string> materials;for(const auto& brush:geometry.brushes)for(const auto& face:brush.faces)materials.insert(face.material);item["materialDependencies"]=materials;item["planeBrushValidation"]="passed";}}
        catch(const std::exception& e){item["planeBrushValidation"]="unsupported";item["diagnostic"]=e.what();}
        report["convertedModules"].push_back(item);
      }
      if(total>8*1024*1024)throw std::runtime_error("Extracted module budget exceeded");
      report["unsupported"]={"WFC adjacency/side IDs, filling, big-block assembly, variants, dependencies and solver weight groups are retained in the original/canonical repository, not reinterpreted as World Architect grammars.","Extracted MAPs preserve stored author geometry and entity text. They need their original materials and game profile; no external texture files are followed or copied.","No generated cells or authored map regions are replaced. Inspect/import the extracted module MAPs using the editor's existing MAP workflow."};
      report["status"]="partial";
    }else if(magic=="AUTOLEVEL_BUILDER"){
      auto builder=al::readBuilder(source);result.seed=uint64_t(uint32_t(builder.seed));result.files["parsed/builder.txt"]=al::serialize(builder);
      report["builder"]={{"uid",builder.uid},{"name",builder.name},{"repository",builder.repoUid},{"seed",builder.seed},{"worldOrigin",vector(builder.worldOrigin)},{"cellPosition",Json::array({builder.levelData.position.x,builder.levelData.position.y,builder.levelData.position.z})},{"cellSize",Json::array({builder.levelData.size.x,builder.levelData.size.y,builder.levelData.size.z})},{"lightScale",builder.lightScale},{"layers",builder.levelData.layers.size()},{"minimumCoverage",builder.minCoverage},{"moduleWeights",builder.moduleWeightMultipliers}};
      report["suggestedWorldSeed"]=*result.seed;report["unsupported"]={"The seed can be explicitly copied; different generation algorithms do not reproduce the previous cell layout.","Cell waves, solved layers, boundaries, module weights, coverage and light settings remain in the backup/canonical builder. They are not silently mapped to unrelated architectural controls."};report["status"]="partial";
    }else if(magic=="AUTOLEVEL_SETTINGS"){
      auto settings=al::readSettings(source);result.files["parsed/settings.txt"]=al::serialize(settings);report["settings"]={{"repositories",settings.repoUids},{"builders",settings.builderUids},{"activeRepository",settings.activeRepo},{"activeBuilder",settings.activeBuilder},{"workbenchOrigin",vector(settings.workbenchOrigin)},{"solverIterations",settings.maxIterations},{"exportChunkSize",settings.exportSize}};
      report["unsupported"]={"Load each referenced repository/builder explicitly. Referenced paths are not followed automatically.","Legacy editor and solver settings are retained in the original/canonical settings file; existing AutoLevel work remains available."};report["status"]="partial";
    }else throw std::runtime_error("Expected AUTOLEVEL_REPO, AUTOLEVEL_BUILDER or AUTOLEVEL_SETTINGS version 1");
  }catch(const std::exception& e){report["status"]="failed";report["diagnostic"]=e.what();result.seed.reset();}
  result.files["report.json"]=report.dump(2)+"\n";return result;
}
}
