// SPDX-License-Identifier: GPL-2.0-or-later
#include <iomanip>
#include <set>
#include <sstream>
struct OrganizationNode
{
  int kind, id, flags, omit, order;
  std::string name;
  int objectCount=-1;
};
static std::vector<OrganizationNode> organizationNodes;
static int organizationReadEpoch = -1, organizationMapEpoch = 0;
static void organizationLoad()
{
  auto& epoch = organizationReadEpoch;
  int next = (int)cvars->GetFloat("ftebroom_tree_epoch");
  if (next == epoch)
    return;
  std::ifstream in(documentPath("data/ftebroom-ui-tree.txt"));
  if (!in)
    return;
  std::vector<OrganizationNode> nodes;
  std::string line;
  while(std::getline(in,line)){
    OrganizationNode node{};
    std::istringstream row(line);
    if(!(row>>node.kind>>node.id>>node.flags>>node.omit>>node.order>>std::quoted(node.name)))
      return;
    row>>node.objectCount;
    nodes.push_back(std::move(node));
  }
  if (!in.eof())
    return;
  organizationNodes = std::move(nodes);
  epoch = next;
  std::stable_sort(
    organizationNodes.begin(), organizationNodes.end(), [](const auto& a, const auto& b) {
      if (!a.id || !b.id)
        return a.id < b.id;
      return a.order < b.order;
    });
}
static std::string organizationQuote(const std::string& value)
{
  std::ostringstream out;
  out << std::quoted(value);
  return out.str();
}
static void organize(const char* operation, int target = 0, const std::string& value = "")
{
  command(("ftebroom_organize " + std::string(operation) + " " + std::to_string(target)
           + " " + organizationQuote(value) + "\n")
            .c_str());
}
static void organizationBatch(
  const char* operation, const std::vector<std::pair<int, int>>& values)
{
  std::string cmd = std::string("ftebroom_layer_batch ") + operation;
  for (const auto& value : values)
    cmd += " " + std::to_string(value.first) + " " + std::to_string(value.second);
  if (!values.empty())
    command((cmd + "\n").c_str());
}
static void organizationGroupControls()
{
  organizationLoad();
  static char name[256] = "Group";
  ImGui::InputText("Name##group", name, sizeof name);
  ImGui::BeginDisabled(cvars->GetFloat("ftebroom_tool") != 0);
  if (ImGui::Button("Group"))
    command(("ftebroom_group " + organizationQuote(name) + "\n").c_str());
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("Ungroup"))
    organize("ungroup");
  ImGui::SameLine();
  if (ImGui::Button("Rename"))
    organize("rename", 0, name);
  if (ImGui::Button("Hide"))
    command("ftebroom_visibility hide\n");
  ImGui::SameLine();
  if (ImGui::Button("Isolate"))
    command("ftebroom_visibility isolate\n");
  ImGui::SameLine();
  if (ImGui::Button("Show all"))
    command("ftebroom_visibility showall\n");
  if (ImGui::BeginCombo("Add selection to group", "Choose group..."))
  {
    for (const auto& node : organizationNodes)
      if (node.kind == 2)
      {
        ImGui::PushID(node.id);
        if (ImGui::Selectable(node.name.c_str()))
          organize("move", node.id);
        ImGui::PopID();
      }
    ImGui::EndCombo();
  }
  if (ImGui::Button("Remove from group"))
    organize("removefromgroup");
  ImGui::SameLine();
  if (ImGui::Button("Make structural"))
    organize("structural");
}
