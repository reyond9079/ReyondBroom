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
 std::string authoring="{\n\"classname\" \"worldspawn\"\n}\n{\n\"classname\" \"func_model\"\n\"model\" \"models/monster.mdl\"\n\"_reyondbroom_authoring\" \"1\"\n\"_reyondbroom_fte_q1\" \"1\"\n\"origin\" \"0 0 24\"\n}\n";
 auto authoringCopy=ftebroom::prepareStaticModels(authoring,2,n);check(n==1&&authoringCopy.find("misc_model")!=authoringCopy.npos,"authoring_model_uses_native_q1_runtime_fallback");check(authoring.find("misc_model")==authoring.npos&&ftebroom::generatedIvyModels(authoringCopy).count("models/monster.mdl"),"authoring_model_staged_without_source_change");
 auto q3=ftebroom::prepareStaticModels(authoring,0,n);check(n==1&&q3.find("misc_model")!=q3.npos,"q3_build_copy_adapts_marked_authoring_models");
 check(ftebroom::prepareStaticModels(authoring,1,n)==authoring&&n==0,"fte_keeps_authoring_func_model");
 for(int profile:{2,3,4}){auto unsupported=authoring;if(profile==2){auto at=unsupported.find("_reyondbroom_fte_q1");unsupported.replace(at,20,"extension_disabledxx");}bool rejected=false;try{ftebroom::prepareStaticModels(unsupported,profile,n);}catch(const std::runtime_error&){rejected=true;}check(rejected,"unsupported_runtime_has_explicit_diagnostic");}
 auto scaled=authoring;auto position=scaled.find("\"origin\"");scaled.insert(position,"\"modelscale\" \"0.5\"\n\"modelscale_vec\" \"1 2 3\"\n");auto q3scaled=ftebroom::rawPreviewEntities(ftebroom::prepareStaticModels(scaled,0,n));check(q3scaled[1].get("modelscale_vec")=="0.5 1 1.5","q3_bakes_effective_scalar_and_vector_transform");check(scaled.find("0.5 1 1.5")==scaled.npos,"transform_adaptation_never_changes_source");
 bool badScale=false;try{ftebroom::prepareStaticModels(scaled,2,n);}catch(const std::runtime_error&){badScale=true;}check(badScale,"q1_nonuniform_scale_has_explicit_diagnostic");
 auto uniform=scaled;uniform.replace(uniform.find("1 2 3"),5,"2 2 2");auto q1scaled=ftebroom::rawPreviewEntities(ftebroom::prepareStaticModels(uniform,2,n));check(q1scaled[1].get("scale")=="1","q1_fallback_receives_effective_uniform_scale");
 uniform.insert(uniform.find("\"origin\""),"\"scale\" \"3\"\n");auto overrideScale=ftebroom::rawPreviewEntities(ftebroom::prepareStaticModels(uniform,0,n));check(overrideScale[1].get("modelscale_vec")=="6 6 6","explicit_scale_overrides_authoring_modelscale");
 check(!ftebroom::generatedAuthoringPath("../outside.mdl")&&!ftebroom::generatedAuthoringPath("C:/outside.mdl")&&ftebroom::generatedAuthoringPath("progs/monster.mdl"),"authoring_staging_rejects_external_paths");
 ftebroom::modelExport::Mesh mesh;mesh.frames=1;ftebroom::modelExport::Surface s;s.material="textures/reyond-plugins/ivy_leaf_v2.tga";s.triangles={{0,1,2}};s.frames={{{{-16,0,0},{0,-1,0},{0,0}},{{16,0,0},{0,-1,0},{1,0}},{{0,0,32},{0,-1,0},{.5,1}}}};mesh.surfaces={s};
 auto md3=ftebroom::modelExport::md3(mesh);auto deps=ftebroom::generatedModelDependencies(md3);check(deps.size()==1&&deps[0]==s.material,"md3_texture_dependencies");
 std::string md2(132,0),skin="models/reyond-plugins/ivy-skin.pcx";md2.replace(0,4,"IDP2");md2[20]=1;md2[44]=68;md2.replace(68,skin.size(),skin);
 deps=ftebroom::generatedModelDependencies(md2);check(deps.size()==1&&deps[0]==skin,"md2_external_skin_dependency");
 bool rejected=false;try{ftebroom::generatedModelDependencies(md3.substr(0,110));}catch(...){rejected=true;}check(rejected,"truncated_model_rejected");
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
