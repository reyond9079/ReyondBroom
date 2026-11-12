// SPDX-License-Identifier: GPL-2.0-or-later
// Lessons run in a disposable map and consume the normal editor input/tools.
static constexpr int tourViewportStep=6,tourClipStep=7,tourToolsStep=8,tourStepCount=9;
static int tourStep=0,tourSavedLayout=1,tourSavedFocus=0,tourSavedMax=-1;
static std::array<int,4> tourSavedViewTypes;
static bool tourDemo=false,tourDemoPressed=false,tourDone=false,tourWaitingForMap=false;
static bool tourChangedLayout=false,tourClipChosen=false;
static int tourClipDemoAction=0;
static float tourSavedGrid=16,tourSavedWheel=1,tourGrid=16,tourSavedClipKeep=0;
static float tourSavedSplitX=.5f,tourSavedSplitY=.5f;
static double tourStepTime=0,tourInputReady=0;
static float tourCameraStart[7]{};
static bool tourCameraBaseline=false,tourCameraMoved=false,tourCameraLooked=false;
static ImVec2 tourSkipCenter,tourNextCenter,tourDemoStart;
static nlohmann::json tourQaItems=nlohmann::json::object();
static bool tourQaEnabled(){return cvars->GetFloat("ftebroom_skip_setup")&&cvars->GetFloat("ftebroom_followup133_qa");}
static void tourQaItem(const std::string& name){
  if(!tourQaEnabled())return;
  const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
  tourQaItems[name]={(a.x+b.x)*.5f,(a.y+b.y)*.5f};
}
static void tourRecord(const char* status){if(!cvars->GetFloat("ftebroom_skip_setup"))std::ofstream(documentPath("onboarding-v1.txt"))<<status<<'\n';}
static void tourPrepare(int step){
  cancelGesture();cameraEnd();tourStep=step;tourDone=false;tourCameraBaseline=tourCameraMoved=tourCameraLooked=false;
  tourChangedLayout=tourClipChosen=false;tourClipDemoAction=0;
  viewSetLayout(1);viewFocusArmed=false;
  tourStepTime=ImGui::GetTime();tourDemo=step>=1&&step<=4;tourDemoPressed=false;tourInputReady=tourStepTime+.6;
  cvars->SetFloat("ftebroom_tour_fixture_ready",0);cvars->SetFloat("ftebroom_tour_step",step);cvars->SetFloat("ftebroom_tour_result",0);cvars->SetFloat("ftebroom_tour_demo",tourDemo);
  cvars->SetFloat("ftebroom_shift",0);cvars->SetFloat("ftebroom_alt",0);
  command("ftebroom_tool_toggle default\n");cvars->SetFloat("ftebroom_tool",0);
  // Every lesson owns a fresh disposable scene, including the grid/tools pages.
  // Practice brushes from the preceding lesson must never accumulate.
  tourWaitingForMap=true;tourInputReady=tourStepTime+30;command(gameClassic()?"mapedit maps/reyond-tutorial.map\n":"mapedit maps/reyond-tutorial-q3.map\n");
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
  tourSavedFocus=viewFocus;tourSavedMax=viewMax;tourSavedClipKeep=cvars->GetFloat("ftebroom_clip_keep");
  tourSavedSplitX=viewSplitX;tourSavedSplitY=viewSplitY;viewSplitX=viewSplitY=.5f;
  std::copy_n(viewTypes,4,tourSavedViewTypes.begin());viewLoaded=true;
  cvars->SetFloat("ftebroom_tour_active",1);cvars->SetFloat("ca_grid",16);cvars->SetFloat("ftebroom_wheel_grid",1);tourRecord("started");tourPrepare(0);
}
static void tourFinish(bool skipped){
  cancelGesture();cameraEnd();tourActive=false;tourDemo=false;tourWaitingForMap=false;tourOfferPending=false;
  cvars->SetFloat("ftebroom_tour_active",0);cvars->SetFloat("ftebroom_tour_demo",0);cvars->SetFloat("ftebroom_shift",0);cvars->SetFloat("ftebroom_alt",0);
  cvars->SetFloat("ca_grid",tourSavedGrid);cvars->SetFloat("ftebroom_wheel_grid",tourSavedWheel);
  cvars->SetFloat("ftebroom_clip_keep",tourSavedClipKeep);cvars->SetFloat("ftebroom_tool",0);
  viewLayout=tourSavedLayout;viewMax=tourSavedMax;cvars->SetFloat("ftebroom_view_layout",viewLayout);
  viewSplitX=tourSavedSplitX;viewSplitY=tourSavedSplitY;
  std::copy(tourSavedViewTypes.begin(),tourSavedViewTypes.end(),viewTypes);viewActivate(tourSavedFocus);
  documentFile.clear();pendingDocumentFile.clear();pendingDocumentAlias.clear();documentUntitled=true;documentName="untitled";
  command(gameClassic()?"mapedit maps/start-quake1.map\n":"mapedit maps/start.map\n");tourRecord(skipped?"skipped":"complete");
}
static void tourOfferPanel(){
  if(tourOfferPending){tourOfferPending=false;if(!std::filesystem::exists(documentPath("onboarding-v1.txt")))ImGui::OpenPopup("Learn ReyondBroom");}
  if(uiDialog("Learn ReyondBroom",{600,300})){
    ImGui::TextUnformatted("Learn the basics in a few minutes");
    ImGui::TextWrapped("Practice camera movement, brush editing, the grid, viewport layouts, and Clip. You can skip the lesson at any time.");
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
static bool tourFixtureReady(){return !tourWaitingForMap&&cvars->GetFloat("ftebroom_tour_fixture_ready")==tourStep+1;}
static bool tourKey(int key,bool down,bool inScene,int hit){
  if(tourDemo||!tourFixtureReady()||ImGui::GetTime()<tourInputReady)return true;
  if(tourStep==tourViewportStep){
    if(down&&inScene&&tourChangedLayout&&!ctrlHeld&&!altHeld&&!shiftHeld&&key==input->GetKeyCode("space",nullptr)){
      viewActivate(hit);viewAction("cycle");tourDone=true;
    }
    return true;
  }
  if(tourStep==tourClipStep){
    if(tourClipChosen&&!tourDone&&cvars->GetFloat("ftebroom_tool")==2){
      if(down&&key==input->GetKeyCode("enter",nullptr)&&!ctrlHeld&&!altHeld&&!shiftHeld){
        command("ftebroom_tool_key apply\n");return true;
      }
      return key!=mouseKeys[0];
    }
    return true;
  }
  return tourStep==tourToolsStep;
}
static void tourDemoCursor(ImVec2 p,float scale,bool held,const char* key=nullptr){
  auto draw=ImGui::GetForegroundDrawList();
  draw->AddTriangleFilled(p,{p.x+4*scale,p.y+27*scale},{p.x+12*scale,p.y+17*scale},IM_COL32_WHITE);
  draw->AddTriangle(p,{p.x+4*scale,p.y+27*scale},{p.x+12*scale,p.y+17*scale},IM_COL32(25,25,25,255),1.5f);
  const ImVec2 card{p.x+20*scale,p.y+10*scale};
  if(key){
    const auto size=ImGui::CalcTextSize(uiTr(key));
    draw->AddRectFilled(card,{card.x+size.x+16*scale,card.y+size.y+10*scale},held?IM_COL32(241,161,70,255):IM_COL32(24,26,30,230),6*scale);
    draw->AddText({card.x+8*scale,card.y+5*scale},held?IM_COL32(25,25,25,255):IM_COL32_WHITE,uiTr(key));
    return;
  }
  draw->AddRectFilled(card,{p.x+58*scale,p.y+46*scale},IM_COL32(24,26,30,230),6*scale);
  draw->AddRect({p.x+29*scale,p.y+15*scale},{p.x+49*scale,p.y+39*scale},IM_COL32(240,240,240,255),6*scale,0,2);
  if(held)draw->AddRectFilled({p.x+30*scale,p.y+16*scale},{p.x+39*scale,p.y+26*scale},IM_COL32(241,161,70,255),3*scale);
}
static bool tourClipPoints(ImVec2 (&points)[2]){
  char text[160]{};cvars->GetString("ftebroom_tour_clip_pixels",text,sizeof text);
  return std::sscanf(text,"%f %f %f %f",&points[0].x,&points[0].y,&points[1].x,&points[1].y)==4&&
    viewHit(points[0].x,points[0].y)==0&&viewHit(points[1].x,points[1].y)==0;
}
static void tourQaPublish(float width,float height){
  if(!tourQaEnabled())return;
  static double next=0;if(ImGui::GetTime()<next)return;next=ImGui::GetTime()+.08;
  nlohmann::json views=nlohmann::json::array(),points=nlohmann::json::array();
  for(int i=0;i<viewLayout;i++){
    const auto& v=viewRects[i];views.push_back({{"x",v.x},{"y",v.y},{"width",v.w},{"height",v.h},{"type",v.type}});
  }
  ImVec2 targets[2];if(tourStep==tourClipStep&&tourClipPoints(targets))for(auto p:targets)points.push_back({p.x,p.y});
  const nlohmann::json state={{"display",{width,height}},{"step",tourStep},{"ready",tourFixtureReady()&&ImGui::GetTime()>=tourInputReady},
    {"demo",tourDemo},{"done",tourDone},{"chosen",tourClipChosen},{"tool",int(cvars->GetFloat("ftebroom_tool"))},
    {"clipPoints",int(cvars->GetFloat("ftebroom_clip_points"))},{"targets",points},{"views",views},{"widgets",tourQaItems}};
  try{bb::atomicWrite(documentPath("onboarding-live.json"),state.dump());}catch(const std::exception&){}
}
static void tourClipDemo(double now,float scale){
  ImVec2 points[2];if(!tourClipPoints(points))return;
  const float elapsed=float(now-tourStepTime);
  // Wait for a fresh render between the two real mouse gestures and the cut.
  if(tourClipDemoAction<2&&elapsed>=1.f+tourClipDemoAction*1.25f){
    tourPointer("down",points[tourClipDemoAction]);tourPointer("up",points[tourClipDemoAction]);++tourClipDemoAction;
  }else if(tourClipDemoAction==2&&elapsed>=3.5f){
    command("ftebroom_tool_key apply\n");++tourClipDemoAction;
  }else if(tourClipDemoAction==3&&elapsed>=4.8f){
    command("ftebroom_tour_restore\nftebroom_tool_toggle clip\nset ftebroom_tour_result 0\nset ftebroom_tour_demo 0\n");
    tourDemo=false;tourInputReady=now+.35;return;
  }
  ImVec2 p=points[0];p.x-=30*scale*(1-std::clamp(elapsed,0.f,1.f));
  if(elapsed>1.35f){
    float t=std::clamp((elapsed-1.35f)/.9f,0.f,1.f);t=t*t*(3-2*t);
    p={points[0].x+(points[1].x-points[0].x)*t,points[0].y+(points[1].y-points[0].y)*t};
  }
  const bool click=(elapsed>=1.f&&elapsed<1.3f)||(elapsed>=2.25f&&elapsed<2.55f);
  tourDemoCursor(p,scale,elapsed>=3.2f?elapsed>=3.5f&&elapsed<3.85f:click,elapsed>=3.2f?"Enter":nullptr);
}
static float tourWheelPhase(double now){return float(std::fmod(now*2.5,6.283185307179586));}
static void tourWheelIndicator(float w,float h,double now,float scale){
  auto draw=ImGui::GetForegroundDrawList();ImVec2 center{w*.5f,h*.58f};
  draw->AddRectFilled({center.x-51*scale,center.y-53*scale},{center.x+51*scale,center.y+61*scale},IM_COL32(24,26,30,225),12*scale);
  draw->AddRect({center.x-21*scale,center.y-35*scale},{center.x+21*scale,center.y+43*scale},IM_COL32(240,240,240,255),20*scale,0,2*scale);
  draw->AddLine({center.x-20*scale,center.y-5*scale},{center.x+20*scale,center.y-5*scale},IM_COL32(240,240,240,255),1.5f*scale);
  draw->AddLine({center.x,center.y-34*scale},{center.x,center.y-5*scale},IM_COL32(240,240,240,255),1.5f*scale);
  ImVec2 wheel{center.x,center.y-20*scale};
  draw->AddRectFilled({wheel.x-4*scale,wheel.y-9*scale},{wheel.x+4*scale,wheel.y+9*scale},IM_COL32(241,161,70,255),4*scale);
  // A moving arc and arrowhead make the wheel gesture visible before input.
  float phase=tourWheelPhase(now),end=phase+4.7f,radius=30*scale;
  draw->PathArcTo(wheel,radius,phase,end,32);draw->PathStroke(IM_COL32(241,161,70,255),0,2.5f*scale);
  ImVec2 tip{wheel.x+std::cos(end)*radius,wheel.y+std::sin(end)*radius};
  ImVec2 tangent{-std::sin(end),std::cos(end)},radial{std::cos(end),std::sin(end)};
  draw->AddTriangleFilled(tip,{tip.x-tangent.x*9*scale+radial.x*5*scale,tip.y-tangent.y*9*scale+radial.y*5*scale},{tip.x-tangent.x*9*scale-radial.x*5*scale,tip.y-tangent.y*9*scale-radial.y*5*scale},IM_COL32(241,161,70,255));
}
static void tourPanel(float w,float h){
  const double now=ImGui::GetTime();const float scale=uiScale();
  if(tourQaEnabled())tourQaItems=nlohmann::json::object();
  static const char* titles[]={"Fly around the scene","Create a brush","Move a brush","Move vertically","Extrude a face","Change the grid","Work with multiple viewports","Cut a brush","Explore the tools"};
  static const char* tasks[]={"Hold the right mouse button to look around. Use WASD to move. Try both movement and looking.","Click and drag on empty ground with the left mouse button to create your own brush. Its height follows the grid.","Click the example brush to select it. Hold the left mouse button and drag it across the horizontal plane.","Hold Alt while dragging the selected brush with the left mouse button to move it up or down.","Keep the brush selected. Hold left Shift and drag a face or silhouette edge to expand it.","Hover over the scene and turn the mouse wheel to change the grid size.","Choose a layout with multiple views. Hover over a viewport and press Space to change its projection.","Select Clip on the Tools panel to watch the demonstration.","The Tools panel lives above the viewport by default. Explore its tools at your own pace after the lesson."};
  static_assert(IM_ARRAYSIZE(titles)==tourStepCount&&IM_ARRAYSIZE(tasks)==tourStepCount);
  if(tourStep==0&&tourFixtureReady()&&now>tourInputReady){char text[512]{};cvars->GetString("ftebroom_camera_state",text,sizeof text);float v[7];if(cameraParse(text,v)){
    if(!tourCameraBaseline){std::copy(v,v+7,tourCameraStart);tourCameraBaseline=true;}
    float distance=0;for(int a=0;a<3;a++)distance+=(v[a]-tourCameraStart[a])*(v[a]-tourCameraStart[a]);tourCameraMoved|=distance>32*32;
    tourCameraLooked|=std::abs(v[3]-tourCameraStart[3])+std::abs(v[4]-tourCameraStart[4])>8;tourDone=tourCameraMoved&&tourCameraLooked;
  }}
  if(!tourDemo&&now>tourInputReady&&tourStep>=1&&tourStep<=4)tourDone|=cvars->GetFloat("ftebroom_tour_result")==tourStep;
  if(tourStep==5&&cvars->GetFloat("ca_grid")!=tourGrid)tourDone=true;
  if(tourStep==tourClipStep&&!tourDemo&&tourClipChosen)tourDone|=cvars->GetFloat("ftebroom_tour_result")==tourClipStep;
  if(tourStep==tourToolsStep&&tourFixtureReady())tourDone=true;
  ImGui::SetNextWindowPos({16*scale,16*scale});ImGui::SetNextWindowSize({std::max(200.f,w-32*scale),0});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,8*scale);
  ImGui::Begin("##lesson",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text(uiTr("ReyondBroom / %d of %d"),tourStep+1,tourStepCount);
  float buttonWidth=ImGui::CalcTextSize(uiTr("Skip onboarding")).x+ImGui::GetStyle().FramePadding.x*2;
  float right=ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-buttonWidth;
  if(right>ImGui::GetItemRectMax().x-ImGui::GetWindowPos().x+ImGui::GetStyle().ItemSpacing.x)ImGui::SameLine(right);else ImGui::SetCursorPosX(std::max(ImGui::GetStyle().WindowPadding.x,right));
  if(ImGui::Button("Skip onboarding")){tourFinish(true);}
  tourQaItem("skip");
  auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();tourSkipCenter={(a.x+b.x)*.5f,(a.y+b.y)*.5f};
  uiSectionHeading(titles[tourStep]);
  const char* task=tasks[tourStep];
  if(tourStep==tourClipStep&&tourClipChosen)task="Place two points on the top face with the left mouse button, then press Enter to cut the brush.";
  ImGui::TextWrapped("%s",uiTr(task));
  if(tourDemo)ImGui::TextUnformatted("Watch the demonstration...");else if(tourDone)ImGui::TextColored({.45f,.95f,.55f,1},"Done! Continue when you are ready.");else ImGui::TextUnformatted("Your turn");
  if(tourStep==tourViewportStep){
    const char* layouts[]={"1 view","2 views","3 views","4 views"};
    ImGui::SetNextItemWidth(135*scale);
    ImGui::BeginDisabled(!tourFixtureReady()||tourDone);
    const bool open=ImGui::BeginCombo("Viewport layout",uiTr(layouts[viewLayout-1]));tourQaItem("layout");
    if(open){
      for(int i=0;i<IM_ARRAYSIZE(layouts);i++){
        const bool selected=ImGui::Selectable(layouts[i],viewLayout==i+1);tourQaItem("layout/"+std::to_string(i+1));
        if(selected&&viewLayout!=i+1){viewSetLayout(i+1);tourChangedLayout=i>0;}
      }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
  }
  ImGui::ProgressBar((tourStep+(tourDone?1.f:0.f))/tourStepCount,{-1,3*scale},"");
  const float toolsY=ImGui::GetWindowPos().y+ImGui::GetWindowHeight()+8*scale;
  ImGui::End();ImGui::PopStyleVar();
  if(!tourActive)return;
  if(tourStep==tourViewportStep&&viewLayout>1){
    const char* names[]={"3D","YZ","XZ","XY"};
    auto draw=ImGui::GetBackgroundDrawList();
    for(int i=0;i<viewLayout;i++){
      const auto& pane=viewRects[i];ImVec2 p{pane.x+10*scale,std::max(pane.y+10*scale,toolsY)};
      if(p.y+ImGui::GetTextLineHeight()+8*scale>=pane.y+pane.h)continue;
      const auto size=ImGui::CalcTextSize(names[pane.type]);
      draw->AddRectFilled(p,{p.x+size.x+14*scale,p.y+size.y+8*scale},IM_COL32(24,26,30,230),4*scale);
      draw->AddText({p.x+7*scale,p.y+4*scale},IM_COL32_WHITE,names[pane.type]);
    }
  }
  if(tourDemo&&tourFixtureReady()&&tourStep==tourClipStep)tourClipDemo(now,scale);
  else if(tourDemo&&tourFixtureReady()){
    float elapsed=float(now-tourStepTime);float t=std::clamp((elapsed-1.4f)/1.8f,0.f,1.f);t=t*t*(3-2*t);
    if(!tourDemoPressed)tourDemoStart=tourAnchor(w,h);
    ImVec2 start=tourDemoStart,end{start.x+w*.12f,start.y};
    if(tourStep==1){start={w*.40f,h*.66f};end={w*.59f,h*.56f};}
    if(tourStep==3||tourStep==4)end={start.x,start.y-h*.14f};
    ImVec2 p{start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t};bool held=elapsed>=1.4f&&elapsed<3.2f;
    if(elapsed>=1.4f&&!tourDemoPressed){tourPointer("down",start);tourDemoPressed=true;}
    if(held)tourPointer("move",p);
    if(elapsed>=3.2f){tourPointer("up",end);command("ftebroom_tour_restore\nset ftebroom_shift 0\nset ftebroom_alt 0\nset ftebroom_tour_demo 0\nset ftebroom_tour_result 0\n");tourDemo=false;tourInputReady=now+.4;}
    tourDemoCursor(p,scale,held);
    if(tourStep>=3)ImGui::GetForegroundDrawList()->AddText({p.x+20*scale,p.y+50*scale},IM_COL32_WHITE,uiTr(tourStep==3?"Alt":"Left Shift"));
  }
  if(tourStep==5&&!tourWaitingForMap)tourWheelIndicator(w,h,now,scale);
  if(tourStep==tourClipStep||tourStep==tourToolsStep){
    ImGui::SetNextWindowPos({16*scale,toolsY});ImGui::SetNextWindowSize({w-32*scale,0});
    ImGui::Begin("Tools##lesson",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse);
    const float size=34*scale;
    for(int tool:{0,1,2,3,7,8,9,5,6}){
      if(tool&&ImGui::GetItemRectMax().x+size+ImGui::GetStyle().ItemSpacing.x<ImGui::GetWindowPos().x+ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x)ImGui::SameLine();
      const bool clip=tourStep==tourClipStep&&tool==2;
      ImGui::BeginDisabled(!clip||tourClipChosen||!tourFixtureReady()||now<tourInputReady);
      if(clip)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
      const bool choose=FteIcons::button(("##lessonTool"+std::to_string(tool)).c_str(),tool,{size,size});
      tourQaItem("tool/"+std::to_string(tool));
      if(clip){
        ImGui::PopStyleColor();ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),IM_COL32(241,161,70,255),5*scale,0,2*scale);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",uiTr("Clip"));
      }
      ImGui::EndDisabled();
      if(clip&&choose){
        tourClipChosen=tourDemo=true;tourStepTime=now;tourClipDemoAction=0;
        command("set ftebroom_clip_keep 0\nset ftebroom_tour_demo 1\nftebroom_tool_toggle clip\n");
      }
    }
    ImGui::End();
  }
  ImGui::SetNextWindowPos({w*.5f,h-18*scale},ImGuiCond_Always,{.5f,1});
  ImGui::Begin("##lessonNavigation",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoDocking);
  if((tourStep>=1&&tourStep<=4)||(tourStep==tourClipStep&&tourClipChosen&&!tourDemo)){if(ImGui::Button("Watch again"))tourPrepare(tourStep);ImGui::SameLine();}
  ImGui::BeginDisabled(!tourDone);if(ImGui::Button(tourStep==tourToolsStep?"Start editing":"Continue")){if(tourStep==tourToolsStep)tourFinish(false);else tourPrepare(tourStep+1);}a=ImGui::GetItemRectMin();b=ImGui::GetItemRectMax();tourNextCenter={(a.x+b.x)*.5f,(a.y+b.y)*.5f};tourQaItem("next");ImGui::EndDisabled();ImGui::End();
  if(tourActive)tourQaPublish(w,h);
}
