// SPDX-License-Identifier: GPL-2.0-or-later
#include <iomanip>
#include "ftebroom_uv_math.h"
static ImVec2 uvTestDragPoint;
static bool uvTestDragReady;
static double uvTestZoom;
static void graphicalUVEditor(float height)
{
  static UVFace face,start;
  static int epoch=-1,drag=0,subdivisions[2]={1,1};
  static double zoom=1;
  static UVPoint pan,origin,down,appliedOffset;
  static bool haveFace=false,shearVertical=true;
  static std::string lastCommand;
  uvTestDragReady=false;
  const bool valid=cvars->GetFloat("ftebroom_uv_face")!=0;
  if(!valid)
  {
    if(drag>=1 && drag<=5)command("ftebroom_uv cancel\n");
    drag=0;haveFace=false;face.points.clear();
    epoch=-1;
  }
  int nextEpoch=(int)cvars->GetFloat("ftebroom_uv_epoch");
  bool fit=false;
  if(valid&&nextEpoch!=epoch)
  {
    qhandle_t handle;int length=files->Open("data/ftebroom-uv.txt",&handle,1);
    std::string text;
    if(length>=0){if(length<=65536){text.resize(length);files->Read(handle,text.data(),length);}files->Close(handle);}
    std::istringstream in(text);UVFace next;int count=0;
    in>>std::quoted(next.material)>>next.model>>next.id>>next.face>>count>>next.width>>next.height;
    for(auto& v:next.matrix)in>>v;
    if(count>=3 && count<=64)
    {
      next.points.resize(count);for(auto& p:next.points)in>>p.x>>p.y;
      bool finite=std::isfinite(next.width)&&std::isfinite(next.height)&&next.width>0&&next.height>0;
      for(auto value:next.matrix)finite=finite&&std::isfinite(value)&&std::abs(value)<=1e12;
      for(auto point:next.points)finite=finite&&std::isfinite(point.x)&&std::isfinite(point.y);
      finite=finite&&std::abs(next.matrix[0]*next.matrix[4]-next.matrix[1]*next.matrix[3])>1e-12;
      if(in && finite)
      {
        bool same=haveFace && face.points.size()==next.points.size();
        if(same)for(size_t i=0;i<next.points.size();++i)if(uvLength(face.points[i]-next.points[i])>.01)same=false;
        fit=!same && !drag;face=std::move(next);haveFace=true;epoch=nextEpoch;
      }
    }
  }
  ImGui::BeginChild("UV canvas",{0,height},ImGuiChildFlags_Borders|ImGuiChildFlags_ResizeY,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
  if(!haveFace){
    const char* message=cvars->GetFloat("ftebroom_face_count")>1?"Multiple faces selected":"Select a face to preview UVs";
    const auto room=ImGui::GetContentRegionAvail(),position=ImGui::GetCursorPos();
    const auto label=ImGui::CalcTextSize(message);
    ImGui::SetCursorPos({position.x+std::max(0.f,(room.x-label.x)*.5f),position.y+std::max(0.f,(room.y-label.y)*.5f)});
    ImGui::TextDisabled("%s",message);
    ImGui::EndChild();
  }else{
  ImVec2 top=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();size.x=std::max(size.x,40.f);size.y=std::max(size.y,40.f);
  ImGui::InvisibleButton("UV interaction",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight|ImGuiButtonFlags_MouseButtonMiddle);
  // Reserve wheel input for the canvas instead of scrolling its inspector ancestors.
  ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
  ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
  bool hovered=ImGui::IsItemHovered();ImVec2 mouse=ImGui::GetIO().MousePos;
  UVPoint low=face.points[0],high=low,center;
  for(auto p:face.points){low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);center=center+p;}
  center=center*(1.0/face.points.size());
  if(fit){zoom=std::clamp(std::min(size.x/std::max(1.,high.x-low.x),size.y/std::max(1.,high.y-low.y))*.65,.01,64.);pan=center;origin=center;}
  auto world=[&](ImVec2 p)->UVPoint{return {(p.x-top.x-size.x*.5)/zoom+pan.x,(p.y-top.y-size.y*.5)/zoom+pan.y};};
  auto screen=[&](UVPoint p)->ImVec2{return {top.x+size.x*.5f+(float)((p.x-pan.x)*zoom),top.y+size.y*.5f+(float)((p.y-pan.y)*zoom)};};
  UVPoint cursor=world(mouse);
  if(hovered && !drag && ImGui::GetIO().MouseWheel!=0)
  {
    zoom=std::clamp(zoom*std::pow(1.2,ImGui::GetIO().MouseWheel),.01,64.);
    pan=pan+(cursor-world(mouse));cursor=world(mouse);
  }
  if(cvars->GetFloat("ftebroom_skip_setup"))uvTestZoom=zoom;
  if(hovered && !drag && (ImGui::IsMouseDragging(ImGuiMouseButton_Right)||ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
  {auto d=ImGui::GetIO().MouseDelta;pan=pan-UVPoint{d.x/zoom,d.y/zoom};cursor=world(mouse);}
  auto snapPoint=[&](UVPoint point){double best=4/zoom;UVPoint result=point;for(auto p:face.points)if(uvLength(p-point)<best){best=uvLength(p-point);result=p;}if(uvLength(center-point)<best)result=center;return result;};
  const double gridX=face.width/subdivisions[0],gridY=face.height/subdivisions[1];
  if(cvars->GetFloat("ftebroom_skip_setup") && !drag)
  {
    uvTestDragReady=false;auto pivot=uvMap(face.matrix,origin);
    for(int y=20;y<(int)size.y-30&&!uvTestDragReady;y+=11)for(int x=20;x<(int)size.x-30&&!uvTestDragReady;x+=11)
    {
      auto p=world({top.x+x,top.y+y}),uv=uvMap(face.matrix,p);
      double sx=zoom/std::max(1e-9,std::hypot(face.matrix[0],face.matrix[1])),sy=zoom/std::max(1e-9,std::hypot(face.matrix[3],face.matrix[4]));
      if(std::abs(uv.x-std::round(uv.x/gridX)*gridX)*sx>8 && std::abs(uv.y-std::round(uv.y/gridY)*gridY)*sy>8 &&
         std::abs(uv.x-pivot.x)*sx>8 && std::abs(uv.y-pivot.y)*sy>8 && std::abs(uvLength(p-origin)*zoom-65*uiScale())>10)
      {uvTestDragPoint={top.x+x,top.y+y};uvTestDragReady=true;}
    }
  }
  if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
  {
    start=face;down=cursor;appliedOffset={};lastCommand.clear();
    UVPoint uv=uvMap(start.matrix,cursor),o=uvMap(start.matrix,origin);
    double distU=std::abs(uv.x-std::round(uv.x/gridX)*gridX)/std::max(1e-9,std::hypot(start.matrix[0],start.matrix[1]))*zoom;
    double distV=std::abs(uv.y-std::round(uv.y/gridY)*gridY)/std::max(1e-9,std::hypot(start.matrix[3],start.matrix[4]))*zoom;
    double axisU=std::abs(uv.x-o.x)/std::max(1e-9,std::hypot(start.matrix[0],start.matrix[1]))*zoom;
    double axisV=std::abs(uv.y-o.y)/std::max(1e-9,std::hypot(start.matrix[3],start.matrix[4]))*zoom;
    double radius=uvLength(cursor-origin)*zoom;
    if(std::abs(radius-65*uiScale())<5)drag=2;
    else if(radius<8)drag=6;
    else if(axisU<4)drag=7;
    else if(axisV<4)drag=8;
    else if(std::min(distU,distV)<4){shearVertical=distU<distV;drag=ImGui::GetIO().KeyAlt?5:(distU<distV?3:4);}
    else drag=1;
    if(drag<=5)command("ftebroom_uv begin\n");
  }
  if(drag && !ImGui::IsKeyPressed(ImGuiKey_Escape) &&
     (uvLength(cursor-down)*zoom>.001 || !lastCommand.empty() || drag>=6) &&
     (ImGui::IsMouseDown(ImGuiMouseButton_Left)||ImGui::IsMouseReleased(ImGuiMouseButton_Left)))
  {
    if(drag>=6)
    {
      UVPoint snapped=snapPoint(cursor);
      if(drag==6)origin=snapped;
      else {auto a=uvMap(start.matrix,origin),b=uvMap(start.matrix,snapped);if(drag==7)a.x=b.x;else a.y=b.y;origin=uvInverse(start.matrix,a);}
    }
    else
    {
      double a=1,b=0,c=0,d=1,tx=0,ty=0;auto o=uvMap(start.matrix,origin),initial=uvMap(start.matrix,down),current=uvMap(start.matrix,cursor);
      if((drag==3 || drag==4) && !ImGui::GetIO().KeyCtrl) {
        double nearest=1e30;
        for(auto vertex:start.points) {
          auto candidate=uvMap(start.matrix,vertex);
          double distance=drag==3?candidate.x-current.x:candidate.y-current.y;
          if(std::abs(distance)<std::abs(nearest))nearest=distance;
        }
        double rowLength=drag==3?std::hypot(start.matrix[0],start.matrix[1]):std::hypot(start.matrix[3],start.matrix[4]);
        if(std::abs(nearest)<8*uiScale()/zoom*rowLength) {
          if(drag==3)current.x+=nearest;else current.y+=nearest;
        }
      }
      if(drag==1)
      {
        auto delta=uvDragOffset(start,down,cursor,{gridX,gridY},zoom,uiScale(),!ImGui::GetIO().KeyCtrl,appliedOffset);
        tx=delta.x;ty=delta.y;appliedOffset=delta;
      }
      else if(drag==2)
      {
        double angle=std::atan2(cursor.y-origin.y,cursor.x-origin.x)-std::atan2(down.y-origin.y,down.x-origin.x);
        double base=std::atan2(-start.matrix[1],start.matrix[0]);
        for(size_t i=0;i<face.points.size();++i)
        {
          auto edge=face.points[(i+1)%face.points.size()]-face.points[i];double target=std::atan2(edge.y,edge.x);
          double diff=std::remainder(target-base-angle,3.141592653589793);
          if(std::abs(diff)<.045){angle+=diff;break;}
        }
        a=d=std::cos(angle);b=std::sin(angle);c=-b;
      }
      else if(drag==3){if(std::abs(current.x-o.x)>.001)a=std::clamp((initial.x-o.x)/(current.x-o.x),-100.,100.);}
      else if(drag==4){if(std::abs(current.y-o.y)>.001)d=std::clamp((initial.y-o.y)/(current.y-o.y),-100.,100.);}
      else if(shearVertical && std::abs(current.y-o.y)>.001)b=std::clamp((initial.x-current.x)/(current.y-o.y),-100.,100.);
      else if(!shearVertical && std::abs(current.x-o.x)>.001)c=std::clamp((initial.y-current.y)/(current.x-o.x),-100.,100.);
      if(drag!=1){tx=o.x-a*o.x-b*o.y;ty=o.y-c*o.x-d*o.y;}
      if(std::abs(a*d-b*c)>1e-6)
      {
        char text[240];std::snprintf(text,sizeof text,"ftebroom_uv matrix %.9g %.9g %.9g %.9g %.9g %.9g\n",a,b,c,d,tx,ty);
        if(lastCommand!=text){command(text);lastCommand=text;}
        // Predict the same immutable-start transform while QC processes the command.
        const auto& m=start.matrix;
        face.matrix={a*m[0]+b*m[3],a*m[1]+b*m[4],a*m[2]+b*m[5]+tx,
                     c*m[0]+d*m[3],c*m[1]+d*m[4],c*m[2]+d*m[5]+ty};
      }
    }
  }
  if(drag && ImGui::IsKeyPressed(ImGuiKey_Escape)){if(drag<=5)command("ftebroom_uv cancel\n");face=start;drag=0;}
  if(drag && ImGui::IsMouseReleased(ImGuiMouseButton_Left)){if(drag<=5)command("ftebroom_uv commit\n");drag=0;}
  auto dl=ImGui::GetWindowDrawList();dl->PushClipRect(top,{top.x+size.x,top.y+size.y},true);
  dl->AddRectFilled(top,{top.x+size.x,top.y+size.y},IM_COL32(34,34,36,255));
  UVPoint uvLow{1e20,1e20},uvHigh{-1e20,-1e20};
  for(ImVec2 p:{top,ImVec2{top.x+size.x,top.y},ImVec2{top.x,top.y+size.y},ImVec2{top.x+size.x,top.y+size.y}})
  {auto q=uvMap(face.matrix,world(p));uvLow.x=std::min(uvLow.x,q.x);uvLow.y=std::min(uvLow.y,q.y);uvHigh.x=std::max(uvHigh.x,q.x);uvHigh.y=std::max(uvHigh.y,q.y);}
  auto textureName=face.material;if(textureName.rfind("textures/",0)!=0 && materials.count("textures/"+textureName))textureName="textures/"+textureName;
  auto image=(ImTextureID)thumbnail(textureName);
  int x0=(int)std::clamp(std::floor(uvLow.x/face.width),-1e8,1e8),x1=(int)std::clamp(std::ceil(uvHigh.x/face.width),-1e8,1e8),y0=(int)std::clamp(std::floor(uvLow.y/face.height),-1e8,1e8),y1=(int)std::clamp(std::ceil(uvHigh.y/face.height),-1e8,1e8);
  if((double)(x1-x0)*(y1-y0)<=1024)for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)
  {
    auto p0=screen(uvInverse(face.matrix,{x*face.width,y*face.height})),p1=screen(uvInverse(face.matrix,{(x+1)*face.width,y*face.height}));
    auto p2=screen(uvInverse(face.matrix,{(x+1)*face.width,(y+1)*face.height})),p3=screen(uvInverse(face.matrix,{x*face.width,(y+1)*face.height}));
    dl->AddImageQuad(image,p0,p1,p2,p3,{0,0},{1,0},{1,1},{0,1},IM_COL32(185,185,185,255));
  }
  auto line=[&](UVPoint a,UVPoint b,ImU32 color){dl->AddLine(screen(uvInverse(face.matrix,a)),screen(uvInverse(face.matrix,b)),color);};
  if((uvHigh.x-uvLow.x)/gridX<512)for(double x=std::ceil(uvLow.x/gridX)*gridX;x<uvHigh.x;x+=gridX)line({x,uvLow.y},{x,uvHigh.y},IM_COL32(150,150,150,150));
  if((uvHigh.y-uvLow.y)/gridY<512)for(double y=std::ceil(uvLow.y/gridY)*gridY;y<uvHigh.y;y+=gridY)line({uvLow.x,y},{uvHigh.x,y},IM_COL32(150,150,150,150));
  for(size_t i=0;i<face.points.size();++i)dl->AddLine(screen(face.points[i]),screen(face.points[(i+1)%face.points.size()]),IM_COL32_WHITE,2);
  auto originUV=uvMap(face.matrix,origin);line({originUV.x,uvLow.y},{originUV.x,uvHigh.y},IM_COL32(235,70,55,220));line({uvLow.x,originUV.y},{uvHigh.x,originUV.y},IM_COL32(235,70,55,220));
  dl->AddCircleFilled(screen(origin),5*uiScale(),IM_COL32(255,215,50,255));dl->AddCircle(screen(origin),65*uiScale(),IM_COL32(255,215,50,210),64);
  auto cuv=uvMap(face.matrix,center);line(cuv,cuv+UVPoint{20/zoom,0},IM_COL32(255,60,60,255));line(cuv,cuv+UVPoint{0,20/zoom},IM_COL32(60,255,60,255));
  dl->PopClipRect();ImGui::EndChild();
  }
  ImGui::BeginDisabled(cvars->GetFloat("ftebroom_face_count")==0);
  const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
  for(const auto& item:{std::array<const char*,3>{"Reset","reset 1 0","Reset texture alignment"},
      {"Flip H","fliph 1 0","Flip horizontally"},{"Flip V","flipv 1 0","Flip vertically"},
      {"-90","rotate -1 1","Rotate left 90 degrees"},{"+90","rotate 1 1","Rotate right 90 degrees"},
      {"Fit","", "Fit one texture repeat to the face"}}){
    const float width=ImGui::CalcTextSize(item[0]).x+2*ImGui::GetStyle().FramePadding.x;
    if(item[0]!=std::string_view("Reset")&&
       ImGui::GetItemRectMax().x+width+ImGui::GetStyle().ItemSpacing.x<right)
      ImGui::SameLine();
    ImGui::PushID(item[0]);
    if(ImGui::Button(item[0])){
      if(*item[1])action((std::string("ftebroom_texture_key ")+item[1]+"\n").c_str());
      else action("ftebroom_texture_align auto 1 0\n");
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",item[2]);
    ImGui::PopID();
  }
  ImGui::EndDisabled();
  const float gridWidth=127*uiScale();
  if(ImGui::GetItemRectMax().x+gridWidth+ImGui::GetStyle().ItemSpacing.x<right)
    ImGui::SameLine(right-ImGui::GetWindowPos().x-gridWidth);
  for(int i=0;i<2;++i){
    if(i)ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s",i?"Y":"Grid X");ImGui::SameLine();
    ImGui::SetNextItemWidth(28*uiScale());
    ImGui::InputInt(i?"##grid_y":"##grid_x",&subdivisions[i],0,0);
    subdivisions[i]=std::clamp(subdivisions[i],1,32);
  }
}
