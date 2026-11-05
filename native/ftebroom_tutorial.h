// SPDX-License-Identifier: GPL-2.0-or-later
// Lessons run in a disposable map and consume the normal editor input/tools.
static int tourStep=0,tourSavedLayout=1;
static bool tourDemo=false,tourDemoPressed=false,tourDone=false,tourWaitingForMap=false;
static float tourSavedGrid=16,tourSavedWheel=1,tourGrid=16;
static double tourStepTime=0,tourInputReady=0;
static float tourCameraStart[7]{};
static bool tourCameraBaseline=false,tourCameraMoved=false,tourCameraLooked=false;
static ImVec2 tourSkipCenter,tourNextCenter,tourDemoStart;
static void tourRecord(const char* status){if(!cvars->GetFloat("ftebroom_skip_setup"))std::ofstream(documentPath("onboarding-v1.txt"))<<status<<'\n';}
static void tourPrepare(int step){
  cancelGesture();cameraEnd();tourStep=step;tourDone=false;tourCameraBaseline=tourCameraMoved=tourCameraLooked=false;
  tourStepTime=ImGui::GetTime();tourDemo=step>=1&&step<=4;tourDemoPressed=false;tourInputReady=tourStepTime+.6;
  cvars->SetFloat("ftebroom_tour_step",step);cvars->SetFloat("ftebroom_tour_result",0);cvars->SetFloat("ftebroom_tour_demo",tourDemo);
  cvars->SetFloat("ftebroom_shift",0);cvars->SetFloat("ftebroom_alt",0);
  command("ftebroom_tool_toggle default\n");cvars->SetFloat("ftebroom_tool",0);
  if(step<=4){tourWaitingForMap=true;tourInputReady=tourStepTime+30;command(gameClassic()?"mapedit maps/reyond-tutorial.map\n":"mapedit maps/reyond-tutorial-q3.map\n");}
  if(step==5)tourGrid=cvars->GetFloat("ca_grid");
}
static void tourMapReady(){
  if(!tourActive||!tourWaitingForMap)return;tourWaitingForMap=false;
  // Resource reload/camera restoration must finish before creating the example.
  tourStepTime=ImGui::GetTime()+.35;tourInputReady=tourStepTime+.6;
  command(("in 0.3 ftebroom_tour_prepare "+std::to_string(tourStep)+"\nin 0.35 ftebroom_camera_restore 0 -400 260 25 90 0 80\n").c_str());
}
static void tourBegin(){
  tourActive=true;tourOfferPending=false;recentWindow=false;setupNeeded=false;
  tourSavedLayout=viewLayout;tourSavedGrid=cvars->GetFloat("ca_grid");tourSavedWheel=cvars->GetFloat("ftebroom_wheel_grid");
  viewLoaded=true;viewLayout=1;viewFocus=0;viewMax=-1;viewTypes[0]=0;cvars->SetFloat("ftebroom_view_layout",1);cvars->SetFloat("ca_brush_view",0);
  cvars->SetFloat("ftebroom_tour_active",1);cvars->SetFloat("ca_grid",16);cvars->SetFloat("ftebroom_wheel_grid",1);tourRecord("started");tourPrepare(0);
}
static void tourFinish(bool skipped){
  cancelGesture();cameraEnd();tourActive=false;tourDemo=false;tourWaitingForMap=false;tourOfferPending=false;
  cvars->SetFloat("ftebroom_tour_active",0);cvars->SetFloat("ftebroom_tour_demo",0);cvars->SetFloat("ftebroom_shift",0);cvars->SetFloat("ftebroom_alt",0);
  cvars->SetFloat("ca_grid",tourSavedGrid);cvars->SetFloat("ftebroom_wheel_grid",tourSavedWheel);cvars->SetFloat("ftebroom_tool",0);
  viewLayout=tourSavedLayout;cvars->SetFloat("ftebroom_view_layout",viewLayout);viewTypes[0]=0;viewFocus=0;
  documentFile.clear();pendingDocumentFile.clear();pendingDocumentAlias.clear();documentUntitled=true;documentName="untitled";
  command(gameClassic()?"mapedit maps/start-quake1.map\n":"mapedit maps/start.map\n");tourRecord(skipped?"skipped":"complete");
}
static void tourOfferPanel(){
  if(tourOfferPending){tourOfferPending=false;if(!std::filesystem::exists(documentPath("onboarding-v1.txt")))ImGui::OpenPopup("Learn ReyondBroom");}
  if(uiDialog("Learn ReyondBroom",{600,300})){
    ImGui::TextUnformatted("Learn the basics in a few minutes");
    ImGui::TextWrapped("Try camera movement, creating and moving brushes, vertical movement, extrude and the grid. Demonstrations use an animated cursor. Practice in a separate disposable scene; you can skip the lesson at any time.");
    if(ImGui::Button("Start onboarding")){ImGui::CloseCurrentPopup();documentRequest("__tour");}
    if(ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x-ImGui::GetItemRectMax().x>=ImGui::CalcTextSize(uiTr("Skip onboarding")).x+ImGui::GetStyle().FramePadding.x*2+ImGui::GetStyle().ItemSpacing.x)ImGui::SameLine();
    if(ImGui::Button("Skip onboarding")){tourRecord("declined");ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();
  }
}
static ImVec2 tourAnchor(float w,float h){
  // Use the renderer's actual projection, including FOV adaptation and UI scaling.
  char text[128]{};float x=w*.5f,y=h*.5f;cvars->GetString("ftebroom_tour_anchor",text,sizeof text);
  if(std::sscanf(text,"%f %f",&x,&y)!=2||!std::isfinite(x)||!std::isfinite(y))return {w*.5f,h*.5f};
  return {x,y};
}
static void tourPointer(const char* phase,ImVec2 p){
  char text[192];int alt=tourStep==3,shift=tourStep==4;
  cvars->SetFloat("ftebroom_shift",shift);cvars->SetFloat("ftebroom_alt",alt);
  std::snprintf(text,sizeof text,"ftebroom_pointer %s %.3f %.3f %d 0 %d\n",phase,p.x,p.y,alt,shift?2:0);command(text);
}
static void tourPanel(float w,float h){
  const double now=ImGui::GetTime();const float scale=uiScale();
  static const char* titles[]={"Fly around the scene","Create a brush","Move a brush","Move vertically","Extrude a face","Change the grid","Explore the tools"};
  static const char* tasks[]={"Hold the right mouse button to look around. Use WASD to move. Try both movement and looking.","Click and drag on empty ground with the left mouse button to create your own brush. Its height follows the grid.","Click the example brush to select it. Hold the left mouse button and drag it across the horizontal plane.","Hold Alt while dragging the selected brush with the left mouse button to move it up or down.","Keep the brush selected. Hold left Shift and drag a face or silhouette edge to expand it.","Hover over the scene and turn the mouse wheel to change the grid size.","The Tools panel lives above the viewport by default. Explore its tools at your own pace after the lesson."};
  if(tourStep==0&&now>tourInputReady){char text[512]{};cvars->GetString("ftebroom_camera_state",text,sizeof text);float v[7];if(cameraParse(text,v)){
    if(!tourCameraBaseline){std::copy(v,v+7,tourCameraStart);tourCameraBaseline=true;}
    float distance=0;for(int a=0;a<3;a++)distance+=(v[a]-tourCameraStart[a])*(v[a]-tourCameraStart[a]);tourCameraMoved|=distance>32*32;
    tourCameraLooked|=std::abs(v[3]-tourCameraStart[3])+std::abs(v[4]-tourCameraStart[4])>8;tourDone=tourCameraMoved&&tourCameraLooked;
  }}
  if(!tourDemo&&now>tourInputReady&&tourStep>=1&&tourStep<=4)tourDone|=cvars->GetFloat("ftebroom_tour_result")==tourStep;
  if(tourStep==5&&cvars->GetFloat("ca_grid")!=tourGrid)tourDone=true;if(tourStep==6)tourDone=true;
  ImGui::SetNextWindowPos({16*scale,16*scale});ImGui::SetNextWindowSize({std::max(200.f,w-32*scale),0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,8*scale);
  ImGui::Begin("##lesson",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text(uiTr("ReyondBroom / %d of 7"),tourStep+1);
  float buttonWidth=ImGui::CalcTextSize(uiTr("Skip onboarding")).x+ImGui::GetStyle().FramePadding.x*2;
  float right=ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-buttonWidth;
  if(right>ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x)ImGui::SameLine(right);else ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,right));
  if(ImGui::Button("Skip onboarding")){tourFinish(true);}
  auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();tourSkipCenter={(a.x+b.x)*.5f,(a.y+b.y)*.5f};
  uiSectionHeading(titles[tourStep]);const char* task=tasks[tourStep];const char* end=nullptr;FteBroomTranslate(task,end);ImGui::TextWrapped("%s",task);
  if(tourDemo)ImGui::TextUnformatted("Watch the demonstration...");else if(tourDone)ImGui::TextColored({.45f,.95f,.55f,1},"Done! Continue when you are ready.");else ImGui::TextUnformatted("Your turn");
  ImGui::ProgressBar((tourStep+(tourDone?1.f:0.f))/7,{-1,3*scale},"");
  ImGui::End();ImGui::PopStyleVar();
  if(!tourActive)return;
  if(tourDemo&&!tourWaitingForMap){
    float elapsed=float(now-tourStepTime);float t=std::clamp((elapsed-1.4f)/1.8f,0.f,1.f);t=t*t*(3-2*t);
    if(!tourDemoPressed)tourDemoStart=tourAnchor(w,h);
    ImVec2 start=tourDemoStart,end{start.x+w*.12f,start.y};
    if(tourStep==1){start={w*.40f,h*.66f};end={w*.59f,h*.56f};}
    if(tourStep==3||tourStep==4)end={start.x,start.y-h*.14f};
    ImVec2 p{start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t};bool held=elapsed>=1.4f&&elapsed<3.2f;
    if(elapsed>=1.4f&&!tourDemoPressed){tourPointer("down",start);tourDemoPressed=true;}
    if(held)tourPointer("move",p);
    if(elapsed>=3.2f){tourPointer("up",end);command("set ftebroom_shift 0\nset ftebroom_alt 0\nset ftebroom_tour_demo 0\nset ftebroom_tour_result 0\n");if(tourStep==1)command("ftebroom_deselect\n");tourDemo=false;tourInputReady=now+.4;}
    auto draw=ImGui::GetForegroundDrawList();draw->AddTriangleFilled(p,{p.x+4*scale,p.y+27*scale},{p.x+12*scale,p.y+17*scale},IM_COL32(255,255,255,255));draw->AddTriangle(p,{p.x+4*scale,p.y+27*scale},{p.x+12*scale,p.y+17*scale},IM_COL32(25,25,25,255),1.5f);
    draw->AddRectFilled({p.x+20*scale,p.y+10*scale},{p.x+58*scale,p.y+46*scale},IM_COL32(24,26,30,230),6*scale);
    draw->AddRect({p.x+29*scale,p.y+15*scale},{p.x+49*scale,p.y+39*scale},IM_COL32(240,240,240,255),6*scale,0,2);
    if(held)draw->AddRectFilled({p.x+30*scale,p.y+16*scale},{p.x+39*scale,p.y+26*scale},IM_COL32(241,161,70,255),3*scale);
    if(tourStep>=3)draw->AddText({p.x+20*scale,p.y+50*scale},IM_COL32_WHITE,uiTr(tourStep==3?"Alt":"Left Shift"));
  }
  if(tourStep==6){ImGui::SetNextWindowPos({16*scale,150*scale});ImGui::SetNextWindowSize({w-32*scale,0});ImGui::Begin("Tools##lesson",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove);ImGui::BeginDisabled();for(int t:{0,1,2,3,7,8,9,5,6}){if(t)ImGui::SameLine();FteIcons::button(("##lessonTool"+std::to_string(t)).c_str(),t,{34*scale,34*scale});}ImGui::EndDisabled();ImGui::End();}
  ImGui::SetNextWindowPos({w*.5f,h-18*scale},ImGuiCond_Always,{.5f,1});
  ImGui::Begin("##lessonNavigation",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDocking);
  if(tourStep>=1&&tourStep<=4){if(ImGui::Button("Watch again"))tourPrepare(tourStep);ImGui::SameLine();}
  ImGui::BeginDisabled(!tourDone);if(ImGui::Button(tourStep==6?"Start editing":"Continue")){if(tourStep==6)tourFinish(false);else tourPrepare(tourStep+1);}a=ImGui::GetItemRectMin();b=ImGui::GetItemRectMax();tourNextCenter={(a.x+b.x)*.5f,(a.y+b.y)*.5f};ImGui::EndDisabled();ImGui::End();
}
