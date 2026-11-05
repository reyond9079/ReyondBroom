// SPDX-License-Identifier: MIT
// Algorithm design after AutoLevel by Al-Asl (MIT).
#include "al_storage.cpp"
#include "al_brush.cpp"
#include "al_repo_compile.cpp"
#include "al_solver.cpp"
#include "al_group_solver.cpp"
#include "al_default_repo.cpp"
#include "al_legacy.cpp"
#include "al_build_map.cpp"
#include "al_app.cpp"
#include "al_job.cpp"
#include "al_ui_builder.cpp"
#include "al_ui_repo.cpp"
#include "al_viewport.cpp"
#include "al_viewport_tools.cpp"
extern "C" RB_PLUGIN_EXPORT const RBPluginV1* ReyondBroomPluginInit(const RBHostV1* api){
 using namespace alui;if(!api||api->api_version!=1)return nullptr;host=api;
 ready=AL_HAS(text)&&AL_HAS(button)&&AL_HAS(disabled)&&AL_HAS(same_line)&&AL_HAS(separator)&&AL_HAS(number)&&AL_HAS(string)&&AL_HAS(command)&&AL_HAS(import_map)&&AL_HAS(input_int)&&AL_HAS(input_float)&&AL_HAS(checkbox)&&AL_HAS(combo)&&AL_HAS(progress)&&AL_HAS(read_data)&&AL_HAS(write_data)&&AL_HAS(input_text)&&AL_HAS(section)&&AL_HAS(preview_lines)&&AL_HAS(replace_selection);
 static RBPluginV1 plugin{1,sizeof(RBPluginV1),"org.reyondbroom.autolevel",draw,tick,busy,shutdown};return &plugin;
}
