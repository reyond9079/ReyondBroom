// SPDX-License-Identifier: GPL-2.0-or-later
namespace BrushIcons {
enum class Tool {
  Move, Brush, Clip, Rotate, Sweep, Scale, Shear, Vertex, Edge, Face,
  ControlPoint, Sculpt, Bend, PolygonCut, PolygonSpawn, Spin, RotateGrid,
  FlipHorizontal, FlipVertical
};

// Draw in a 24-unit square. Shared projection and line weight keep the
// silhouette consistent as each tool changes one part of the brush.
struct Glyph {
  ImDrawList* draw;
  ImVec2 origin;
  float scale;
  ImU32 ink, accent;

  ImVec2 point(ImVec2 p) const { return {origin.x+p.x*scale,origin.y+p.y*scale}; }
  void path(std::initializer_list<ImVec2> points,ImU32 color,float width=1.45f) const
  {
    for(auto p:points)draw->PathLineTo(point(p));
    draw->PathStroke(color,0,std::max(1.f,width*scale));
  }
  void fill(std::initializer_list<ImVec2> points,ImU32 color) const
  {
    for(auto p:points)draw->PathLineTo(point(p));
    draw->PathFillConvex(color);
  }
  void dot(ImVec2 p,float radius=2.1f) const
  {
    draw->AddCircleFilled(point(p),radius*scale,accent,12);
    draw->AddCircle(point(p),radius*scale,ink,12,std::max(1.f,scale));
  }
  void arrow(ImVec2 start,ImVec2 end) const
  {
    const float dx=end.x-start.x,dy=end.y-start.y,length=std::hypot(dx,dy);
    const ImVec2 stem{end.x-3.6f*dx/length,end.y-3.6f*dy/length};
    const ImVec2 wing{-2.1f*dy/length,2.1f*dx/length};
    path({start,stem},accent,1.7f);
    fill({end,{stem.x+wing.x,stem.y+wing.y},{stem.x-wing.x,stem.y-wing.y}},accent);
  }
  void block(float x=12,float y=3.5f,float half=8.6f,float depth=9,float shear=0,int highlight=0) const
  {
    const float rise=half*.52f;
    const ImVec2 back{x+shear,y},left{x-half+shear,y+rise},
      front{x+shear,y+2*rise},right{x+half+shear,y+rise};
    const ImVec2 bottomLeft{x-half,left.y+depth},bottom{x,front.y+depth},
      bottomRight{x+half,right.y+depth};
    if(highlight==1)fill({back,right,front,left},accent);
    if(highlight==2)fill({front,right,bottomRight,bottom},accent);
    path({back,right,bottomRight,bottom,bottomLeft,left,back},ink);
    path({left,front,right},ink);
    path({front,bottom},ink);
  }
  void turn(float y) const
  {
    path({{3,y+3},{5.5f,y+.8f},{10,y},{15,y+.2f},{19,y+1.5f},{21,y+3.4f}},accent);
    fill({{22.8f,y+5},{17.8f,y+4.1f},{21.2f,y+.2f}},accent);
  }
};

static void drawTool(Tool tool,ImVec2 min,ImVec2 size)
{
  const float extent=std::min(size.x,size.y)*.79f;
  const ImVec4 text=ImGui::GetStyle().Colors[ImGuiCol_Text];
  const bool lightInk=.2126f*text.x+.7152f*text.y+.0722f*text.z>.55f;
  const ImVec4 accent=lightInk?ImVec4(.48f,.78f,1.f,1.f):ImVec4(.04f,.34f,.64f,1.f);
  const Glyph g{ImGui::GetWindowDrawList(),
    {min.x+(size.x-extent)*.5f,min.y+(size.y-extent)*.5f},extent/24.f,
    ImGui::GetColorU32(ImGuiCol_Text),ImGui::GetColorU32(accent)};
  switch(tool){
  case Tool::Move:
    g.block(8.5f,8,6.2f,6);
    g.fill({{13,2},{22.2f,11},{17.8f,11},{16.5f,16},{13,12}},g.accent);
    g.path({{13,2},{22.2f,11},{17.8f,11},{16.5f,16},{13,12},{13,2}},g.ink,1.15f);
    break;
  case Tool::Brush:
    g.block(12,3.5f,8.6f,9,0,1);
    break;
  case Tool::Clip:
    g.block();
    g.path({{5,20},{19,3}},g.accent,2.35f);
    break;
  case Tool::Rotate:
    g.block(12,7,7.8f,7.2f);
    g.turn(2);
    break;
  case Tool::Sweep:
    g.block(8,12,5.3f,4.8f);
    g.path({{13,6},{18,3.4f},{22.6f,5.8f},{17.6f,8.4f},{13,6}},g.accent);
    g.arrow({9,12},{15.8f,7.8f});
    break;
  case Tool::Scale:
    g.block(9,9,6,6.5f);
    g.arrow({15.5f,8.5f},{22,2});
    break;
  case Tool::Shear:
    g.block(10,7.3f,7.2f,6.8f,3.6f);
    g.arrow({8,3.5f},{20,3.5f});
    break;
  case Tool::Vertex:
    g.block();
    g.dot({20.6f,8});
    break;
  case Tool::Edge:
    g.block();
    g.path({{12,12.45f},{12,21.45f}},g.accent,2.8f);
    break;
  case Tool::Face:
    g.block(12,3.5f,8.6f,9,0,2);
    break;
  case Tool::ControlPoint:
    g.block();
    g.path({{4.3f,10},{12,14},{19.7f,10}},g.accent,1.15f);
    for(auto point:{ImVec2{4.3f,10},ImVec2{12,14},ImVec2{19.7f,10}})g.dot(point,1.45f);
    break;
  case Tool::Sculpt:
    g.path({{3.5f,11},{8,8},{12,5},{16,8},{20.5f,11},{20.5f,17},{12,21},{3.5f,17},{3.5f,11},
            {12,15},{20.5f,11}},g.ink);
    g.path({{12,15},{12,21}},g.ink);
    g.arrow({12,11},{12,1.2f});
    break;
  case Tool::Bend:
    g.path({{3,15},{7,9},{12,6},{17,9},{21,15},{21,18},{17,12},{12,9},{7,12},{3,18},{3,15}},g.ink);
    g.path({{3,15},{7,9},{12,6},{17,9},{21,15}},g.accent);
    g.dot({12,6},1.6f);
    break;
  case Tool::PolygonCut:
    g.block();
    g.path({{3.5f,13},{7.4f,15.1f},{12,14},{16.4f,11.5f},{20.5f,12.5f}},g.accent,2.f);
    g.dot({7.4f,15.1f},1.35f);
    g.dot({16.4f,11.5f},1.35f);
    break;
  case Tool::PolygonSpawn:
    g.path({{3,13},{8,9},{13,10.5f},{17,8},{21,12},{15,16},{11,14.5f},{6,17},{3,13},
            {3,16.5f},{6,20.5f},{11,18},{15,19.5f},{21,15.5f},{21,12}},g.ink);
    g.path({{6,17},{6,20.5f}},g.ink);
    g.path({{15,16},{15,19.5f}},g.ink);
    g.path({{8,4.5f},{16,4.5f}},g.accent,1.8f);
    g.path({{12,1},{12,8}},g.accent,1.8f);
    break;
  case Tool::Spin:
    g.block(12,7,7.3f,7.5f);
    g.path({{12,1},{12,16}},g.accent);
    g.path({{4,14},{1.6f,16.4f},{3.8f,19.5f},{10,21.6f},{17,21.2f},{21.5f,18.3f}},g.accent);
    g.fill({{22.7f,16.4f},{17.8f,17.2f},{21.8f,21.1f}},g.accent);
    break;
  case Tool::RotateGrid:
    g.path({{2,15},{12,9},{22,15},{12,21},{2,15}},g.ink);
    g.path({{7,12},{17,18}},g.ink);
    g.path({{7,18},{17,12}},g.ink);
    g.turn(3);
    break;
  case Tool::FlipHorizontal:
    g.block(12,7,7.4f,6.8f);
    g.arrow({10,3.5f},{2,3.5f});
    g.arrow({14,3.5f},{22,3.5f});
    break;
  case Tool::FlipVertical:
    g.block(10,4,6.8f,9);
    g.arrow({21,10},{21,2});
    g.arrow({21,14},{21,22});
    break;
  }
}

static bool button(const char* id,Tool tool,ImVec2 size)
{
  const bool pressed=ImGui::Button(id,size);
  if(ImGui::IsItemVisible())drawTool(tool,ImGui::GetItemRectMin(),size);
  return pressed;
}
}
