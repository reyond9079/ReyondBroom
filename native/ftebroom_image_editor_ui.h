// SPDX-License-Identifier: GPL-2.0-or-later
#include "ftebroom_image_edit.h"

namespace imageEdit = ftebroom::imageEdit;
struct ImageEditorUI {
  imageEdit::Document document;
  qhandle_t preview = 0;
  bool needsPreview = true, focused = false;
  int tool = 0, brushSize = 5;
  int resize[2]{};
  float color[4]{0.2f, 0.65f, 1.f, 1.f};
  float brightness = 0, contrast = 1;
  std::array<int, 2> anchor{}, pointer{};
  std::optional<std::array<int, 4>> crop;
  bool dragging = false;
  std::string notice;
  char copyName[256]{};
  std::filesystem::path copyFolder;

  bool active() const { return !document.current.pixels.rgba.empty(); }
  void invalidatePreview() { preview = 0; needsPreview = true; }
  void reset() {
    if (preview) draw->UnloadImage(preview);
    *this = {};
  }
  void beginEdit() {
    document.beginEdit();
    notice.clear();
  }
  void replace(imageEdit::Texture pixels) {
    beginEdit();
    document.current.pixels = std::move(pixels);
    crop.reset();
    needsPreview = true;
  }
  void history(bool forward) {
    if (!document.step(forward)) return;
    notice.clear();
    crop.reset();
    needsPreview = true;
  }
};

static ImageEditorUI imageEditUI;
static std::future<std::string> imageCopyJob;

static std::string writeImageCopy(const std::filesystem::path& target, const imageEdit::Texture& pixels) {
  namespace fs = std::filesystem;
  const auto temp = target.parent_path() / fs::u8path(".rb-image-" + ftebroom::bb::uuid());
  try {
    const auto png = entityLower(target.extension().u8string()) == ".png";
    const auto encoded = png ? ftebroom::bb::png(pixels) : ftebroom::bb::tga(pixels);
    if (encoded.empty()) throw std::runtime_error("Cannot encode the image.");
    ftebroom::bb::atomicWrite(temp, encoded);
    const auto staged = ftebroom::filesystemAccessPath(temp);
    const auto output = ftebroom::filesystemAccessPath(target);
#ifdef _WIN32
    if (!MoveFileExW(staged.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH))
      throw fs::filesystem_error("Cannot create the image copy", target,
                                  std::error_code(int(GetLastError()), std::system_category()));
#else
    std::error_code error;
    fs::create_hard_link(staged, output, error);
    if (error == std::errc::operation_not_supported || error == std::errc::operation_not_permitted ||
        error == std::errc::function_not_supported)
      fs::copy_file(staged, output, fs::copy_options::none);
    else if (error)
      throw fs::filesystem_error("Cannot create the image copy", target, error);
#endif
    std::error_code ignored;
    fs::remove(staged, ignored);
    return {};
  } catch (const std::exception& error) {
    std::error_code ignored;
    fs::remove(ftebroom::filesystemAccessPath(temp), ignored);
    return error.what();
  }
}

static void imageCopyDialog() {
  namespace fs = std::filesystem;
  auto& editor = imageEditUI;
  if (ImGui::BeginPopupModal("Save image copy", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", editor.copyFolder.u8string().c_str());
    ImGui::SetNextItemWidth(340);
    ImGui::InputText("Filename", editor.copyName, sizeof editor.copyName);
    browserQaItem("image-editor/name");
    if (ImGui::Button("Save")) {
      try {
        const std::string name = editor.copyName;
        if (name.empty() || name.find_first_of("/\\:*?\"<>|") != std::string::npos ||
            std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; }) ||
            name.back() == ' ' || name.back() == '.')
          throw std::runtime_error("Enter a filename for the copy.");
        auto target = editor.copyFolder / fs::u8path(name);
        const auto ext = entityLower(target.extension().u8string());
        if (ext.empty()) target += ".png";
        else if (ext != ".png" && ext != ".tga")
          throw std::runtime_error("Use a .png or .tga extension.");
        modFileValidate(target);
        if (fs::exists(ftebroom::filesystemAccessPath(target)))
          throw std::runtime_error("That file already exists. Choose a different name.");
        imageCopyJob = std::async(std::launch::async, [target, pixels = editor.document.current.pixels] {
          auto error = writeImageCopy(target, pixels);
          return error.empty() ? "Saved " + target.filename().u8string() : "Error: " + error;
        });
        editor.notice.clear();
        ImGui::CloseCurrentPopup();
      } catch (const std::exception& error) { editor.notice = error.what(); }
    }
    browserQaItem("image-editor/save-confirm");
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    browserQaItem("image-editor/save-cancel");
    if (!editor.notice.empty()) ImGui::TextWrapped("%s", editor.notice.c_str());
    ImGui::EndPopup();
  }
}

static void imageEditorTools(const std::filesystem::path& source) {
  namespace bb = ftebroom::bb;
  auto& editor = imageEditUI;
  auto& document = editor.document;
  const auto& pixels = document.current.pixels;

  if (imageCopyJob.valid() && imageCopyJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    editor.notice = imageCopyJob.get();
    if (editor.notice.rfind("Saved ", 0) == 0) {
      document.saved = document.current.id;
      modFilesRefresh = true;
    }
  }
  ImGui::BeginDisabled(imageCopyJob.valid());
  if (!imageCopyJob.valid() && !ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) editor.history(false);
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) ||
        ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) editor.history(true);
  }
  ImGui::BeginDisabled(document.undo.empty());
  if (ImGui::Button("Undo")) editor.history(false);
  browserQaItem("image-editor/undo");
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(document.redo.empty());
  if (ImGui::Button("Redo")) editor.history(true);
  browserQaItem("image-editor/redo");
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Transform")) ImGui::OpenPopup("Image transforms");
  browserQaItem("image-editor/transform");
  if (ImGui::BeginPopup("Image transforms")) {
    const std::pair<const char*, imageEdit::Transform> actions[]{
      {"Rotate left", imageEdit::Transform::Left}, {"Rotate right", imageEdit::Transform::Right},
      {"Flip horizontally", imageEdit::Transform::FlipX}, {"Flip vertically", imageEdit::Transform::FlipY},
    };
    for (const auto& [label, operation] : actions) {
      if (ImGui::MenuItem(label)) editor.replace(imageEdit::transform(pixels, operation));
      browserQaItem(std::string("image-editor/") + label);
    }
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Resize...")) {
    editor.resize[0] = pixels.width;
    editor.resize[1] = pixels.height;
    ImGui::OpenPopup("Resize image");
  }
  browserQaItem("image-editor/resize");
  if (ImGui::BeginPopup("Resize image")) {
    ImGui::SetNextItemWidth(130);
    ImGui::InputInt("Width", &editor.resize[0], 0);
    browserQaItem("image-editor/width");
    ImGui::SetNextItemWidth(130);
    ImGui::InputInt("Height", &editor.resize[1], 0);
    browserQaItem("image-editor/height");
    if (ImGui::Button("Resize")) {
      try {
        if (pixels.width != editor.resize[0] || pixels.height != editor.resize[1])
          editor.replace(imageEdit::resizeNearest(pixels, editor.resize[0], editor.resize[1]));
        editor.notice.clear();
        ImGui::CloseCurrentPopup();
      } catch (const std::exception& error) { editor.notice = error.what(); }
    }
    browserQaItem("image-editor/resize-apply");
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Adjust...")) {
    editor.brightness = 0;
    editor.contrast = 1;
    ImGui::OpenPopup("Adjust image");
  }
  browserQaItem("image-editor/adjust");
  if (ImGui::BeginPopup("Adjust image")) {
    ImGui::SliderFloat("Brightness", &editor.brightness, -.5f, .5f);
    browserQaItem("image-editor/brightness");
    ImGui::SliderFloat("Contrast", &editor.contrast, .25f, 2.f);
    if (ImGui::Button("Apply adjustment")) {
      if (editor.brightness != 0 || editor.contrast != 1) {
        editor.beginEdit();
        bb::adjustImage(document.current.pixels, editor.brightness, editor.contrast, 0, 1, 1);
        editor.needsPreview = true;
      }
      ImGui::CloseCurrentPopup();
    }
    browserQaItem("image-editor/adjust-apply");
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Save copy...")) {
    editor.copyFolder = modFileMember(source) || wadBrowseMember(source) ? modFileRoot() : source.parent_path();
    const auto name = source.stem().u8string() + "-edited.png";
    std::snprintf(editor.copyName, sizeof editor.copyName, "%s",
                  name.size() < sizeof editor.copyName ? name.c_str() : "image-edited.png");
    editor.notice.clear();
    ImGui::OpenPopup("Save image copy");
  }
  browserQaItem("image-editor/save");
  imageCopyDialog();
  for (const auto& [tool, label] : {std::pair{0, "View"}, {1, "Brush"}, {2, "Eraser"}, {3, "Crop"}}) {
    if (tool) ImGui::SameLine();
    if (ImGui::RadioButton(label, editor.tool == tool)) {
      editor.tool = tool;
      editor.dragging = false;
    }
    browserQaItem(std::string("image-editor/tool-") + label);
  }
  if (editor.tool == 1 || editor.tool == 2) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("Size", &editor.brushSize, 1, 50);
    if (editor.tool == 1) {
      ImGui::SameLine();
      ImGui::ColorEdit4("##brush-color", editor.color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    }
  }
  if (editor.tool == 3) {
    ImGui::SameLine();
    ImGui::BeginDisabled(!editor.crop);
    if (ImGui::Button("Apply crop") && editor.crop) editor.replace(bb::cropImage(pixels, *editor.crop));
    browserQaItem("image-editor/apply-crop");
    ImGui::EndDisabled();
  }
  ImGui::EndDisabled();
  if (imageCopyJob.valid()) ImGui::TextUnformatted("Saving copy...");
  else if (!editor.notice.empty()) ImGui::TextWrapped("%s", editor.notice.c_str());
}

static void imageEditorCanvas(float zoom) {
  namespace bb = ftebroom::bb;
  auto& editor = imageEditUI;
  auto& pixels = editor.document.current.pixels;
  if (editor.needsPreview) {
    const auto scale = std::min({1.f, 2048.f / pixels.width, 2048.f / pixels.height});
    auto resized = scale < 1 ? imageEdit::resizeNearest(pixels, std::max(1, int(pixels.width * scale)),
                                                       std::max(1, int(pixels.height * scale))) : imageEdit::Texture{};
    const auto& preview = scale < 1 ? resized : pixels;
    if (auto api = bbImageAPI())
      editor.preview = api->upload("bbtex/image-editor.tga", preview.rgba.data(), preview.width, preview.height);
    if (!editor.preview) {
      auto bytes = bb::tga(preview);
      draw->LoadImageData("ftebroom/image-editor.tga", "image/tga", bytes.data(), bytes.size());
      editor.preview = draw->LoadImage("ftebroom/image-editor.tga");
    }
    editor.needsPreview = false;
  }
  const auto origin = ImGui::GetCursorScreenPos();
  const ImVec2 size{std::max(1.f, pixels.width * zoom), std::max(1.f, pixels.height * zoom)};
  const ImVec2 end{origin.x + size.x, origin.y + size.y};
  auto* list = ImGui::GetWindowDrawList();
  const auto clipStart = list->GetClipRectMin();
  const auto clipEnd = list->GetClipRectMax();
  constexpr int tile = 12;
  const int firstX = std::max(0, int((clipStart.x - origin.x) / tile));
  const int firstY = std::max(0, int((clipStart.y - origin.y) / tile));
  list->AddRectFilled(origin, end, IM_COL32(120, 120, 120, 255));
  for (int y = firstY; y * tile + origin.y < std::min(end.y, clipEnd.y); ++y)
    for (int x = firstX + ((firstX + y) & 1); x * tile + origin.x < std::min(end.x, clipEnd.x); x += 2)
      list->AddRectFilled({origin.x + x * tile, origin.y + y * tile},
                         {std::min(end.x, origin.x + (x + 1) * tile),
                          std::min(end.y, origin.y + (y + 1) * tile)}, IM_COL32(86, 86, 86, 255));
  ImGui::Image((ImTextureID)editor.preview, size);
  browserQaItem("image-editor/canvas");
  if (imageCopyJob.valid() || !ImGui::IsMouseDown(0) || !ImGui::IsMousePosValid()) editor.dragging = false;
  if (!imageCopyJob.valid() && editor.tool && ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
    editor.dragging = true;
    if (editor.tool != 3) editor.beginEdit();
  }
  if (editor.dragging) {
    const auto mouse = ImGui::GetIO().MousePos;
    std::array<int, 2> point{
      std::clamp(int((mouse.x - origin.x) * pixels.width / size.x), 0, pixels.width - 1),
      std::clamp(int((mouse.y - origin.y) * pixels.height / size.y), 0, pixels.height - 1)};
    const bool start = ImGui::IsMouseClicked(0);
    if (start) editor.anchor = editor.pointer = point;
    if (editor.tool == 3) {
      editor.crop = std::array<int, 4>{std::min(point[0], editor.anchor[0]), std::min(point[1], editor.anchor[1]),
                                      std::max(point[0], editor.anchor[0]) + 1, std::max(point[1], editor.anchor[1]) + 1};
    } else if (start || point != editor.pointer) {
      bb::PaintOptions options;
      options.clip = {0, 0, pixels.width, pixels.height};
      options.circle = true;
      options.size = editor.brushSize;
      options.eraser = editor.tool == 2;
      for (size_t c = 0; c < 4; ++c) options.color[c] = std::lround(editor.color[c] * 255);
      bb::stroke(pixels, editor.pointer[0], editor.pointer[1], point[0], point[1], options);
      editor.needsPreview = true;
    }
    editor.pointer = point;
  }
  if (editor.tool == 3 && editor.crop) {
    const auto& c = *editor.crop;
    const float x = size.x / pixels.width, y = size.y / pixels.height;
    list->AddRect({origin.x + c[0] * x, origin.y + c[1] * y},
                  {origin.x + c[2] * x, origin.y + c[3] * y}, IM_COL32(255, 205, 80, 255), 0, 0, 2);
  }
}
