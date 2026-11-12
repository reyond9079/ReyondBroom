// SPDX-License-Identifier: GPL-2.0-or-later
#include <map>
static int uiLanguage=0;
static const char* uiLanguageNames[]={"English","Русский","Deutsch","Français","简体中文","日本語","Español"};
static const char* uiTranslations[][7]={
#include "ftebroom_translations.inc"
};
// Translate presentation only: widget IDs, file paths and editable strings stay unchanged.
void FteBroomTranslate(const char*& text,const char*& end)
{
  if(uiLanguage<=0||uiLanguage>6||!text)return;
  const char* visible=text;while((!end||visible<end)&&*visible&&!(visible[0]=='#'&&visible[1]=='#'))++visible;
  static const auto lookup=[](){std::map<std::string,size_t,std::less<>> result;for(size_t i=0;i<sizeof(uiTranslations)/sizeof(*uiTranslations);i++)result.emplace(uiTranslations[i][0],i);return result;}();
  auto found=lookup.find(std::string_view(text,visible-text));if(found==lookup.end())return;
  if(const char* translation=uiTranslations[found->second][uiLanguage]){
    text=translation;end=text+std::strlen(text);
  }
}
// Translate formats before substitution, and custom-drawn text before measurement.
// Keep the original labels for ImGui widget IDs.
static const char* uiTr(const char* text){const char* end=nullptr;FteBroomTranslate(text,end);return text;}
static ImVector<ImWchar> uiCJKRanges;
static void uiBuildGlyphRanges()
{
  ImFontGlyphRangesBuilder builder;
  builder.AddRanges(ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
  for(auto& row:uiTranslations)for(auto text:row)if(text)builder.AddText(text);
  for(auto name:uiLanguageNames)builder.AddText(name);
  builder.BuildRanges(&uiCJKRanges);
}
