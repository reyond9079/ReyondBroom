// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace {
struct JamChatMessage {
  std::string id, account, nickname, text;
  std::chrono::steady_clock::time_point bubbleUntil;
};

struct JamChatState {
  std::vector<JamChatMessage> history;
  std::optional<JamChatMessage> pending;
  std::int64_t after=0;
  std::array<char,961> input{};
  std::string error;
  bool supported=false, collapsed=false, follow=false;
} jamChat;

void jamChatReceive(const JamJson& response)
{
  using namespace std::chrono;
  const auto result=response.find("chat");
  jamChat.supported=result!=response.end() && result->is_object() && result->contains("messages");
  if (!jamChat.supported) return;
  const auto now=steady_clock::now();
  for (const auto& row : result->at("messages")) {
    const auto sequence=row.value("sequence",std::int64_t(0));
    if (sequence<=jamChat.after) continue;
    const auto id=jamField(row,"id"), account=jamField(row,"accountId"), text=jamField(row,"text");
    if (!jam::hexId(id,32) || !jam::hexId(account,32) || text.empty() || text.size()>960) continue;
    const auto age=std::clamp(row.value("age",8.0),0.0,8.0);
    const auto until=now+duration_cast<steady_clock::duration>(duration<double>(8.0-age));
    jamChat.history.push_back({id,account,jamField(row,"nickname"),text,until});
    jamChat.after=sequence;
  }
  if (jamChat.history.size()>80) jamChat.history.erase(jamChat.history.begin(),jamChat.history.end()-80);
  if (!jamChat.pending) return;
  if (jamField(*result,"ack")==jamChat.pending->id) {
    jamChat.pending.reset();
    return;
  }
  const auto error=jamField(*result,"error");
  if (error.empty()) return;
  jamChat.error=error=="RATE_LIMITED" ? jamText("Wait a moment and send again.","Подожди немного и отправь ещё раз.") :
    jamText("Use a single line of up to 240 characters.","Введи одну строку длиной до 240 символов.");
  if (!jamChat.input[0]) jamAssign(jamChat.input.data(),jamChat.input.size(),jamChat.pending->text);
  jamChat.pending.reset();
}

bool jamChatSend(std::string text)
{
  if (!jamChat.supported || jamChat.pending) return false;
  const auto first=text.find_first_not_of(" \t\r\n");
  if (first==text.npos) return false;
  text=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
  if (ImTextCountCharsFromUtf8(text.c_str(),nullptr)>240) {
    jamChat.error=jamText("Use up to 240 characters.","Введи не больше 240 символов.");
    return false;
  }
  jamRecordTelemetry("button","chat_send");
  jamChat.pending=JamChatMessage{jam::randomHex(16),jamField(jamLocal.account,"accountId"),
                                jamField(jamLocal.account,"nickname"),std::move(text),{}};
  jamChat.input.fill(0);
  jamChat.error.clear();
  jamChat.follow=true;
  return true;
}

const JamChatMessage* jamChatBubble(const std::string& account, std::chrono::steady_clock::time_point now)
{
  for (auto item=jamChat.history.rbegin();item!=jamChat.history.rend();++item)
    if (item->account==account) return item->bubbleUntil>now ? &*item : nullptr;
  return nullptr;
}
} // namespace
