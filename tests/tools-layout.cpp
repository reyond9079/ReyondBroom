// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone real-ImGui regression. Link the four core ImGui .cpp files.
#include <algorithm>
#include <cmath>
#include <cassert>
#include <iostream>
#include "imgui.h"
#include "imgui_internal.h"
#include "../native/ftebroom_tools_layout.h"
// Standalone fixtures use the untranslated labels for ImGui IDs.
void FteBroomTranslate(const char*&,const char*&){}
static ImGuiID dock;
static void frame(int rows,bool reset=false,bool horizontal=true){
 auto& io=ImGui::GetIO();io.DisplaySize={900,700};io.DeltaTime=1.f/60;
 ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
 ImGui::Begin("host",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking);
 dock=ImGui::GetID("dock");
 if(reset){ImGui::DockBuilderRemoveNode(dock);ImGui::DockBuilderAddNode(dock,ImGuiDockNodeFlags_DockSpace);ImGui::DockBuilderSetNodeSize(dock,{900,650});ImGuiID top,center;ImGui::DockBuilderSplitNode(dock,ImGuiDir_Up,.18,&top,&center);ImGui::DockBuilderDockWindow("Tools",top);ImGui::DockBuilderFinish(dock);}
 ImGui::DockSpace(dock,{0,0},ImGuiDockNodeFlags_PassthruCentralNode);ImGui::End();
 ImGui::Begin("Tools",nullptr,ImGuiWindowFlags_NoCollapse);
 for(int i=0;i<rows;i++)ImGui::Dummy({60,32});
 auto* w=ImGui::GetCurrentWindow();assert(uiToolsHorizontal(w)==horizontal);
 const float content=w->DC.CursorMaxPos.y+w->Scroll.y-w->Pos.y+w->WindowPadding.y;
 if(horizontal)uiToolsFitHeight(w,content,1);ImGui::End();ImGui::Render();
}
int main(){ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_DockingEnable;unsigned char*p;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&p,&w,&h);
 frame(1,true);for(int i=0;i<5;i++)frame(1);auto* tools=ImGui::FindWindowByName("Tools");float collapsed=tools->Size.y;assert(tools->ScrollMax.y<1);
 for(int i=0;i<5;i++)frame(6);assert(tools->Size.y>collapsed+100);assert(tools->ScrollMax.y<1);
 for(int i=0;i<5;i++)frame(1);assert(std::abs(tools->Size.y-collapsed)<1);assert(tools->ScrollMax.y<1);
 ImGui::SetWindowDock(tools,0,ImGuiCond_Always);ImGui::SetWindowSize(tools,{280,70});ImGui::SetWindowPos(tools,{20,20});
 for(int i=0;i<5;i++)frame(12);assert(!tools->DockNode&&tools->Size.y>tools->Size.x);assert(uiToolsHorizontal(tools));
 ImGui::SetWindowSize(tools,{200,500});for(int i=0;i<3;i++)frame(12,false,false);assert(!uiToolsHorizontal(tools));
 ImGui::SetWindowSize(tools,{600,80});for(int i=0;i<5;i++)frame(6);assert(uiToolsHorizontal(tools));
 std::cout<<"PASS: actual ImGui dock grows for six rows, shrinks for one row, dock/floating orientation remains stable after natural growth, manual resize can choose vertical/horizontal, no vertical overflow\n";ImGui::DestroyContext();}
