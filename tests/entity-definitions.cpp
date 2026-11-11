// SPDX-License-Identifier: GPL-2.0-or-later
#include <cmath>
#include "../native/ftebroom_entity_defs.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace ftebroom;
  auto xml=entityRead(R"(<classes><list name="mode"><item name="Loop" value="1"/></list><point name="light" box="-8 -8 -8 8 8 8" color="1 .8 .2"><integer key="light" name="Intensity" value="300">Brightness &amp; range</integer><flag bit="2" name="Linear"/><mode key="mode" name="Mode"/></point><group name="func_door"><real key="speed" value="100"/></group></classes>)",".ent");
  assert(xml.size()==2 && !xml.at("light").brush && xml.at("func_door").brush);
  assert(xml.at("light").properties.at("light").description=="Brightness & range");
  assert(xml.at("light").properties.at("spawnflags").choices[0].value=="4");
  assert(xml.at("light").properties.at("mode").choices[0].label=="Loop");
  auto def=entityRead("/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) SUSPENDED - TEAM\nHealth pickup.\n*/\n/*QUAKED func_door (0 .5 .8) ? START_OPEN\nDoor.\n*/", ".def");
  assert(def.size()==2 && def.at("func_door").brush && def.at("item_health").maxs[2]==16);
  assert(def.at("item_health").properties.at("spawnflags").choices[1].value=="4");
  auto fgd=entityRead(R"(@BaseClass = Target [ targetname(string) : "Name" ]
@BaseClass base(Target) size(-16 -16 -24, 16 16 32) color(255 128 0) = Actor [ health(integer) : "Health" : 100 ]
@PointClass base(Actor) studio("models/player.md3") = player : "Player start" [
health(integer) : "Health" : 200
mode(choices) : "Mode" : 0 = [ 0 : "Off" 1 : "On" ]
spawnflags(flags) = [ 1 : "Fast" : 1 4 : "Silent" : 0 ]
]
@SolidClass = func_wall [ ])", ".fgd");
  assert(fgd.size()==2 && fgd.at("player").properties.at("targetname").type=="string");
  assert(fgd.at("player").properties.at("health").value=="200");
  assert(fgd.at("player").properties.at("spawnflags").value=="1");
  assert(fgd.at("player").properties.at("mode").choices.size()==2);
  assert(fgd.at("player").mins[2]==-24 && fgd.at("player").model=="models/player.md3");
  auto prefixed=entityRead(R"(@PointClass model({{ skin -> ":models/alternate.md2", ":models/default.md2" }}) = q2_item [skin(integer) : "Skin" : 0])",".fgd");
  assert(prefixed.at("q2_item").model=="models/default.md2");
  assert(prefixed.at("q2_item").modelRules.front().path=="=models/alternate.md2");
  assert(entityModelPath("progs\\monster.mdl")=="progs/monster.mdl");
  auto model=entityRead(R"(<classes><point name="sample" model="models/sample.md3" skin="2" frame="frame" scale="modelscale, modelscale_vec"/></classes>)", ".ent");
  assert(model.at("sample").modelSkin=="2" && model.at("sample").modelFrame=="frame");
  assert(model.at("sample").modelScale=="modelscale, modelscale_vec");
  int rejected=0;
  for(auto source:{"@BaseClass base(B) = A [] @BaseClass base(A) = B []", "@PointClass base(Missing) = A []", "@PointClass = A [ mode(choices) = [ 1 : \"bad\"", "@PointClass size(1 1 1,0 0 0) = A []"})try{entityRead(source,".fgd");}catch(const std::exception&){rejected++;}
  assert(rejected==4);
  try{entityRead("<classes><point name='bad'></classes>",".ent");assert(false);}catch(const std::exception&){}
  std::cout << "Entity definitions: ENT, DEF, FGD inheritance, flags, choices and malformed-input tests passed\n";
}
