// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_static_models.h"
#include "../native/ftebroom_model_md2_export.h"
#include <iostream>
static void check(bool ok,const char* name){if(!ok)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
int main(){try{
 std::string source="// original comment\n{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"func_model\"\n\"model\" \"models/reyond-plugins/ivy-old.mdl\"\n\"origin\" \"12 34 56\"\n}\n{\n\"classname\" \"func_model\"\n\"model\" \"progs/custom.mdl\"\n}\n";
 size_t n=99;auto copy=ftebroom::prepareStaticModels(source,2,n);check(n==1&&copy.find("\"classname\" \"misc_model\"")!=copy.npos,"old_ivy_migrates_in_q1_compile_copy");
 check(source.find("misc_model")==source.npos&&copy.find("\"origin\" \"12 34 56\"")!=copy.npos,"source_and_placement_preserved");
 auto entities=ftebroom::rawPreviewEntities(copy);check(entities[2].get("classname")=="func_model","custom_mod_classes_unchanged");
 for(int p:{0,1,3,4})check(ftebroom::prepareStaticModels(source,p,n)==source&&n==0,"other_profiles_unchanged");
 check(ftebroom::prepareStaticModels(copy,2,n)==copy&&n==0,"migration_idempotent");
 check(ftebroom::generatedIvyModels(copy).size()==1&&!ftebroom::generatedIvyPath("models/reyond-plugins/ivy-x/../../escape.mdl"),"generated_resource_paths_scoped");
 ftebroom::modelExport::Mesh mesh;mesh.frames=1;ftebroom::modelExport::Surface s;s.material="textures/reyond-plugins/ivy_leaf_v2.tga";s.triangles={{0,1,2}};s.frames={{{{-16,0,0},{0,-1,0},{0,0}},{{16,0,0},{0,-1,0},{1,0}},{{0,0,32},{0,-1,0},{.5,1}}}};mesh.surfaces={s};
 auto md3=ftebroom::modelExport::md3(mesh);auto deps=ftebroom::generatedModelDependencies(md3);check(deps.size()==1&&deps[0]==s.material,"md3_texture_dependencies");
 std::string md2(132,0),skin="models/reyond-plugins/ivy-skin.pcx";md2.replace(0,4,"IDP2");md2[20]=1;md2[44]=68;md2.replace(68,skin.size(),skin);
 deps=ftebroom::generatedModelDependencies(md2);check(deps.size()==1&&deps[0]==skin,"md2_external_skin_dependency");
 bool rejected=false;try{ftebroom::generatedModelDependencies(md3.substr(0,110));}catch(...){rejected=true;}check(rejected,"truncated_model_rejected");
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
