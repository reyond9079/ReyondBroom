// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ftebroom_jam_style.h"

namespace {
bool jamModeActive()
{
  return jamEnabled() && jamLocal.open;
}

ImVec2 jamQaPlayButton, jamQaUploadButton, jamQaBrowseButton, jamQaAlphaButton;
enum class JamCompileButton { Upload, View, Edit };
JamCompileButton jamCompileButton()
{
  if (cvars->GetFloat("ftebroom_bsp_preview") || buildPreviewRequested) return JamCompileButton::Edit;
  return jamPublishedCurrent() ? JamCompileButton::View : JamCompileButton::Upload;
}

void jamViewportToolbar()
{
  if (!jamEditing() || !uiHasScene || modEmbedded || bbActive || modelViewerOpen) return;
  const ViewRect* pane = nullptr;
  for (int i=0;i<viewLayout;++i) {
    const auto& view = viewRects[i];
    if (view.type==0 && view.w>0 && view.h>0 && (!pane || i==viewFocus)) pane=&view;
  }
  if (!pane) return;
  JamWindowStyle style;
  const float u = uiScale();
  const auto compileButton = jamCompileButton();
  const char* label = compileButton==JamCompileButton::Edit ? jamText("Back to editing","Вернуться к редактированию") :
    compileButton==JamCompileButton::View ? jamText("View compiled map","Посмотреть скомпилированную карту") :
    jamLocal.canPublish ? jamText("Compile and upload map","Скомпилировать и загрузить карту") :
    jamText("Compile locally","Скомпилировать локально");
  const float width = ImGui::CalcTextSize(label).x+26*u;
  const char* play = jamText("Play","Играть");
  const char* browse = jamText("Browse Jam","На карту Jam");
  const float playWidth=ImGui::CalcTextSize(play).x+26*u;
  const float browseWidth=ImGui::CalcTextSize(browse).x+26*u;
  const float rowWidth=playWidth+width+browseWidth+2*ImGui::GetStyle().ItemSpacing.x;
  const bool showStatus = !jamLocal.error.empty() || !jamLocal.status.empty();
  const float panelWidth = std::min(pane->w-16*u,std::max(rowWidth,showStatus ? 292*u : rowWidth));
  if (panelWidth<width || pane->h<52*u) return;
  const bool singleRow=panelWidth>=rowWidth;
  const float viewOptionsClearance=ImGui::CalcTextSize("View Options").x+24*u;
  const float offset=singleRow && (pane->w-rowWidth)*.5f>=viewOptionsClearance ? 8*u : 42*u;
  ImGui::SetNextWindowPos({pane->x+(pane->w-panelWidth)*.5f,pane->y+offset});
  ImGui::SetNextWindowSizeConstraints({panelWidth,0},{panelWidth,pane->h-16*u});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
  constexpr auto flags = ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoDocking|
    ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoBringToFrontOnFocus|
    ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin("##jam-viewport-actions",nullptr,flags)) {
    const auto rememberButton = [](ImVec2& point) {
      if (std::getenv("REYONDBROOM_JAM_QA_STATE")) {
        const auto pos = ImGui::GetItemRectMin(), size = ImGui::GetItemRectSize();
        point = {pos.x+size.x*.5f,pos.y+size.y*.5f};
      }
    };
    const bool locked = jamBusy() || jamPublishing() || jamLocal.child || savingDocument ||
      runActive || runExclusive || buildRunning || assetsLoading || jamLocal.client.token.empty();
    const auto playButton = [&] {
      ImGui::BeginDisabled(locked || !jamLocal.canPublish);
      if(jamButton(play,{playWidth,0}))jamRequestUpload(true);
      rememberButton(jamQaPlayButton);
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s",jamLocal.canPublish
          ? jamText("Compile, publish and play this map","Собрать, опубликовать и запустить эту карту")
          : jamText("Publishing is temporarily closed.","Публикация временно закрыта."));
    };
    if(singleRow){
      ImGui::SetCursorPosX((panelWidth-rowWidth)*.5f);
      playButton();
      ImGui::SameLine();
    }else ImGui::SetCursorPosX((panelWidth-width)*.5f);
    ImGui::BeginDisabled(locked);
    const bool compileAction = jamButton(label,{width,0},JamTone::Action);
    rememberButton(jamQaUploadButton);
    if (compileAction) {
      if (compileButton==JamCompileButton::Edit) buildEdit();
      else if (compileButton==JamCompileButton::View) jamShowCompiled(jamLocal.publishedMap);
      else if (!jamLocal.canPublish) buildBegin();
      else jamRequestUpload();
    }
    ImGui::EndDisabled();
    if(singleRow)ImGui::SameLine();
    else{
      ImGui::SetCursorPosX(std::max(0.f,(panelWidth-playWidth-browseWidth-ImGui::GetStyle().ItemSpacing.x)*.5f));
      playButton();
      ImGui::SameLine();
    }
    if(jamButton(browse,{browseWidth,0}))jamLocal.open=true;
    rememberButton(jamQaBrowseButton);
    if (showStatus) {
      const bool error = !jamLocal.error.empty();
      const auto message = jamDisplayMessage(error ? jamLocal.error : jamLocal.status);
      const auto position = ImGui::GetCursorScreenPos();
      const auto font = ImGui::GetFontSize();
      const auto color = error ? IM_COL32(255,171,164,255) : IM_COL32(225,241,255,255);
      const auto wrap = panelWidth-16*u;
      const auto size = ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,wrap,message.c_str());
      auto* list = ImGui::GetWindowDrawList();
      list->AddRectFilled(position,{position.x+panelWidth,position.y+size.y+12*u},IM_COL32(13,24,41,240),5*u);
      list->AddText(ImGui::GetFont(),font,{position.x+8*u,position.y+6*u},color,message.c_str(),nullptr,wrap);
      ImGui::Dummy({panelWidth,size.y+12*u});
      if (error && jamLocal.inspectLocalBuild) {
        ImGui::BeginDisabled(locked);
        if (jamButton(jamText("Inspect compile locally","Проверить сборку локально"))) {
          jamLocal.inspectLocalBuild = false;
          buildBegin();
        }
        ImGui::EndDisabled();
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

void jamEditorMenu()
{
  if (!ImGui::BeginMenu("Mapping Jams")) return;
  if (ImGui::MenuItem(jamText("Jam map","Карта Jam"))) jamLocal.open = true;
  if (ImGui::MenuItem(jamText("Open my maps…","Открыть мои карты…"))) jamFileRequest(JamFileDialog::Open);
  if (ImGui::MenuItem(jamText("Quit Jam","Выйти из Jam"),nullptr,false,
                     !jamLocal.child&&!jamBusy()&&!jamPublishing()&&!savingDocument)) jamQuit();
  ImGui::EndMenu();
}

void jamOpenOwnMaps()
{
  jamFileRequest(JamFileDialog::Open);
}

void jamEditorDialog()
{
  if (jamLocal.fileDialog==JamFileDialog::None) return;
  JamWindowStyle style;
  const auto dialog = jamLocal.fileDialog;
  const char* popup = "###jam-map-files";
  const auto heading = dialog==JamFileDialog::Open ? jamText("Open a Jam map","Открыть карту Jam") :
    dialog==JamFileDialog::Confirm ? jamText("Save changes?","Сохранить изменения?") :
    dialog==JamFileDialog::Compile ? jamText("Save and compile","Сохранить и скомпилировать") :
    dialog==JamFileDialog::Upload ? jamText("Save, compile and upload","Сохранить, собрать и загрузить") :
    dialog==JamFileDialog::Play ? jamText("Save and play","Сохранить и играть") :
    jamText("Save Jam map","Сохранить карту Jam");
  const std::string window = std::string(heading)+popup;
  if (!ImGui::IsPopupOpen(popup)) ImGui::OpenPopup(popup);
  const auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(),ImGuiCond_Appearing,{.5f,.5f});
  ImGui::SetNextWindowSizeConstraints({std::min(450*uiScale(),viewport->Size.x),0},viewport->Size);
  if (!ImGui::BeginPopupModal(window.c_str(),nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)) return;
  bool close = false;
  if (dialog==JamFileDialog::Open) {
    try {
      const auto& maps = jamLocal.openMaps;
      JamFs::path chosen;
      if (maps.empty()) ImGui::TextUnformatted(jamText("You have no saved Jam maps.","У тебя пока нет сохранённых карт Jam."));
      if (!maps.empty() && ImGui::BeginChild("##jam-saved-maps",{0,std::min(270*uiScale(),viewport->Size.y*.5f)})) {
        for (const auto& map : maps) {
          ImGui::PushID(map.u8string().c_str());
          const auto label = map.stem().u8string()+"  ·  "+map.parent_path().filename().u8string()+"##open";
          const bool picked = jamButton(label.c_str(),{-1,0});
          ImGui::PopID();
          if (picked) {chosen=map;break;}
        }
      }
      if (!maps.empty()) ImGui::EndChild();
      if (!chosen.empty()) {jamOpenMap(chosen);close=true;}
    } catch (const std::exception& error) {jamLocal.fileError=error.what();}
  } else if (dialog==JamFileDialog::Confirm) {
    ImGui::TextUnformatted(jamText("The current Jam map has unsaved changes.","В текущей карте Jam есть несохранённые изменения."));
    if (jamButton(jamText("Save","Сохранить"),{},JamTone::Action)) {
      if (documentUntitled) jamFileRequest(JamFileDialog::Save);
      else close = documentSave(documentName,false,documentFile);
    }
    ImGui::SameLine();
    if (jamButton(jamText("Don't save","Не сохранять"))) {documentProceed();close=true;}
    ImGui::SameLine();
  } else {
    ImGui::TextUnformatted(jamText("Map name","Название карты"));
    if (jamLocal.dialogFocus) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1);
    const bool enter = ImGui::InputText("##jam-map-name",jamLocal.mapName,sizeof jamLocal.mapName,
                                      ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll);
    const bool save = jamButton(heading,{},JamTone::Action);
    if (enter || save) {
      try {close=jamSaveNamed();}
      catch (const std::exception& error) {jamLocal.fileError=error.what();}
    }
    ImGui::SameLine();
  }
  if (jamLocal.fileDialog==dialog) jamLocal.dialogFocus = false;
  const bool cancel = jamButton(jamText("Cancel","Отмена")) || ImGui::IsKeyPressed(ImGuiKey_Escape,false);
  if (!jamLocal.fileError.empty()) ImGui::TextWrapped("%s",jamDisplayMessage(jamLocal.fileError).c_str());
  if (cancel) {
    documentNext.clear();
    jamLocal.playAfterSave = false;
    jamLocal.publishPlayAfterSave = false;
  }
  if (close || cancel) {
    jamLocal.fileDialog = JamFileDialog::None;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

bool jamOffer()
{
  if (!jamEnabled()) return true;
  ImGui::TextUnformatted("Mapping Jams");
  ImGui::TextWrapped("%s", jamText("Build and play maps with the Jam community.", "Создавай и проходи карты участников Jam."));
  if (!ImGui::BeginTable("##jam-welcome", 2, ImGuiTableFlags_SizingStretchSame)) return false;
  ImGui::TableNextColumn();
  const bool join = jamButton(jamText("Join Mapping Jam (unstable alpha)","Участвовать в Mapping Jam (unstable alpha)"),
                              {-1,0},JamTone::Hub);
  if (join) jamLocal.open = true;
  ImGui::TableNextColumn();
  const bool offline = ImGui::Button(jamText("Work offline","Работать офлайн"), {-1,0});
  ImGui::EndTable();
  return join || offline;
}

void jamLimits()
{
  ImGui::TextUnformatted(jamText("Map limits","Лимиты карты"));
  const auto& size = jamLocal.rules.at("bounds").at("maxSize");
  ImGui::Text(jamText("Size: %d x %d x %d Quake units","Габариты: %d x %d x %d ед. Quake"),
              size[0].get<int>(), size[1].get<int>(), size[2].get<int>());
  ImGui::Text(jamText("Brushes: %d | Entities: %d","Браши: %d | Энтити: %d"),
              jamLocal.rules.value("maxBrushes",0), jamLocal.rules.value("maxEntities",0));
}

void jamCoin(ImDrawList* drawList, ImVec2 center, float radius, bool gift)
{
  const ImU32 rim=gift ? IM_COL32(215,175,252,255) : IM_COL32(255,225,136,255);
  const ImU32 face=gift ? IM_COL32(123,76,170,255) : IM_COL32(186,124,40,255);
  const ImU32 side=gift ? IM_COL32(57,31,88,255) : IM_COL32(97,60,22,255);
  const float stroke=std::max(1.f,radius*.12f);
  drawList->AddCircleFilled({center.x,center.y+radius*.25f},radius,side);
  drawList->AddCircleFilled(center,radius,face);
  drawList->AddCircle(center,radius,rim,0,stroke);
  drawList->AddCircle(center,radius*.75f,rim,0,stroke*.65f);
  if(gift){
    const float r=radius*.35f;
    drawList->AddRect({center.x-r,center.y-r*.3f},{center.x+r,center.y+r},rim,0,0,stroke);
    drawList->AddLine({center.x,center.y-r*.3f},{center.x,center.y+r},rim,stroke);
    drawList->AddTriangle({center.x,center.y-r*.3f},{center.x-r*.95f,center.y-r},{center.x-r*.35f,center.y-r},rim,stroke);
    drawList->AddTriangle({center.x,center.y-r*.3f},{center.x+r*.95f,center.y-r},{center.x+r*.35f,center.y-r},rim,stroke);
  }else{
    const float size=radius*.94f;
    const auto text=ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,0,"RB");
    drawList->AddText(ImGui::GetFont(),size,{center.x-text.x*.5f,center.y-size*.5f},rim,"RB");
  }
}

void jamWallet()
{
  if (!ImGui::BeginTable("##jam-wallet",2,ImGuiTableFlags_SizingStretchSame)) return;
  const char* labels[] = {jamText("Ordinary RB","Обычные RB"),jamText("Giftable RB","Подарочные RB")};
  const char* keys[] = {"spendableRb","giftableRb"};
  const JamTone tones[] = {JamTone::Hub,JamTone::Occupied};
  for (int i=0;i<2;++i) {
    ImGui::TableNextColumn();
    const auto balance = std::to_string(jamLocal.wallet.value(keys[i],0));
    const float line = ImGui::GetTextLineHeight();
    ImGui::Dummy({ImGui::GetContentRegionAvail().x,line*2.9f});
    const ImRect area{ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
    auto* list = ImGui::GetWindowDrawList();
    jamFace(list,area,tones[i],3*uiScale(),false,false,false);
    const float textWidth=std::min(ImGui::CalcTextSize(labels[i]).x,area.GetWidth()-line*2.7f);
    const float left=area.GetCenter().x-(textWidth+line*2.05f)*.5f;
    jamCoin(list,{left+line*.8f,area.Min.y+line*1.33f},line*.84f,i==1);
    const ImRect textArea{{left+line*2.05f,area.Min.y},{left+line*2.05f+textWidth,area.Max.y}};
    jamCenteredText(list,textArea,area.Min.y+line*.8f,labels[i],ImGui::GetFontSize(),ImGui::GetColorU32(ImGuiCol_Text));
    jamCenteredText(list,textArea,area.Min.y+line*1.97f,balance.c_str(),ImGui::GetFontSize()*1.22f,jamColors(tones[i]).accent);
  }
  ImGui::EndTable();
}

void jamAvatar(const std::string& url)
{
  const float size = 54*uiScale();
  const auto image = jamLocal.avatarTextures.find(url);
  if (image != jamLocal.avatarTextures.end() && image->second) {
    ImGui::Image((ImTextureID)image->second,{size,size});
  } else {
    const auto p = ImGui::GetCursorScreenPos();
    ImGui::Dummy({size,size});
    auto* canvas = ImGui::GetWindowDrawList();
    const auto color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    canvas->AddRectFilled(p,{p.x+size,p.y+size},ImGui::GetColorU32(ImGuiCol_FrameBg),size*.14f);
    canvas->AddCircleFilled({p.x+size*.5f,p.y+size*.34f},size*.12f,color);
    canvas->AddEllipseFilled({p.x+size*.5f,p.y+size*.70f},{size*.23f,size*.14f},color);
    if (!url.empty() && image == jamLocal.avatarTextures.end() && ImGui::IsItemVisible())
      jamLoadAvatar(url);
  }
}

void jamAvatarEditor()
{
  jamAvatar(jamLocal.avatarEdited ? jamLocal.pendingAvatarUrl : jamField(jamLocal.account,"avatarUrl"));
  ImGui::TextWrapped("%s",jamText("PNG, JPEG or WebP · up to 4 MiB","PNG, JPEG или WebP · до 4 МиБ"));
#ifdef _WIN32
  if (jamButton(jamText("Choose avatar…","Выбрать аватар…"))) {
    wchar_t buffer[4096]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof dialog;
    dialog.lpstrFile = buffer;
    dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
    dialog.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.webp)\0*.png;*.jpg;*.jpeg;*.webp\0\0";
    dialog.Flags = OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&dialog)) jamUploadAvatar(JamFs::path(buffer));
  }
#else
  ImGui::InputTextWithHint("##jam-avatar-path",jamText("Image file path","Путь к файлу изображения"),
                           jamLocal.avatarPath,sizeof jamLocal.avatarPath);
  ImGui::BeginDisabled(!*jamLocal.avatarPath);
  if (jamButton(jamText("Load avatar","Загрузить аватар"))) jamUploadAvatar(JamFs::u8path(jamLocal.avatarPath));
  ImGui::EndDisabled();
#endif
  ImGui::SameLine();
  if (jamButton(jamText("Remove avatar","Убрать аватар"))) {
    jamLocal.pendingAvatar.clear();
    jamLocal.pendingAvatarUrl.clear();
    jamLocal.avatarEdited = true;
  }
}

void jamHubEmblem(ImDrawList* list, ImVec2 center, float radius, ImU32 color)
{
  const auto diamond = [center](float r, float y) {
    return std::array<ImVec2,4>{{{center.x,center.y+y-r*.59f},{center.x+r,center.y+y},
      {center.x,center.y+y+r*.59f},{center.x-r,center.y+y}}};
  };
  const auto top = diamond(radius,0), bottom = diamond(radius,radius*.42f);
  const auto shade = IM_COL32(17,36,51,255);
  list->AddQuadFilled(top[3],top[2],bottom[2],bottom[3],shade);
  list->AddQuadFilled(top[2],top[1],bottom[1],bottom[2],IM_COL32(29,61,82,255));
  list->AddQuadFilled(top[0],top[1],top[2],top[3],IM_COL32(42,77,99,255));
  list->AddPolyline(top.data(),4,color,ImDrawFlags_Closed,uiScale()*1.6f);
  const ImVec2 lower[]{top[3],bottom[3],bottom[2],bottom[1],top[1]};
  list->AddPolyline(lower,5,color,0,uiScale()*1.25f);
  list->AddLine(top[2],bottom[2],color,uiScale()*1.25f);
  const auto inset = diamond(radius*.33f,0);
  list->AddPolyline(inset.data(),4,color,ImDrawFlags_Closed,uiScale());
}

void jamTileText(ImDrawList* list, const ImRect& area, float centerY, const std::string& text,
                 float fontSize, ImU32 color)
{
  const float inset = 9*uiScale();
  const float width = area.GetWidth()-2*inset;
  if (width<=0 || text.empty()) return;
  auto* font = ImGui::GetFont();
  const char* end = text.c_str()+text.size();
  if (font->CalcTextSizeA(fontSize,FLT_MAX,0,text.c_str(),end).x<=width) {
    jamCenteredText(list,area,centerY,text.c_str(),fontSize,color);
    return;
  }
  const char* limit = text.c_str();
  const float available = width-font->CalcTextSizeA(fontSize,FLT_MAX,0,"...").x;
  if (available<=0) return;
  font->CalcTextSizeA(fontSize,available,0,text.c_str(),end,&limit);
  jamCenteredText(list,area,centerY,(std::string(text.c_str(),limit)+"...").c_str(),fontSize,color);
}

void jamTile(const std::string& id, const char* title, const char* subtitle, ImVec2 size, JamTone tone,
             const std::string& mapTitle = {})
{
  const bool hub = tone == JamTone::Hub;
  if (ImGui::InvisibleButton(id.c_str(),size,ImGuiButtonFlags_EnableNav)) {
    jamLocal.selection = id;
    jamLocal.confirm.clear();
  }
  if (!ImGui::IsItemVisible()) return;
  const ImRect area{ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  const bool held = ImGui::IsItemActive();
  const float depth = std::min((hub ? 19.f : 13.f)*uiScale(),area.GetHeight()*.35f);
  auto* list = ImGui::GetWindowDrawList();
  const auto accent = jamColors(tone).accent;
  const float sizeY = area.GetHeight()-depth*1.24f, y = area.Min.y+jamPressOffset(depth,held);
  jamFace(list,area,tone,depth,ImGui::IsItemHovered(),held,jamLocal.selection==id || ImGui::IsItemFocused());
  if (id==jamLocal.purchasedPlot) {
    const float t=float(ImGui::GetTime()-jamLocal.purchasedAt)/1.1f;
    if(t>=0&&t<1){
      const float fade=(1-t)*(1-t);
      const auto center=area.GetCenter();
      const float spread=(.25f+.65f*t)*std::min(area.GetWidth(),area.GetHeight());
      list->PushClipRect(area.Min,area.Max,true);
      list->AddRectFilled(area.Min,area.Max,IM_COL32(131,255,215,int(46*fade)),8*uiScale());
      for(int i=0;i<8;++i){
        const float angle=float(i)*IM_PI/4;
        const ImVec2 dot{center.x+std::cos(angle)*spread,center.y+std::sin(angle)*spread};
        list->AddCircleFilled(dot,(2+2*fade)*uiScale(),IM_COL32(255,223,132,int(255*fade)),8);
      }
      list->PopClipRect();
    }
  }
  const float font = ImGui::GetFontSize();
  if (hub)
    jamHubEmblem(list,{area.GetCenter().x,y+sizeY*.31f},std::min(31*uiScale(),sizeY*.18f),accent);
  if (!mapTitle.empty()) {
    jamTileText(list,area,y+sizeY*.20f,title,std::min(font*.87f,sizeY*.25f),accent);
    jamTileText(list,area,y+sizeY*.48f,mapTitle,std::min(font,sizeY*.26f),ImGui::GetColorU32(ImGuiCol_Text));
    jamTileText(list,area,y+sizeY*.76f,subtitle,std::min(font*.81f,sizeY*.24f),accent);
    return;
  }
  jamCenteredText(list,area,y+sizeY*(hub?.59f:.32f),title,
                  std::min(font*(hub?1.75f:1.11f),sizeY*(hub?.25f:.44f)),ImGui::GetColorU32(ImGuiCol_Text));
  if (hub || area.GetWidth() > 36*uiScale())
    jamTileText(list,area,y+sizeY*(hub?.77f:.69f),subtitle,
                std::min(font*(hub?1.f:.83f),sizeY*.35f),accent);
}

void jamGrid()
{
  constexpr int columns = 5, rows = 5;
  constexpr const char* hubId = "C3";
  const float u = uiScale(), gap = 9*u;
  const float width = std::clamp(std::floor(ImGui::GetContentRegionAvail().x)-u,45*u,610*u);
  const float unit = (width-gap*(columns-1))/columns;
  const auto start = ImGui::GetCursorScreenPos();
  const float visibleHeight = ImGui::GetCurrentWindow()->ClipRect.Max.y-start.y;
  const float fit = (visibleHeight-gap*(rows-1)-8*u)/rows;
  const float rowHeight = std::max(38*u,std::min({unit*.8f,86*u,fit}));
  const auto tilePosition = [start,unit,rowHeight,gap](int column, int row) {
    return ImVec2(start.x+(unit+gap)*column,start.y+(rowHeight+gap)*row);
  };
  ImGui::PushID("jam-grid");
  for (const auto& cell : jamLocal.board.at("cells")) {
    const auto row = jamField(cell,"row");
    const int column = cell.at("column").get<int>();
    if (row.size()!=1 || row[0]<'A' || row[0]>'E' || column<1 || column>columns) continue;
    const auto id = row + std::to_string(column);
    const bool hub = jamField(cell,"kind") == "hub";
    if (hub) {
      if (id == hubId) {
        ImGui::SetCursorScreenPos(tilePosition(1,1));
        jamTile(hubId,"HUB",jamText("Reserved","Зарезервирован"),
                {unit*3+gap*2,rowHeight*3+gap*2},JamTone::Hub);
      }
      continue;
    }
    const auto& owner = cell.at("owner");
    const bool free = owner.is_null();
    const bool own = !free && jamField(owner,"accountId") == jamField(jamLocal.account,"accountId");
    const auto state = free ? jamText("Free plot","Свободный участок") :
      own ? jamText("My plot","Мой участок") : jamText("Occupied","Занят");
    const auto price = std::to_string(cell.value("priceRb",20)) + " RB";
    const auto nickname = jamField(owner,"nickname");
    const auto title = jamField(cell.at("publication"),"title");
    ImGui::SetCursorScreenPos(tilePosition(column-1,row[0]-'A'));
    jamTile(id,id.c_str(),free ? price.c_str() : nickname.empty() ? state : nickname.c_str(),
            {unit,rowHeight},free ? JamTone::Free : own ? JamTone::Mine : JamTone::Occupied,title);
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::TextUnformatted(state);
      if (!nickname.empty()) ImGui::TextUnformatted(nickname.c_str());
      if (!title.empty()) ImGui::TextUnformatted(title.c_str());
      ImGui::EndTooltip();
    }
  }
  ImGui::PopID();
  ImGui::SetCursorScreenPos(start);
  ImGui::Dummy({width,rows*rowHeight+(rows-1)*gap});
}

void jamPlot()
{
  const auto cell = jamCell();
  if (cell.is_null()) {
    ImGui::TextWrapped("%s", jamText("Choose a plot.","Выбери участок."));
    return;
  }
  if (jamField(cell,"kind") == "hub") {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(jamColors(JamTone::Hub).accent),"Hub B2-D4");
    ImGui::TextWrapped("%s",jamText("Reserved for the Jam. Purchase is unavailable.","Зарезервирован для Jam. Покупка недоступна."));
    ImGui::BeginDisabled(jamLocal.child!=0);
    if (jamButton(jamText("Play Hub","Играть в HUB"),{},JamTone::Action)) jamPlay(true);
    ImGui::EndDisabled();
    if (jamEditing()) {
      if (!jamLocal.gameDraft.empty() && jamLocal.child)
        ImGui::TextWrapped("%s",jamText("Press F10 in the Hub to return to editing.","Нажми F10 в HUB, чтобы вернуться к редактированию."));
      ImGui::BeginDisabled(jamLocal.child!=0);
      if (jamButton(jamText("Return to editing","Вернуться к редактированию"))) jamResumeDraft();
      ImGui::EndDisabled();
    }
    return;
  }
  ImGui::Text(jamText("Plot %s · 1 x 1","Участок %s · 1 x 1"),jamLocal.selection.c_str());
  const auto& owner = cell.at("owner");
  const auto& publication = cell.at("publication");
  if(!owner.is_null()){
    const auto count=cell.value("rbLikes",0);
    const bool own=jamField(owner,"accountId")==jamField(jamLocal.account,"accountId");
    const std::string label="RB Like  "+std::to_string(count)+"##rb-like";
    ImGui::BeginDisabled(own||owner.value("restricted",false)||jamLocal.wallet.value("giftableRb",0)==0);
    if(jamButton(label.c_str(),{ImGui::CalcTextSize(label.c_str(),nullptr,true).x+48*uiScale(),0},JamTone::Occupied))
      jamLike(jamField(cell,"plotId"));
    const auto corner = ImGui::GetItemRectMin();
    const float u = uiScale(), x = corner.x+13*u, y = corner.y+ImGui::GetItemRectSize().y*.37f;
    auto* draw = ImGui::GetWindowDrawList();
    const auto heart = ImGui::GetColorU32(ImGuiCol_Text);
    draw->AddCircleFilled({x-2.7f*u,y-1.2f*u},3.3f*u,heart,12);
    draw->AddCircleFilled({x+2.7f*u,y-1.2f*u},3.3f*u,heart,12);
    draw->AddTriangleFilled({x-5.5f*u,y},{x+5.5f*u,y},{x,y+6*u},heart);
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip("%s",jamText("Gift 1 RB to this plot's owner","Подарить 1 RB владельцу участка"));
  }
  if (owner.is_null()) {
    jamLimits();
    ImGui::TextUnformatted(jamText("Price: 20 ordinary RB","Цена: 20 обычных RB"));
    ImGui::BeginDisabled(jamLocal.wallet.value("spendableRb",0)<20);
    if (jamButton(jamText("Buy for 20 RB","Купить за 20 RB"))) jamLocal.confirm = "purchase";
    ImGui::EndDisabled();
  } else {
    const bool playable = publication.is_object() && publication.value("playable",false);
    if (playable) ImGui::TextWrapped("%s",jamField(publication,"title").c_str());
    else if (!publication.is_null()) ImGui::TextUnformatted(jamText("Map unavailable","Карта недоступна"));
    else ImGui::TextUnformatted(jamText("No published map","Карта не опубликована"));
    if(playable){
      ImGui::BeginDisabled(jamLocal.child!=0);
      if(jamButton(jamText("Play","Играть"),{},JamTone::Action))jamPlay();
      ImGui::EndDisabled();
    }
    if (jamField(owner,"accountId") == jamField(jamLocal.account,"accountId")) {
      jamLimits();
      if (jamButton(publication.is_null() ? jamText("Create or continue map","Создать или продолжить карту")
                                             : jamText("Edit map","Редактировать карту"))) jamOpenDraft();
      const bool active = jamLocal.selection == jamLocal.draftPlot && !documentUntitled && !documentFile.empty();
      if (active) ImGui::TextWrapped("%s",documentName.c_str());
      ImGui::BeginDisabled(!active || !jamLocal.operation.empty());
      if (jamButton(publication.is_null() ? jamText("Save to plot","Сохранить в участок")
                                            : jamText("Replace published map","Заменить опубликованную карту")))
        jamLocal.confirm = "publish";
      ImGui::EndDisabled();
    } else {
      ImGui::TextWrapped("%s",jamField(owner,"nickname").c_str());
      if (ImGui::CollapsingHeader(jamText("Owner profile","Профиль владельца"))) {
        jamAvatar(jamField(owner,"avatarUrl"));
        ImGui::TextWrapped("%s",jamField(owner,"bio").c_str());
      }
      ImGui::BeginDisabled(owner.value("restricted",false));
      if (jamButton(jamText("Gift RB","Подарить RB"))) jamLocal.confirm = "gift";
      ImGui::EndDisabled();
      if (jamButton(jamText("Report…","Пожаловаться…"))) {
        jamLocal.confirm = "report";
        jamLocal.reportTarget = playable ? 1 : 0;
        jamLocal.reportReason = 0;
        jamLocal.reportNote[0] = '\0';
      }
    }
  }
}

void jamConfirmation()
{
  if (jamLocal.confirm.empty()) return;
  const auto cell = jamCell();
  if (cell.is_null()) { jamLocal.confirm.clear(); return; }
  ImGui::Separator();
  if (jamLocal.confirm == "purchase") {
    ImGui::TextWrapped(jamText("Buy %s for 20 ordinary RB?","Купить %s за 20 обычных RB?"),jamLocal.selection.c_str());
    if (jamButton(jamText("Confirm purchase","Подтвердить покупку"),{},JamTone::Action)) {
      jamLocal.confirm.clear();
      jamPurchase();
      return;
    }
  } else if (jamLocal.confirm == "gift") {
    const int available = jamLocal.wallet.value("giftableRb",0);
    ImGui::TextWrapped(jamText("Giftable RB available: %d","Доступно подарочных RB: %d"), available);
    ImGui::InputInt(jamText("Gift amount","Сумма подарка"),&jamLocal.gift);
    ImGui::TextWrapped("%s",jamText("The recipient receives ordinary RB.","Получатель получит обычные RB."));
    ImGui::BeginDisabled(jamLocal.gift <= 0 || jamLocal.gift > available || cell["owner"].is_null());
    const bool send = jamButton(jamText("Confirm gift","Подтвердить подарок"));
    ImGui::EndDisabled();
    if (send) {
      jamLocal.confirm.clear();
      jamGift(jamField(cell["owner"],"accountId"));
      return;
    }
  } else if (jamLocal.confirm == "report") {
    const char* targets[] = {jamText("Plot","Участок"),jamText("Published map","Опубликованная карта"),jamText("Profile","Профиль")};
    const char* reasons[] = {jamText("Inappropriate content","Недопустимый контент"),jamText("Map does not work","Карта не работает"),
                             jamText("Spam","Спам"),jamText("Other","Другое")};
    static constexpr const char* codes[] = {"inappropriate","broken_map","spam","other"};
    ImGui::Combo(jamText("Report target","Объект жалобы"),&jamLocal.reportTarget,targets,3);
    ImGui::Combo(jamText("Reason","Причина"),&jamLocal.reportReason,reasons,4);
    ImGui::InputTextMultiline(jamText("Comment (optional)","Комментарий (необязательно)"),
                              jamLocal.reportNote,sizeof jamLocal.reportNote,{-1,65*uiScale()});
    const auto& publication = cell.at("publication");
    const bool canReport = jamLocal.reportTarget != 1 || (publication.is_object() && publication.value("playable",false));
    ImGui::BeginDisabled(!canReport);
    const bool send = jamButton(jamText("Submit report","Отправить жалобу"));
    ImGui::EndDisabled();
    if (send) {
      const auto kind = jamLocal.reportTarget==0 ? "plot" : jamLocal.reportTarget==1 ? "revision" : "profile";
      const auto target = jamLocal.reportTarget==0 ? jamLocal.selection : jamLocal.reportTarget==1
        ? jamField(publication,"revisionId") : jamField(cell.at("owner"),"accountId");
      jamLocal.confirm.clear();
      jamReport(kind,target,codes[jamLocal.reportReason]);
      return;
    }
  } else if (jamLocal.confirm == "publish") {
    ImGui::TextWrapped("%s",jamText("Other participants can play this version after validation.",
                                    "После проверки другие участники смогут сыграть в эту версию."));
    if (jamButton(jamText("Publish","Опубликовать"),{},JamTone::Action)) {
      jamLocal.confirm.clear();
      jamBeginPublish(jamLocal.selection);
      return;
    }
  }
  if (jamButton(jamText("Cancel##jam-confirm","Отмена##jam-confirm"))) jamLocal.confirm.clear();
}

void jamPlayers()
{
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(jamColors(JamTone::Hub).accent),
                     "%s",jamText("Players with plots","Владельцы участков"));
  const auto fresh = std::chrono::steady_clock::now()-jamLocal.lastPresence < std::chrono::seconds(40);
  if (jamLocal.lastPresence==std::chrono::steady_clock::time_point{}) {
    ImGui::TextDisabled("%s",jamText("Loading player status…","Загрузка статусов игроков…"));
    return;
  }
  if (jamLocal.players.empty())
    ImGui::TextDisabled("%s",jamText("No plots have been claimed yet.","Участки пока никто не занял."));
  if (!ImGui::BeginTable("##jam-players",2,ImGuiTableFlags_RowBg)) return;
  ImGui::TableSetupColumn("##player",ImGuiTableColumnFlags_WidthStretch);
  const float statusWidth = 14*uiScale()+std::max(ImGui::CalcTextSize(jamText("Online","Онлайн")).x,
                                                 ImGui::CalcTextSize(jamText("Offline","Офлайн")).x);
  ImGui::TableSetupColumn("##presence",ImGuiTableColumnFlags_WidthFixed,statusWidth);
  for (const auto& player : jamLocal.players) {
    const bool online = fresh && player.value("online",false);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s",jamField(player,"nickname").c_str());
    ImGui::TableNextColumn();
    const auto point = ImGui::GetCursorScreenPos();
    const auto color = online ? IM_COL32(99,224,154,255) : IM_COL32(148,158,173,255);
    ImGui::GetWindowDrawList()->AddCircleFilled({point.x+4*uiScale(),point.y+ImGui::GetTextLineHeight()*.5f},
                                                3.5f*uiScale(),color);
    ImGui::SetCursorScreenPos({point.x+14*uiScale(),point.y});
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(color),"%s",
                       online ? jamText("Online","Онлайн") : jamText("Offline","Офлайн"));
  }
  ImGui::EndTable();
}

void jamBoard()
{
  jamWallet();
  if (jamButton(jamText("Refresh","Обновить"))) jamRefresh();
  ImGui::SameLine();
  if (jamButton(jamText("My profile","Мой профиль"))) jamBeginProfile(false);
  ImGui::SameLine();
  if (jamButton(jamText("My maps","Мои карты"))) jamOpenOwnMaps();
  ImGui::SameLine();
  ImGui::BeginDisabled(!jamLocal.operation.empty());
  if (jamButton(jamText("Sign out","Выйти"))) jamSignOut();
  ImGui::EndDisabled();
  if (ImGui::GetContentRegionAvail().x > 695*uiScale() &&
      ImGui::BeginTable("##jam-board-layout",2,ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("##jam-board",ImGuiTableColumnFlags_WidthStretch,1.65f);
    ImGui::TableSetupColumn("##jam-plot",ImGuiTableColumnFlags_WidthStretch,1.f);
    ImGui::TableNextColumn(); jamGrid();
    ImGui::TableNextColumn(); jamPlayers(); ImGui::Separator(); jamPlot(); jamConfirmation();
    ImGui::EndTable();
  } else {
    jamGrid();
    jamPlayers();
    ImGui::Separator();
    jamPlot();
    jamConfirmation();
  }
}

const char* jamHeading()
{
  switch (jamLocal.screen) {
    case JamScreen::Register: return jamText("Create an account","Создать аккаунт");
    case JamScreen::Verify: return jamText("Confirm email","Подтвердить почту");
    case JamScreen::Recover:
    case JamScreen::Reset: return jamText("Recover your account","Восстановить доступ");
    case JamScreen::Profile: return jamText("Player profile","Профиль игрока");
    case JamScreen::Resources: return jamText("Game resources","Ресурсы игры");
    case JamScreen::Board: return jamText("Jam map","Карта Jam");
    default: return jamText("Join the Jam","Вход в Jam");
  }
}

void jamBanner()
{
  const float line = ImGui::GetTextLineHeight();
  const bool local = jamEndpoint().hostname.empty();
  ImGui::Dummy({std::max(1.f,ImGui::GetContentRegionAvail().x),line*(local ? 2.9f : 2.15f)});
  const ImRect area{ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};
  auto* list = ImGui::GetWindowDrawList();
  jamCenteredText(list,area,area.Min.y+line*.93f,jamHeading(),ImGui::GetFontSize()*1.80f,
                  ImGui::GetColorU32(ImGuiCol_Text));
  if (local) jamCenteredText(list,area,area.Min.y+line*2.16f,
    jamText("Local development server","Локальный сервер разработки"),ImGui::GetFontSize()*.9f,
    ImGui::GetColorU32(ImGuiCol_TextDisabled));
}

void jamBackdrop(const ImGuiViewport& viewport)
{
  const auto tl = viewport.Pos;
  const ImVec2 br{tl.x+viewport.Size.x,tl.y+viewport.Size.y};
  auto* list = ImGui::GetWindowDrawList();
  list->PushClipRect(tl,br,false);
  list->AddRectFilledMultiColor(tl,br,IM_COL32(16,30,52,255),IM_COL32(27,41,66,255),
    IM_COL32(9,15,27,255),IM_COL32(8,12,21,255));
  const float step = std::max(38.f,46*uiScale());
  for (float x=tl.x+step;x<br.x;x+=step)
    list->AddLine({x,tl.y},{x,br.y},IM_COL32(125,170,217,11));
  for (float y=tl.y+step;y<br.y;y+=step)
    list->AddLine({tl.x,y},{br.x,y},IM_COL32(125,170,217,11));
  list->PopClipRect();
}

void jamAuthField(const char* label, char* buffer, size_t size, ImGuiInputTextFlags flags = 0)
{
  ImGui::PushID(label);
  ImGui::TextUnformatted(label);
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputText("##value",buffer,size,flags);
  ImGui::PopID();
}

void jamAuthForm()
{
  const float u = uiScale();
  const auto screen = jamLocal.screen;
  const float width = std::min(ImGui::GetContentRegionAvail().x,390*u);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(ImGui::GetContentRegionAvail().x-width)*.5f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.081f,.12f,.192f,1));
  ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.29f,.44f,.58f,.63f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,12*u);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16*u,14*u));
  if(ImGui::BeginChild("##jam-auth",{width,0},ImGuiChildFlags_Borders|ImGuiChildFlags_AutoResizeY|
                       ImGuiChildFlags_AlwaysAutoResize,ImGuiWindowFlags_NoScrollbar)) {
    const auto action = [](const char* label, JamTone tone = JamTone::Neutral) {
      return jamButton(label,{-FLT_MIN,0},tone);
    };
    const auto secondary = [](const char* label, const char* other, bool& first, bool& second) {
      const float spacing = ImGui::GetStyle().ItemSpacing.x;
      const float available = ImGui::GetContentRegionAvail().x;
      const float half = (available-spacing)*.5f;
      const bool sideBySide = half >= std::max(ImGui::CalcTextSize(label).x,ImGui::CalcTextSize(other).x)+20*uiScale();
      const ImVec2 size{sideBySide ? half : available,0};
      first = jamButton(label,size);
      if (sideBySide) ImGui::SameLine();
      second = jamButton(other,size);
    };
    const auto back = jamText("Back to sign in","Назад ко входу");
    const bool registration = screen == JamScreen::Register;
    if (screen == JamScreen::SignIn || registration) {
      jamAuthField(jamText("Email","Почта"),jamLocal.email,sizeof jamLocal.email);
      if (registration) jamAuthField(jamText("Nickname","Ник"),jamLocal.nickname,sizeof jamLocal.nickname);
      jamAuthField(jamText("Password","Пароль"),jamLocal.password,sizeof jamLocal.password,
                   jamLocal.showPassword ? 0 : ImGuiInputTextFlags_Password);
      ImGui::Checkbox(jamText("Show password","Показать пароль"),&jamLocal.showPassword);
      if (registration) {
        ImGui::TextWrapped("%s",jamPasswordRules());
        if (action(jamText("Register","Зарегистрироваться"),JamTone::Action)) jamRegister();
        bool generate, signIn;
        secondary(jamText("Generate password","Сгенерировать пароль"),back,generate,signIn);
        if (generate) jamAssign(jamLocal.password,sizeof jamLocal.password,jam::randomHex(16));
        if (signIn) jamLocal.screen = JamScreen::SignIn;
      } else {
        const auto remember = jamText("Remember me","Запомнить вход");
        if (ImGui::GetContentRegionAvail().x > ImGui::GetFrameHeight()+ImGui::CalcTextSize(remember).x+12*u)
          ImGui::SameLine(0,14*u);
        ImGui::Checkbox(remember,&jamLocal.rememberSession);
        if (action(jamText("Sign in","Войти"),JamTone::Action)) jamLogin();
        if (action(jamText("Create account","Создать аккаунт"))) jamLocal.screen = JamScreen::Register;
        bool forgot, verify;
        secondary(jamText("Forgot password?","Забыли пароль?"),jamText("Enter confirmation code","Ввести код подтверждения"),forgot,verify);
        if (verify) jamLocal.screen = JamScreen::Verify;
        if (forgot) {
          jamLocal.password[0] = jamLocal.code[0] = '\0';
          jamLocal.screen = JamScreen::Recover;
        }
      }
    } else if (screen == JamScreen::Verify) {
      const bool remote = !jamEndpoint().hostname.empty();
      ImGui::TextWrapped("%s",remote
        ? jamText("Enter the confirmation code from your email.","Введите код подтверждения из письма.")
        : jamText("Enter the code from the local server mail folder.","Введи код из почтовой папки локального сервера."));
      jamAuthField(jamText("Code","Код"),jamLocal.code,sizeof jamLocal.code);
      if (action(jamText("Confirm email","Подтвердить почту"),JamTone::Action)) jamVerify();
      if (remote) {
        jamAuthField(jamText("Email","Почта"),jamLocal.email,sizeof jamLocal.email);
        ImGui::BeginDisabled(!jamLocal.email[0]);
        const bool resend = action(jamText("Resend code","Отправить код повторно"));
        ImGui::EndDisabled();
        if (resend) jamResendConfirmation();
      }
      if (action(back)) jamLocal.screen = JamScreen::SignIn;
    } else if (screen == JamScreen::Recover) {
      jamAuthField(jamText("Email","Почта"),jamLocal.email,sizeof jamLocal.email);
      if (action(jamText("Request recovery code","Запросить код восстановления"),JamTone::Action)) jamForgotPassword();
      bool code, signIn;
      secondary(jamText("I already have a code","У меня уже есть код"),back,code,signIn);
      if (code) jamLocal.screen = JamScreen::Reset;
      if (signIn) jamLocal.screen = JamScreen::SignIn;
    } else if (screen == JamScreen::Reset) {
      if (!jamEndpoint().hostname.empty())
        jamAuthField(jamText("Email","Почта"),jamLocal.email,sizeof jamLocal.email);
      jamAuthField(jamText("Recovery code","Код восстановления"),jamLocal.code,sizeof jamLocal.code);
      jamAuthField(jamText("New password","Новый пароль"),jamLocal.password,sizeof jamLocal.password,ImGuiInputTextFlags_Password);
      ImGui::TextWrapped("%s",jamPasswordRules());
      jamAuthField(jamText("Confirm new password","Повтори новый пароль"),jamLocal.passwordConfirmation,
                   sizeof jamLocal.passwordConfirmation,ImGuiInputTextFlags_Password);
      if (action(jamText("Reset password","Сменить пароль"),JamTone::Action)) jamResetPassword();
      if (action(jamText("Back","Назад"))) {
        jamLocal.password[0] = jamLocal.passwordConfirmation[0] = jamLocal.code[0] = '\0';
        jamLocal.screen = JamScreen::Recover;
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void jamAlphaWarning(float left, float lane)
{
  const float u = uiScale();
  const float width = std::min(lane,480*u);
  ImGui::SetCursorPosX(left+(lane-width)*.5f);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY()+std::max(0.f,(ImGui::GetContentRegionAvail().y-210*u)*.4f));
  ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.081f,.12f,.192f,1));
  ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.29f,.44f,.58f,.63f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,12*u);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(20*u,18*u));
  if (ImGui::BeginChild("##jam-alpha-warning",{width,0},ImGuiChildFlags_Borders|
                        ImGuiChildFlags_AutoResizeY|ImGuiChildFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(jamColors(JamTone::Hub).accent),"%s",
      jamText("EARLY ALPHA","РАННЯЯ АЛЬФА"));
    ImGui::Spacing();
    ImGui::TextWrapped("%s",jamText(
      "Mapping Jam is in early development. All of your online progress may be lost in future updates.",
      "Mapping Jam находится на ранней стадии разработки. В будущих обновлениях весь твой онлайн-прогресс может быть потерян."));
    ImGui::Spacing();
    if (jamButton(jamText("Agree and continue","Согласен, продолжить"),{-FLT_MIN,0},JamTone::Action))
      jamAcceptAlphaWarning();
    if (std::getenv("REYONDBROOM_JAM_QA_STATE"))
      jamQaAlphaButton = ImRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax()).GetCenter();
    if (!jamLocal.error.empty()) ImGui::TextWrapped("%s",jamDisplayMessage(jamLocal.error).c_str());
  }
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(2);
}

void jamPanel()
{
  const auto* viewport = ImGui::GetMainViewport();
  const float u = uiScale();
  JamWindowStyle style;
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::SetNextWindowSize(viewport->Size,ImGuiCond_Always);
  ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoDecoration|
    ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse;
  if (ImGui::Begin("Mapping Jams###mapping-jams",nullptr,flags)) {
    jamBackdrop(*viewport);
    const float available = ImGui::GetContentRegionAvail().x;
    const float lane = std::max(1.f,std::min(available,1120*u));
    const float left = ImGui::GetCursorPosX()+(available-lane)*.5f;
    ImGui::SetCursorPosX(left);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(jamColors(JamTone::Hub).accent),"MAPPING JAMS");
    const bool showAlphaWarning = !jamAlphaWarningAccepted();
    const bool board = !showAlphaWarning && jamLocal.screen==JamScreen::Board;
    const auto* exit = board ? jamText("Quit Jam","Выйти из Jam") : jamText("Back to editor","В редактор");
    const float exitWidth = ImGui::CalcTextSize(exit).x+26*u;
    if (lane > exitWidth+ImGui::CalcTextSize("MAPPING JAMS").x+12*u)
      ImGui::SameLine(left+lane-exitWidth);
    else
      ImGui::SetCursorPosX(left);
    ImGui::BeginDisabled(board && (jamLocal.child || jamBusy() || jamPublishing() || savingDocument));
    const bool leave = jamButton(exit,{exitWidth,0});
    ImGui::EndDisabled();
    const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape,false) && !ImGui::GetIO().WantTextInput &&
      !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    if (escape && !jamLocal.confirm.empty()) jamLocal.confirm.clear();
    else if (leave) {
      if (board) jamQuit();
      else jamLocal.open = false;
    } else if (escape) {
      if (board && jamEditing() && !jamLocal.child && !jamPublishing()) jamResumeDraft();
      else if (!board || (!jamLocal.child && !jamPublishing())) jamLocal.open = false;
    }
    if (!jamLocal.open) {
      ImGui::End();
      return;
    }
    if (showAlphaWarning) {
      jamAlphaWarning(left,lane);
      ImGui::End();
      return;
    }
    ImGui::SetCursorPosX(left+(jamLocal.screen==JamScreen::Board ? 0.f : (lane-std::min(lane,625*u))*.5f));
    const float contentWidth = jamLocal.screen==JamScreen::Board ? lane : std::min(lane,625*u);
    if (ImGui::BeginChild("##jam-content",{contentWidth,0},ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoBackground)) {
    const bool auth = jamLocal.screen == JamScreen::SignIn || jamLocal.screen == JamScreen::Register ||
      jamLocal.screen == JamScreen::Verify || jamLocal.screen == JamScreen::Recover || jamLocal.screen == JamScreen::Reset;
    if (auth)
      ImGui::SetCursorPosY(ImGui::GetCursorPosY()+std::clamp((ImGui::GetContentRegionAvail().y-425*u)*.22f,0.f,95*u));
    jamBanner();
    const auto feedback = [auth,u](std::string message, ImVec4 color) {
      if (message.empty()) return;
      message = jamDisplayMessage(std::move(message));
      const auto available = ImGui::GetContentRegionAvail().x;
      if (auth) {
        const float width = std::min(available,390*u);
        const float textWidth = ImGui::CalcTextSize(message.c_str(),nullptr,false,width).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(available-textWidth)*.5f));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width);
      } else ImGui::PushTextWrapPos(0);
      ImGui::PushStyleColor(ImGuiCol_Text,color);
      ImGui::TextUnformatted(message.c_str());
      ImGui::PopStyleColor();
      ImGui::PopTextWrapPos();
    };
    feedback(jamLocal.status,ImGui::GetStyleColorVec4(ImGuiCol_Text));
    feedback(jamLocal.error,{1,.43f,.4f,1});
    if (jamBusy() && jamLocal.status == jamText("Downloading the published map…","Загрузка опубликованной карты…") &&
        jamButton(jamText("Cancel game launch","Отменить запуск игры")))
      jamLocal.playCanceled = true;
    ImGui::BeginDisabled(jamBusy() || jamLocal.snapshot.has_value());
    switch (jamLocal.screen) {
      case JamScreen::SignIn:
      case JamScreen::Register:
      case JamScreen::Verify:
      case JamScreen::Recover:
      case JamScreen::Reset: jamAuthForm(); break;
      case JamScreen::Profile:
        jamAvatarEditor();
        ImGui::InputText(jamText("Nickname","Ник"),jamLocal.nickname,sizeof jamLocal.nickname);
        ImGui::InputTextMultiline(jamText("About me","О себе"),jamLocal.bio,sizeof jamLocal.bio,{-1,80*uiScale()});
        if (jamButton(jamText("Save profile","Сохранить профиль"))) jamSaveProfile(false);
        ImGui::SameLine();
        if (jamButton(jamLocal.profileFirst ? jamText("Skip","Пропустить") : jamText("Back","Назад"))) {
          if (jamLocal.profileFirst) jamSaveProfile(true);
          else jamLocal.screen = JamScreen::Board;
        }
        break;
      case JamScreen::Resources:
        ImGui::TextWrapped("LibreQuake 0.09 + Plant Generator");
        if (jamButton(jamText("Prepare Jam resources","Подготовить ресурсы Jam"),{},JamTone::Action)) jamResources();
        ImGui::SameLine();
        if (jamButton(jamText("Sign out","Выйти"))) jamSignOut();
        break;
      case JamScreen::Board: jamBoard(); break;
    }
    ImGui::EndDisabled();
    }
    ImGui::EndChild();
  }
  ImGui::End();
}
} // namespace
