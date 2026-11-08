// SPDX-License-Identifier: GPL-2.0-or-later
// Included inside testDesktop(), after its guarded setup and expect helper.
if(std::string_view(mode)=="rb10tourstage"){
  char step[16]{};commands->Argv(2,step,sizeof step);
  firstSetupStep=-1;setupNeeded=false;recentWindow=false;
  if(!tourActive)tourBegin();tourPrepare(std::clamp(std::atoi(step),0,6));
  expect(tourWaitingForMap,"rb10_tutorial_every_stage_requests_fresh_map");
}
if(std::string_view(mode)=="rb10tourcheck"){
  frame();frame();
  expect(tourActive&&!tourWaitingForMap,"rb10_tutorial_stage_map_ready");
  expect(tourStep==5||tourStep==6,"rb10_tutorial_final_stage");
  expect(!tourDemo&&!cvars->GetFloat("ftebroom_selection_count"),"rb10_tutorial_final_stage_has_no_selected_practice_brush");
  expect(std::abs(tourWheelPhase(1)-tourWheelPhase(1.25))>.1f,"rb10_tutorial_wheel_indicator_rotates");
}
