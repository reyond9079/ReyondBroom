// SPDX-License-Identifier: GPL-2.0-or-later
static std::string cameraMap, cameraLastState;
static bool cameraCursorPending = false;
static auto cameraLastSave = std::chrono::steady_clock::now();
static bool cameraPersistence()
{
  return !cvars->GetFloat("ftebroom_skip_setup")
         || cvars->GetFloat("ftebroom_test_camera_persistence");
}
static bool cameraParse(const std::string& text, float (&values)[7])
{
  if (
    std::sscanf(
      text.c_str(),
      "%f %f %f %f %f %f %f",
      &values[0],
      &values[1],
      &values[2],
      &values[3],
      &values[4],
      &values[5],
      &values[6])
    != 7)
    return false;
  for (float v : values)
    if (!std::isfinite(v))
      return false;
  for (int i = 0; i < 3; i++)
    if (std::abs(values[i]) > 65536)
      return false;
  return values[3] >= -89 && values[3] <= 89 && values[6] >= 10 && values[6] <= 150;
}
static void cameraSave()
{
  if (!cameraPersistence() || cameraMap.empty() || cameraLastState.empty())
    return;
  auto path = documentPath("cameras/" + cameraMap + ".cam");
  if (path.empty())
    return;
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return;
  auto temporary = path;
  temporary += ".tmp";
  std::ofstream file(temporary, std::ios::trunc);
  file << cameraLastState << "\n";
  file.close();
  if (!file)
    return;
#ifdef _WIN32
  MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
  std::filesystem::rename(temporary, path, error);
#endif
}
static void cameraTick()
{
  if (cameraMap.empty() || cvars->GetFloat("ca_brush_view") != 0)
    return;
  char text[512] = {};
  float values[7];
  if (
    cvars->GetString("ftebroom_camera_state", text, sizeof text)
    && cameraParse(text, values))
    cameraLastState = text;
  auto now = std::chrono::steady_clock::now();
  if (now - cameraLastSave > std::chrono::seconds(1))
  {
    cameraSave();
    cameraLastSave = now;
  }
}
static void cameraMapReady()
{
  cameraSave();
  cameraLastState.clear();
  char name[256] = {};
  cvars->GetString("mapname", name, sizeof name);
  cameraMap = std::filesystem::path(name).stem().string();
  if (cameraMap == pendingDocumentAlias && !pendingDocumentFile.empty())
    cameraMap =
      "external_"
      + std::to_string(std::hash<std::string>{}(pendingDocumentFile.u8string()));
  if (!safeName(cameraMap.c_str()))
    cameraMap.clear();
  std::string text;
  float values[7];
  if (cameraPersistence() && !cameraMap.empty())
  {
    std::ifstream file(documentPath("cameras/" + cameraMap + ".cam"));
    std::getline(file, text);
  }
  command("setpos 320 -400 280 25 130 0\n");
  if (cameraParse(text, values))
  {
    char restore[512];
    std::snprintf(
      restore,
      sizeof restore,
      "in 0.2 ftebroom_camera_restore %.9g %.9g %.9g %.9g %.9g 0 %.9g\n",
      values[0],
      values[1],
      values[2],
      values[3],
      values[4],
      values[6]);
    command(restore);
  }
}
static void cameraEnd()
{
  if (sceneLook)
  {
    sceneLook = false;
    cameraCursorPending = false;
    cvars->SetFloat("ftebroom_camera_captured", 0);
    command("ftebroom_camera_input end 0 0 0 0\n");
    input->SetMenuFocus(qtrue, "", 0, 0, 1);
    ImGui::GetIO().AddMousePosEvent(lastX, lastY);
  }
}
static void cameraInput(const char* kind, float dx = 0, float dy = 0, float scroll = 0)
{
  char text[160];
  int mods = (ctrlHeld ? 1 : 0) | (altHeld ? 2 : 0) | (shiftHeld ? 4 : 0);
  std::snprintf(
    text,
    sizeof text,
    "ftebroom_camera_input %s %g %g %g %d\n",
    kind,
    dx,
    dy,
    scroll,
    mods);
  command(text);
}
static void cameraPreferences()
{
  uiSectionHeading("Mouse / 3D camera");
  struct Setting
  {
    const char* label;
    const char* name;
    float low, high;
  };
  for (auto setting :
       {Setting{"Look sensitivity", "ftebroom_look_speed", .01f, 5},
        Setting{"Pan sensitivity", "ftebroom_pan_speed", .01f, 20},
        Setting{"Wheel movement", "ftebroom_move_speed", .01f, 20},
        Setting{"Flight speed", "ftebroom_fly_speed", 100, 10000},
        Setting{"Base FOV", "ftebroom_fov", 30, 150}})
  {
    float value = cvars->GetFloat(setting.name);
    if (ImGui::SliderFloat(setting.label, &value, setting.low, setting.high))
    {
      cvars->SetFloat(setting.name, value);
      if (std::string(setting.name) == "ftebroom_fov")
        cameraInput("resetzoom");
    }
  }
  for (auto setting :
       {std::pair{"Invert horizontal look", "ftebroom_invert_horizontal"},
        std::pair{"Invert vertical look", "ftebroom_invert_vertical"},
        std::pair{"Wheel moves toward cursor", "ftebroom_move_to_cursor"},
        std::pair{"Alt + middle drag moves forward", "ftebroom_alt_move"}})
  {
    bool value = cvars->GetFloat(setting.second) != 0;
    if (ImGui::Checkbox(setting.first, &value))
      cvars->SetFloat(setting.second, value);
  }
}

static bool cameraPositionRequest;
static void cameraPositionDialog()
{
  static float position[3];
  if (cameraPositionRequest)
  {
    cameraPositionRequest = false;
    char state[512] = {};
    float values[7] = {};
    cvars->GetString("ftebroom_camera_state", state, sizeof state);
    if (cameraParse(state, values))
      std::copy(values, values + 3, position);
    ImGui::OpenPopup("Move Camera to");
  }
  if (uiDialog("Move Camera to"))
  {
    ImGui::InputFloat3("Position X Y Z", position);
    bool valid = true;
    for (float v : position)
      valid &= std::isfinite(v) && std::abs(v) <= 65536;
    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Move"))
    {
      char text[160];
      std::snprintf(
        text,
        sizeof text,
        "ftebroom_camera_move %.9g %.9g %.9g\n",
        position[0],
        position[1],
        position[2]);
      action(text);
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}
