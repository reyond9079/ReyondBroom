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
  const ImU32 gold=ImGui::GetColorU32(ImVec4(1.f,.78f,.28f,1.f));
  const ImU32 shade=ImGui::GetColorU32(ImVec4(.23f,.37f,.49f,1.f));
  const ImU32 cutFill=ImGui::GetColorU32(ImVec4(.30f,.52f,.70f,1.f));
  void ring(ImVec2 p, float radius) const
  {
    draw->AddCircle(point(p),radius*scale,accent,12,std::max(1.f,1.3f*scale));
  }
  void mark(ImVec2 p,ImU32 color,float radius) const
  {
    draw->AddCircleFilled(point(p),radius*scale,color,12);
  }

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
  case Tool::Move: {
    for(auto p:{ImVec2{5,2},ImVec2{21,13.5f},ImVec2{14.3f,14.1f},ImVec2{17.3f,20.6f},
                ImVec2{13.8f,22.1f},ImVec2{10.8f,15.8f},ImVec2{5.8f,20}})g.draw->PathLineTo(g.point(p));
    g.draw->PathFillConcave(g.accent);
    g.path({{5,2},{21,13.5f},{14.3f,14.1f},{17.3f,20.6f},{13.8f,22.1f},{10.8f,15.8f},{5.8f,20},{5,2}},g.ink,1.25f);
    break;
  }
  case Tool::Brush: {
    const ImVec2 a{3,14},b{8,5},c{18.6f,6.6f},d{21,14.7f},e{11.6f,19.5f};
    g.fill({a,b,c,d,e},g.shade);
    g.path({a,b,c,d,e},g.ink);
    g.path({{10,18.45f},{8.2f,17.3f}},g.accent,1.1f);
    g.path({{6.7f,16.35f},{5.1f,15.35f}},g.accent,1.1f);
    for(auto p:{a,b,c,d})g.mark(p,g.ink,1.25f);
    g.mark(e,g.accent,1.65f);
    break;
  }
  case Tool::Clip: {
    const ImVec2 left{1.8f,6.6f},right{16.2f,6.6f},back{9,2.8f};
    const ImVec2 leftLow{1.8f,10.5f},rightLow{16.2f,10.5f};
    g.fill({left,right,rightLow,leftLow},g.cutFill);
    g.path({leftLow,left,back,right,rightLow,leftLow},g.ink);
    g.path({left,right},g.ink);
    const ImVec2 nearLeft{1.8f,13},nearRight{16.2f,13},front{9,16.8f};
    const ImVec2 bottomLeft{1.8f,17.5f},bottomRight{16.2f,17.5f},bottom{9,21.3f};
    g.fill({nearLeft,nearRight,front},g.shade);
    g.path({nearLeft,nearRight,bottomRight,bottom,bottomLeft,nearLeft,front,nearRight},g.ink);
    g.path({front,bottom},g.ink);
    // The open blades meet at the gap between the two halves.
    g.path({{13.8f,10.2f},{19,12},{21.2f,14.6f}},g.accent,1.3f);
    g.path({{13.8f,13.8f},{19,12},{21.2f,9.4f}},g.accent,1.3f);
    g.ring({22,8.4f},1.5f);
    g.ring({22,15.6f},1.5f);
    break;
  }
  case Tool::Rotate: {
    g.block(12,7.9f,7.8f,6.4f);
    g.turn(3.1f);
    g.path({{12,1},{12,9.6f}},g.accent,1.15f);
    g.path({{12,17.1f},{12,23}},g.accent,1.15f);
    break;
  }
  case Tool::Sweep:
    g.block(8,12,5.3f,4.8f);
    g.path({{13,6},{18,3.4f},{22.6f,5.8f},{17.6f,8.4f},{13,6}},g.accent);
    g.arrow({9,12},{15.8f,7.8f});
    break;
  case Tool::Scale: {
    // Show the exposed edges of two larger boxes at the same projection.
    const auto silhouette=[&](float x,float y,float half,float depth,ImU32 color){
      const float rise=half*.52f;
      g.path({{x-half,y+rise},{x,y},{x+half,y+rise},
              {x+half,y+rise+depth},{x,y+2*rise+depth}},color,1.4f);
    };
    silhouette(13.2f,2,9.2f,10.8f,g.cutFill);
    silhouette(10.4f,7.3f,7.2f,7.8f,g.accent);
    g.block(7.5f,12.2f,4.8f,5.4f);
    break;
  }
  case Tool::Shear:
    g.block(10,7.3f,7.2f,6.8f,3.6f);
    g.arrow({8,3.5f},{20,3.5f});
    break;
  case Tool::Vertex: {
    g.block(12,2.7f,8.2f,11.5f);
    const ImVec2 hidden{12,14.2f};
    g.path({{6,17},{8.5f,16}},g.cutFill,1.05f);
    g.path({{9.7f,15.3f},hidden,{14.3f,15.3f}},g.cutFill,1.05f);
    g.path({{15.5f,16},{18,17}},g.cutFill,1.05f);
    for(auto p:{ImVec2{12,2.7f},ImVec2{3.8f,6.96f},ImVec2{12,11.23f},hidden,
                ImVec2{3.8f,18.46f},ImVec2{20.2f,18.46f},ImVec2{12,22.73f}})
      g.mark(p,g.gold,1.25f);
    g.mark({20.2f,6.96f},g.accent,1.65f);
    break;
  }
  case Tool::Edge:
    g.block();
    g.path({{12,12.45f},{12,21.45f}},g.accent,2.8f);
    break;
  case Tool::Face:
    g.block(12,3.5f,8.6f,9,0,2);
    break;
  case Tool::ControlPoint: {
    g.path({{2.7f,13},{7.2f,9.6f},{12.3f,6.7f},{17,6.8f},{21.4f,8.5f},{20,13},
            {18.7f,17.5f},{13.8f,16.1f},{9.2f,16.7f},{4.5f,19.3f},{3.6f,16.2f},{2.7f,13}},g.ink);
    g.path({{3.6f,16.2f},{8.2f,13.1f},{13,11.3f},{17,11.5f},{20,13}},g.ink,1.1f);
    g.path({{12.3f,6.7f},{13,11.3f},{13.8f,16.1f}},g.ink,1.1f);
    g.mark({8.2f,13.1f},g.accent,1.15f);
    g.mark({13,11.3f},g.accent,1.55f);
    g.mark({17,11.5f},g.accent,1.15f);
    break;
  }
  case Tool::Sculpt: {
    const ImVec2 back{12,8},right{22,14.5f},front{12,21},left{2,14.5f};
    const ImVec2 backLeft{7,11.25f},backRight{17,11.25f};
    const ImVec2 frontLeft{7,17.75f},frontRight{17,17.75f},hill{12,11.9f};
    g.fill({back,backRight,hill,backLeft},g.shade);
    g.fill({hill,frontRight,front,frontLeft},g.shade);
    g.path({back,right,front,left,back},g.ink);
    g.path({backLeft,hill,frontRight},g.ink,1.1f);
    g.path({backRight,hill,frontLeft},g.ink,1.1f);
    g.path({left,hill,right},g.ink,1.05f);
    g.path({{7,4.5f},{8.6f,3},{12,2.4f},{15.4f,3},{17,4.5f},
            {15.4f,6},{12,6.6f},{8.6f,6},{7,4.5f}},g.accent,1.5f);
    break;
  }
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
  case Tool::FlipHorizontal: {
    g.path({{12,2},{12,5}},g.accent);
    g.path({{12,7},{12,10}},g.accent);
    g.path({{12,12},{12,15}},g.accent);
    g.path({{12,17},{12,22}},g.accent);
    g.fill({{2.8f,7},{8.4f,11.5f},{8.4f,18},{2.8f,13.5f}},g.shade);
    g.path({{2.8f,7},{8.4f,11.5f},{8.4f,18},{2.8f,13.5f},{2.8f,7}},g.ink);
    g.path({{21.2f,7},{15.6f,11.5f},{15.6f,18},{21.2f,13.5f},{21.2f,7}},g.ink);
    g.arrow({7.9f,3.7f},{18.4f,3.7f});
    break;
  }
  case Tool::FlipVertical: {
    g.path({{2,12},{5,12}},g.accent);
    g.path({{7,12},{10,12}},g.accent);
    g.path({{12,12},{15,12}},g.accent);
    g.path({{17,12},{22,12}},g.accent);
    g.fill({{5.8f,7.7f},{10.3f,2.1f},{16.8f,2.1f},{12.3f,7.7f}},g.shade);
    g.path({{5.8f,7.7f},{10.3f,2.1f},{16.8f,2.1f},{12.3f,7.7f},{5.8f,7.7f}},g.ink);
    g.path({{5.8f,16.3f},{10.3f,21.9f},{16.8f,21.9f},{12.3f,16.3f},{5.8f,16.3f}},g.ink);
    g.arrow({21,7.9f},{21,18.4f});
    break;
  }
  }
}

static bool button(const char* id,Tool tool,ImVec2 size)
{
  const bool pressed=ImGui::Button(id,size);
  if(ImGui::IsItemVisible())drawTool(tool,ImGui::GetItemRectMin(),size);
  return pressed;
}
}
