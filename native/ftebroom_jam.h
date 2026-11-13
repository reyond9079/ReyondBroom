// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <future>
#include "ftebroom_jam_client.h"

namespace jam = ftebroom::jam;
namespace {
using JamJson = jam::Json;
namespace JamFs = std::filesystem;

unsigned jamPort()
{
  static const unsigned port = [] {
    const auto* value = std::getenv("REYONDBROOM_JAM_PORT");
    if (!value || !*value || std::strspn(value, "0123456789") != std::strlen(value)) return 0u;
    try {
      const auto number = std::stoul(value);
      return number >= 1024 && number <= 65535 ? static_cast<unsigned>(number) : 0u;
    } catch (...) { return 0u; }
  }();
  return port;
}

const jam::Endpoint& jamEndpoint()
{
  static const auto endpoint = [] {
    if (const auto port = jamPort()) return jam::Endpoint{port,{}};
    const auto* url = std::getenv("REYONDBROOM_JAM_URL");
    if (url && *url) return jam::httpsEndpoint(url);
#ifdef REYONDBROOM_JAM_DEFAULT_URL
    return jam::httpsEndpoint(REYONDBROOM_JAM_DEFAULT_URL);
#else
    return jam::Endpoint{};
#endif
  }();
  return endpoint;
}

enum class JamScreen { SignIn, Register, Verify, Recover, Reset, Profile, Resources, Board };
enum class JamFileDialog { None, Open, Save, Compile, Upload, Play, Confirm };
struct JamPublishedMap {
  JamFs::path draft;
  std::string plot, revision, savedHash;
  uint64_t generation = 0;
  int contentRevision = -1;
  bool play = false;
};
struct JamUpload {
  JamFs::path path;
  std::string title;
  JamJson baseRevision;
  JamPublishedMap map;
};
struct JamLocal {
  jam::Client client;
  JamScreen screen = JamScreen::SignIn;
  bool open = false;
  bool canPublish = false;
  std::optional<bool> alphaWarningAccepted;
  bool profileFirst = true;
  bool playCanceled = false;
  bool inspectLocalBuild = false;
  char email[181]{}, password[513]{}, nickname[161]{}, bio[1601]{}, code[121]{};
  char passwordConfirmation[513]{}, avatarPath[4096]{}, reportNote[2401]{};
  bool showPassword = false;
  bool rememberSession = false, triedSavedSession = false;
  std::string purchasedPlot;
  double purchasedAt = 0;
  int reportReason = 0, reportTarget = 0;
  int gift = 1;
  std::string error, status, selection, confirm, operation, draftPlot;
  JamFs::path gameDraft;
  JamPublishedMap publishingMap, publishedMap;
  std::string retryKey, retryIntent;
  JamFs::path pack;
  std::optional<JamUpload> snapshot;
  JamJson account, rules, board, wallet;
  JamJson players = JamJson::array();
  std::future<JamJson> presenceWork;
  std::string presenceToken;
  std::chrono::steady_clock::time_point nextPresence{}, lastPresence{};
  JamFs::path workspace, offlineMap;
  std::optional<int> offlineProfile;
  int offlineMapFormat = 0;
  std::string offlineResourceDirectory;
  bool resourcesMounted = false;
  JamFileDialog fileDialog = JamFileDialog::None;
  std::vector<JamFs::path> openMaps;
  char mapName[241]{};
  std::string fileError;
  bool compileAfterSave = false, compileReady = false, compileWithLeaks = false, dialogFocus = false;
  bool uploadAfterSave = false, uploadReady = false, playAfterSave = false;
  bool publishPlayAfterSave = false;
  std::string pendingAvatar, pendingAvatarUrl;
  bool avatarEdited = false;
  std::map<std::string, qhandle_t> avatarTextures;
  std::future<JamJson> work;
  std::function<void(JamJson)> ready;
  std::chrono::steady_clock::time_point nextPoll{};
#ifdef _WIN32
  HANDLE child = nullptr;
#else
  pid_t child = 0;
#endif
} jamLocal;

bool jamEnabled() { return jamEndpoint().port != 0; }
bool jamBusy() { return jamLocal.work.valid(); }
const char* jamText(const char* en, const char* ru) { return uiLanguage == 1 ? ru : en; }

std::string jamDisplayMessage(std::string message)
{
  const auto& hostname = jamEndpoint().hostname;
  if (hostname.empty()) return message;
  auto lower = message;
  for (auto& ch : lower) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
  constexpr std::string_view replacement = "Mapping Jams";
  for (std::size_t pos = 0; (pos = lower.find(hostname,pos)) != lower.npos;) {
    auto begin = pos;
    for (const auto prefix : {std::string_view("https://"),std::string_view("http://")}) {
      if (pos >= prefix.size() && lower.compare(pos-prefix.size(),prefix.size(),prefix) == 0) {
        begin = pos-prefix.size();
        break;
      }
    }
    const auto length = pos+hostname.size()-begin;
    message.replace(begin,length,replacement);
    lower.replace(begin,length,replacement);
    pos = begin+replacement.size();
  }
  return message;
}

JamFs::path jamRoot()
{
  const auto& endpoint = jamEndpoint();
  const auto root = settingsPath().parent_path();
  if (endpoint.hostname.empty()) return root / "mapping-jams-local" / std::to_string(endpoint.port);
  static const auto key = jam::sha256(endpoint.hostname);
  const auto previous = root / "mapping-jams" / endpoint.hostname;
  const auto current = root / "mapping-jams" / key;
  std::error_code error;
  if (JamFs::is_directory(previous,error) && !JamFs::exists(current,error) && !error) {
    JamFs::rename(previous,current,error);
    if (error) return previous;
  }
  return current;
}

JamFs::path jamAlphaWarningPath()
{
  return settingsPath().parent_path() / "mapping-jams-local" / "alpha-warning-accepted";
}

bool jamAlphaWarningAccepted()
{
  if (!jamLocal.alphaWarningAccepted) {
    try { jamLocal.alphaWarningAccepted = jam::fileBytes(jamAlphaWarningPath(),16) == "accepted\n"; }
    catch (...) { jamLocal.alphaWarningAccepted = false; }
  }
  return *jamLocal.alphaWarningAccepted;
}

void jamAcceptAlphaWarning()
{
  try {
    jam::writeBytes(jamAlphaWarningPath(),"accepted\n");
    jamLocal.alphaWarningAccepted = true;
    jamLocal.error.clear();
  } catch (const std::exception&) {
    jamLocal.error = jamText("Could not save your choice. Try again.",
                             "Не удалось сохранить согласие. Попробуй ещё раз.");
  }
}

std::string jamField(const JamJson& value, const char* key)
{
  if (!value.is_object()) return {};
  const auto field = value.find(key);
  return field != value.end() && field->is_string() ? field->get<std::string>() : std::string{};
}

void jamAssign(char* target, std::size_t size, const std::string& value)
{
  std::snprintf(target, size, "%s", value.c_str());
}

const char* jamPasswordRules()
{
  return jamText("Use 12 to 128 characters. Numbers, symbols and uppercase letters are optional.",
                 "От 12 до 128 символов. Цифры, спецсимволы и заглавные буквы необязательны.");
}

bool jamValidNewPassword()
{
  const std::string_view password(jamLocal.password);
  const auto length = std::count_if(password.begin(),password.end(),[](unsigned char byte) {
    return (byte & 0xc0) != 0x80;
  });
  if (length >= 12 && length <= 128) return true;
  jamLocal.error = jamPasswordRules();
  return false;
}

void jamStart(std::string status, std::function<JamJson(jam::Client)> task,
              std::function<void(JamJson)> done)
{
  if (jamBusy()) return;
  jamLocal.error.clear();
  jamLocal.status = std::move(status);
  jamLocal.ready = std::move(done);
  jamLocal.client.endpoint = jamEndpoint();
  jamLocal.work = std::async(std::launch::async, [client=jamLocal.client, task=std::move(task)] {
    return task(client);
  });
}

void jamApplyBoard(JamJson result)
{
  jamLocal.board = std::move(result.at("board"));
  jamLocal.wallet = std::move(result.at("wallet"));
  jamLocal.nextPresence = {};
}

void jamPresenceTick()
{
  const auto now = std::chrono::steady_clock::now();
  const auto& client = jamLocal.client;
  if (jamLocal.presenceWork.valid() &&
      jamLocal.presenceWork.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    try {
      auto value = jamLocal.presenceWork.get();
      if (jamLocal.presenceToken == client.token) {
        jamLocal.players = std::move(value.at("players"));
        jamLocal.lastPresence = now;
      }
    } catch (const std::exception&) {}
  }
  if ((!jamLocal.open && jamLocal.workspace.empty()) || client.token.empty() || client.prefix.empty() ||
      jamLocal.presenceWork.valid() || now < jamLocal.nextPresence) return;
  jamLocal.presenceToken = client.token;
  jamLocal.nextPresence = now + std::chrono::seconds(10);
  jamLocal.presenceWork = std::async(std::launch::async,[client] {
    return client.json("POST",client.prefix+"presence",JamJson::object());
  });
}

void jamRefresh()
{
  jamStart(jamText("Refreshing the board…","Обновление поля…"),
    [](auto client) {
      auto board = client.board();
      board["canPublish"] = client.json("GET","/v1/jams/current").at("jam").value("canPublish",false);
      return board;
    }, [](auto board) {
      jamLocal.canPublish = board.value("canPublish",false);
      jamApplyBoard(std::move(board));
    });
}

void jamJoin()
{
  jamStart(jamText("Checking Jam resources…","Проверка ресурсов Jam…"), [root=jamRoot()](auto client) {
    auto info = client.json("GET", "/v1/jams/current").at("jam");
    client.prefix = "/v1/jams/" + info.at("jamId").template get<std::string>() + "/";
    auto rules = client.json("GET", client.prefix + "rules");
    client.json("POST", client.prefix + "join", JamJson::object());
    auto data = client.board();
    data["prefix"] = client.prefix;
    data["rules"] = rules;
    data["canPublish"] = info.value("canPublish",false);
    const auto version=rules.at("packVersion").template get<std::string>();
    if(version=="librequake-0.09-plants-local1"){
      const auto manifest=client.json("GET",client.prefix+"packs/"+version+"/manifest");
      if(jamField(manifest,"packVersion")==version && jam::installedPack(root/"packs"/version,manifest))
        data["pack"]=(root/"packs"/version).u8string();
    }
    return data;
  }, [](auto data) {
    jamLocal.client.prefix = data.at("prefix");
    jamLocal.rules = data.at("rules");
    jamLocal.canPublish = data.value("canPublish",false);
    const auto pack=jamField(data,"pack");
    jamApplyBoard(std::move(data));
    jamLocal.pack=JamFs::u8path(pack);
    jamLocal.screen = pack.empty() ? JamScreen::Resources : JamScreen::Board;
  });
}

void jamRegister()
{
  if (!jamValidNewPassword()) return;
  const JamJson form{{"email", jamLocal.email}, {"nickname", jamLocal.nickname}, {"password", jamLocal.password}};
  jamStart(jamText("Creating account…","Создание аккаунта…"), [form](auto client) {
    return client.json("POST", "/v1/auth/register", form);
  }, [](auto) {
    std::fill(std::begin(jamLocal.password), std::end(jamLocal.password), '\0');
    jamLocal.screen = JamScreen::Verify;
  });
}

void jamVerify()
{
  JamJson form{{"token", jamLocal.code}};
  if (jamLocal.email[0]) form["email"] = jamLocal.email;
  jamStart(jamText("Verifying email…","Подтверждение почты…"), [form](auto client) {
    return client.json("POST", "/v1/auth/email/verify", form);
  }, [](auto) {
    std::fill(std::begin(jamLocal.code), std::end(jamLocal.code), '\0');
    jamLocal.screen = JamScreen::SignIn;
    jamLocal.status = jamText("Email confirmed. Sign in.","Почта подтверждена. Войдите в аккаунт.");
  });
}

void jamResendConfirmation()
{
  const JamJson form{{"email",jamLocal.email}};
  jamStart(jamText("Requesting a new confirmation code…","Запрос нового кода подтверждения…"), [form](auto client) {
    return client.json("POST","/v1/auth/email/resend",form);
  }, [](auto) {
    std::fill(std::begin(jamLocal.code), std::end(jamLocal.code), '\0');
    jamLocal.status = jamText("If the account is awaiting confirmation, check your inbox for a new code.",
                              "Если аккаунт ещё не подтверждён, проверьте почту и введите новый код.");
  });
}

void jamForgotPassword()
{
  const JamJson form{{"email",jamLocal.email}};
  jamStart(jamText("Requesting a recovery code…","Запрос кода восстановления…"), [form](auto client) {
    return client.json("POST","/v1/auth/password/forgot",form);
  }, [](auto) {
    jamLocal.screen = JamScreen::Reset;
    jamLocal.status = jamEndpoint().hostname.empty()
      ? jamText("If this email is registered, a recovery code is available in the local mail folder.",
                "Если почта зарегистрирована, код восстановления появится в локальной почтовой папке.")
      : jamText("If this email is registered, check your inbox for a recovery code.",
                "Если почта зарегистрирована, проверьте входящие и введите код восстановления.");
  });
}

void jamResetPassword()
{
  if (!jamValidNewPassword()) return;
  if (std::strcmp(jamLocal.password,jamLocal.passwordConfirmation) != 0) {
    jamLocal.error = jamText("The passwords do not match.","Пароли не совпадают.");
    return;
  }
  JamJson form{{"token",jamLocal.code},{"password",jamLocal.password}};
  if (jamLocal.email[0]) form["email"] = jamLocal.email;
  jamStart(jamText("Resetting password…","Сброс пароля…"), [form](auto client) {
    return client.json("POST","/v1/auth/password/reset",form);
  }, [](auto) {
    std::fill(std::begin(jamLocal.password),std::end(jamLocal.password),'\0');
    std::fill(std::begin(jamLocal.passwordConfirmation),std::end(jamLocal.passwordConfirmation),'\0');
    std::fill(std::begin(jamLocal.code),std::end(jamLocal.code),'\0');
    jamLocal.screen = JamScreen::SignIn;
    jamLocal.status = jamText("Password changed. Sign in with the new password.","Пароль изменён. Войди с новым паролем.");
  });
}

void jamBeginProfile(bool first)
{
  jamLocal.profileFirst = first;
  jamLocal.avatarEdited = false;
  jamLocal.pendingAvatar.clear();
  jamLocal.pendingAvatarUrl.clear();
  jamLocal.avatarPath[0] = '\0';
  jamAssign(jamLocal.nickname,sizeof jamLocal.nickname,jamField(jamLocal.account,"nickname"));
  jamAssign(jamLocal.bio,sizeof jamLocal.bio,jamField(jamLocal.account,"bio"));
  jamLocal.screen = JamScreen::Profile;
}

void jamUploadAvatar(const JamFs::path& path)
{
  jamStart(jamText("Preparing avatar…","Подготовка аватара…"), [path](auto client) {
    return client.json("POST","/v1/me/avatar",nullptr,{},jam::fileBytes(path,4*1024*1024));
  }, [](auto response) {
    jamLocal.pendingAvatar = jamField(response,"avatarId");
    jamLocal.pendingAvatarUrl = "/v1/avatars/" + jamLocal.pendingAvatar;
    jamLocal.avatarEdited = true;
  });
}

void jamLoadAvatar(const std::string& url)
{
  if (url.rfind("/v1/avatars/",0)!=0 || url.size()!=44 || !jam::safePath(url) ||
      jamLocal.avatarTextures.count(url) || jamBusy() || !jamLocal.operation.empty())
    return;
  jamStart({}, [url](auto client) {
    const auto response = jam::request(client.endpoint,"GET",url,client.token,{},{},128*1024);
    return response.status==200
      ? JamJson{{"png",JamJson::binary(std::vector<uint8_t>(response.body.begin(),response.body.end()))}}
      : JamJson::object();
  }, [url](auto value) {
    qhandle_t image = 0;
    if (value.contains("png")) {
      auto& png = value["png"].get_binary();
      const auto name = "ftebroom/jam/avatar-" + url.substr(12) + ".png";
      draw->LoadImageData(name.c_str(),"image/png",png.data(),png.size());
      image = draw->LoadImage(name.c_str());
    }
    jamLocal.avatarTextures[url] = image;
  });
}

JamFs::path jamSessionPath() {return jamRoot()/"session.dat";}

void jamSignedIn(JamJson result)
{
  jamLocal.password[0]='\0';
  jamLocal.client.token=result.at("accessToken");
  jamLocal.account=result.at("account");
  jamAssign(jamLocal.email,sizeof jamLocal.email,jamField(jamLocal.account,"email"));
  if(jamField(jamLocal.account,"state")=="profile_pending")jamBeginProfile(true);
  else jamJoin();
}

void jamResumeSession()
{
  if(jamLocal.triedSavedSession||jamLocal.screen!=JamScreen::SignIn||jamBusy()||!jamLocal.open)return;
  if (!jamAlphaWarningAccepted()) return;
  jamLocal.triedSavedSession=true;
  const auto token=jam::savedSession(jamSessionPath());
  if(token.empty())return;
  jamLocal.rememberSession=true;
  jamStart(jamText("Restoring your session…","Восстановление входа…"),[token](auto client){
    client.token=token;
    return JamJson{{"accessToken",token},{"account",client.json("GET","/v1/me")}};
  },jamSignedIn);
}

void jamLogin()
{
  const bool remember=jamLocal.rememberSession;
  const JamJson form{{"email", jamLocal.email}, {"password", jamLocal.password},{"remember",remember}};
  jamStart(jamText("Signing in…","Вход…"), [form](auto client) {
    auto result = client.json("POST", "/v1/auth/login", form);
    client.token = result.at("accessToken");
    result["account"] = client.json("GET", "/v1/me");
    return result;
  }, [remember](auto result) {
    std::fill(std::begin(jamLocal.password), std::end(jamLocal.password), '\0');
    if(!remember)jam::forgetSession(jamSessionPath());
    const bool stored=!remember||jam::rememberSession(jamSessionPath(),jamField(result,"accessToken"));
    jamSignedIn(std::move(result));
    if(!stored)jamLocal.rememberSession=false;
  });
}

void jamSaveProfile(bool skip)
{
  JamJson data{{"nickname", jamLocal.nickname}, {"bio", skip ? "" : jamLocal.bio},
                     {"completeOnboarding", true}};
  if (skip || jamLocal.avatarEdited)
    data["avatarId"] = skip || jamLocal.pendingAvatar.empty() ? JamJson(nullptr) : JamJson(jamLocal.pendingAvatar);
  jamStart(jamText("Saving profile…","Сохранение профиля…"), [data](auto client) {
    return client.json("PATCH", "/v1/me/profile", data);
  }, [](auto profile) {
    jamLocal.account = std::move(profile);
    if (jamLocal.profileFirst) jamJoin();
    else jamLocal.screen = JamScreen::Board;
  });
}

void jamResources()
{
  const auto version = jamField(jamLocal.rules, "packVersion");
  if (version != "librequake-0.09-plants-local1") {
    jamLocal.error = jamText("Unsupported local pack version.","Эта версия локального пакета не поддерживается.");
    return;
  }
  const auto destination = jamRoot() / "packs" / version;
  jamStart(jamText("Downloading and verifying resources…","Загрузка и проверка ресурсов…"), [version,destination](auto client) {
    const auto manifest = client.json("GET", client.prefix + "packs/" + version + "/manifest");
    if (jamField(manifest,"packVersion") != version)
      throw std::runtime_error("The base pack version changed.");
    jam::download(client, manifest, destination, true);
    if(!jam::installedPack(destination,manifest))throw std::runtime_error("The installed Jam resources failed verification.");
    return client.board();
  }, [destination](auto data) {
    jamLocal.pack = destination;
    jamApplyBoard(std::move(data));
    jamLocal.screen = JamScreen::Board;
  });
}

JamJson jamCell(const std::string& selection)
{
  if (jamLocal.board.is_object())
    for (const auto& cell : jamLocal.board.at("cells")) {
      if (jamField(cell, "row") + std::to_string(cell.at("column").get<int>()) == selection)
        return cell;
    }
  return nullptr;
}

JamJson jamCell() { return jamCell(jamLocal.selection); }

std::string jamMutationKey(const std::string& intent)
{
  if (jamLocal.retryIntent != intent || jamLocal.retryKey.empty()) {
    jamLocal.retryIntent = intent;
    jamLocal.retryKey = jam::randomHex(16);
  }
  return jamLocal.retryKey;
}

void jamPurchase()
{
  const auto plot = jamLocal.selection;
  const auto version = jamField(jamLocal.rules, "rulesVersion");
  const auto key = jamMutationKey("purchase/" + plot + "/" + version);
  jamStart(jamText("Buying plot…","Покупка участка…"), [plot,version,key](auto client) {
    client.json("POST", client.prefix + "plots/" + plot + "/purchase",
                {{"expectedRulesVersion",version},{"expectedPriceRb",20}}, key);
    return client.board();
  }, [plot](auto result) {
    jamLocal.retryKey.clear();
    jamApplyBoard(std::move(result));
    jamLocal.purchasedPlot=plot;
    jamLocal.purchasedAt=ImGui::GetTime();
    jamLocal.status = jamText("Plot purchased.","Участок куплен.");
  });
}

void jamGift(const std::string& recipient)
{
  const int amount = jamLocal.gift;
  const auto key = jamMutationKey("gift/" + recipient + "/" + std::to_string(amount));
  jamStart(jamText("Sending gift…","Отправка подарка…"), [recipient,amount,key](auto client) {
    return client.json("POST", client.prefix + "gifts",
                       {{"recipientAccountId",recipient},{"amountRb",amount}}, key);
  }, [](auto result) {
    jamLocal.retryKey.clear();
    jamLocal.wallet = result.at("wallet");
    jamLocal.status = jamText("Gift sent.","Подарок отправлен.");
  });
}

void jamLike(const std::string& plot)
{
  const auto key=jamMutationKey("like/"+plot);
  jamStart(jamText("Sending 1 RB…","Отправка 1 RB…"),[plot,key](auto client){
    return client.json("POST",client.prefix+"plots/"+plot+"/likes",JamJson::object(),key);
  },[plot](auto result){
    jamLocal.retryKey.clear();
    jamLocal.wallet=result.at("wallet");
    for(auto& cell:jamLocal.board.at("cells"))
      if(jamField(cell,"plotId")==plot)cell["rbLikes"]=result.at("rbLikes");
    jamLocal.status=jamText("1 RB gifted.","Подарен 1 RB.");
  });
}

void jamReport(const std::string& kind, const std::string& target, const std::string& reason)
{
  const JamJson body{{"targetKind",kind},{"targetId",target},{"reasonCode",reason},{"note",jamLocal.reportNote}};
  const auto key = jamMutationKey("report/" + body.dump());
  jamStart(jamText("Submitting report…","Отправка жалобы…"), [body,key](auto client) {
    return client.json("POST",client.prefix + "reports",body,key);
  }, [](auto) {
    jamLocal.retryKey.clear();
    jamLocal.reportNote[0] = '\0';
    jamLocal.status = jamText("Report submitted.","Жалоба отправлена.");
  });
}

bool jamSafePlot(const std::string& plot)
{
  return plot.size()==2 && plot[0]>='A' && plot[0]<='E' && plot[1]>='1' && plot[1]<='5' &&
    (plot[0]=='A' || plot[0]=='E' || plot[1]=='1' || plot[1]=='5');
}

JamFs::path jamDrafts()
{
  const auto id = jamField(jamLocal.account, "accountId");
  if (id.size()!=32 || id.find_first_not_of("0123456789abcdef")!=id.npos) return {};
  return JamFs::absolute(jamRoot() / "drafts" / id).lexically_normal();
}

bool jamStoredPath(const JamFs::path& path)
{
  if (!jamEnabled() || path.empty()) return false;
  std::error_code error;
  const auto root = JamFs::absolute(jamRoot()/"drafts",error).lexically_normal();
  if (error) return false;
  const auto candidate = JamFs::absolute(path,error).lexically_normal();
  if (error) return false;
  const auto relative = candidate.lexically_relative(root);
  return !relative.empty() && *relative.begin()!="..";
}

bool jamMapPath(const JamFs::path& path)
{
  if (!path.is_absolute() || path.extension()!=".map") return false;
  std::error_code error;
  const auto clean = path.lexically_normal();
  const auto plot = clean.parent_path();
  return clean==path && jamSafePlot(plot.filename().u8string()) && plot.parent_path()==jamDrafts() &&
    JamFs::weakly_canonical(path,error)==path && !error;
}

bool jamOwnedPlot(const std::string& plot)
{
  if (!jamLocal.board.is_object()) return false;
  for (const auto& cell : jamLocal.board.at("cells"))
    if (jamField(cell,"plotId")==plot)
      return jamField(cell.at("owner"),"accountId")==jamField(jamLocal.account,"accountId");
  return false;
}

JamFs::path jamDraft(const std::string& plot)
{
  const auto root = jamDrafts();
  if (root.empty() || !jamSafePlot(plot)) throw std::runtime_error("Invalid Jam plot or account.");
  const auto path = root / plot / "jam-draft.map";
  if (!jamMapPath(path)) throw std::runtime_error("Invalid Jam map directory.");
  return path;
}

bool jamEditing() { return !jamLocal.workspace.empty(); }

JamFs::path jamPublicationReceipt(const JamFs::path& draft)
{
  auto receipt = draft;
  receipt += ".jam-publication.json";
  return receipt;
}

std::vector<JamFs::path> jamMaps()
{
  std::vector<JamFs::path> maps;
  const auto root = jamDrafts();
  if (root.empty() || !JamFs::is_directory(root)) return maps;
  for (const auto& plot : JamFs::directory_iterator(root)) {
    if (!jamOwnedPlot(plot.path().filename().u8string()) || plot.is_symlink() || !plot.is_directory()) continue;
    for (const auto& entry : JamFs::directory_iterator(plot.path())) {
      if (entry.path().filename()!="jam-draft.map" && jamMapPath(entry.path()) && entry.is_regular_file())
        maps.push_back(entry.path());
    }
  }
  std::sort(maps.begin(),maps.end(),[](const auto& a,const auto& b) {
    return a.filename()==b.filename() ? a<b : a.filename()<b.filename();
  });
  return maps;
}

void jamOpenMap(const JamFs::path& path)
{
  if (!jamMapPath(path) || !jamOwnedPlot(path.parent_path().filename().u8string()) || !JamFs::is_regular_file(path))
    throw std::runtime_error(jamText("Choose one of your Jam maps.","Выбери одну из своих карт Jam."));
  if (!jamEditing() && documentFile!=path) jamLocal.offlineMap = documentFile;
  jamLocal.open = false;
  documentRequest("@"+path.u8string());
}

bool jamAcceptDocument(const std::string& next)
{
  if (documentIsExit(next)) return true;
  if (next=="__jam_offline" || next=="__jam_signout") return jamEditing();
  if (!jamEnabled()) return true;
  const auto path = next.rfind("@",0)==0 ? JamFs::u8path(next.substr(1)) : JamFs::path{};
  if (jamMapPath(path)) return jamOwnedPlot(path.parent_path().filename().u8string());
  return !jamEditing() && !jamStoredPath(path);
}

void jamSignOut();

void jamRestoreOfflineProfile()
{
  if (!jamLocal.offlineProfile) return;
  gameProfile = *std::exchange(jamLocal.offlineProfile,std::nullopt);
  q1MapFormat = jamLocal.offlineMapFormat;
  std::snprintf(baseq3,sizeof baseq3,"%s",jamLocal.offlineResourceDirectory.c_str());
  jamLocal.resourcesMounted = false;
  gameMapModsKnown = false;
  gameMapResourcePaths.clear();gameMapResourceRoot.clear();
  q1MapWads.clear();q1MissingMapWads.clear();q1DocumentPak.clear();q1DocumentBspWad.clear();
}

void jamRouteDocument(std::string& next)
{
  const bool signingOut = next=="__jam_signout";
  if (next!="__jam_offline" && !signingOut) return;
  const auto previous = std::move(jamLocal.offlineMap);
  jamLocal.offlineMap.clear();
  const bool returnToMap = !previous.empty() && ftebroom::pak::exists(previous);
  next = returnToMap ? "@"+previous.u8string() : "__profile:"+std::to_string(jamLocal.offlineProfile.value_or(2));
  jamLocal.workspace.clear();
  jamRestoreOfflineProfile();
  if (returnToMap) mountAssets();
  if (signingOut) {jamLocal.open=true;jamSignOut();}
}

void jamDocumentLoaded()
{
  if (!jamEnabled()) return;
  jamLocal.workspace = jamMapPath(documentFile) ? documentFile.parent_path() : JamFs::path{};
  jamLocal.draftPlot = jamEditing() ? jamLocal.workspace.filename().u8string() : "";
  if (jamEditing() && documentFile.filename()=="jam-draft.map") {
    documentUntitled = true;
    documentName = "untitled";
  }
  jamLocal.publishedMap = {};
  if (!jamEditing() || documentUntitled) return;
  try {
    const auto receipt = jamPublicationReceipt(documentFile);
    if (!JamFs::is_regular_file(receipt)) return;
    const auto data = JamJson::parse(jam::fileBytes(receipt,4096));
    const auto revision = jamField(data,"revisionId"), hash = jamField(data,"savedHash");
    if (jam::hexId(revision,32) && jam::hexId(hash,64) &&
        jam::sha256(jam::fileBytes(documentFile,32*1024*1024))==hash)
      jamLocal.publishedMap = {documentFile,jamLocal.draftPlot,revision,hash,documentGeneration,buildCurrentContentRevision()};
  } catch (const std::exception&) {}
}

bool jamMatchesDraft(const JamPublishedMap& published)
{
  return !published.draft.empty() && jamEditing() && documentFile==published.draft &&
    documentGeneration==published.generation && buildCurrentContentRevision()==published.contentRevision;
}

bool jamPublishedCurrent()
{
  const auto& published = jamLocal.publishedMap;
  if (published.revision.empty() || !jamMatchesDraft(published)) return false;
  const auto cell = jamCell(published.plot);
  if (!cell.is_object()) return false;
  const auto& current = cell.at("publication");
  return current.is_object() && current.value("playable",false) &&
    jamField(current,"revisionId")==published.revision;
}

void jamShowCompiled(const JamPublishedMap& published)
{
  if (!jamMatchesDraft(published) || !jam::hexId(published.revision,32) || jamBusy()) return;
  const auto cache = jamRoot()/"published"/published.revision;
  jamStart(jamText("Loading the compiled map…","Загрузка скомпилированной карты…"),
    [published,cache](auto client) {
      const auto manifest = client.json("GET",client.prefix+"plots/"+published.plot+"/play-manifest");
      if (jamField(manifest,"revisionId")!=published.revision)
        throw std::runtime_error("This publication is no longer current.");
      jam::download(client,manifest,cache,false,"jam_map",true);
      return JamJson::object();
    },[published,cache](auto) {
      if (!jamMatchesDraft(published) || jamLocal.open) return;
      buildShowArtifact(cache/"maps/jam_map.bsp","maps/ftebroom-build/jam-"+published.revision+"/jam_map.bsp");
    });
}

void jamStageHub(const jam::Client& client, const JamFs::path& game, const JamFs::path& cache,
                 const std::string& pack)
{
  for (int attempt=0; ; ++attempt) {
    const auto manifest = client.json("GET",client.prefix+"hub/manifest");
    const auto version = jamField(manifest,"hubVersion");
    if (!jam::hexId(version,64) || jamField(manifest,"mapStem")!="jam_hub" ||
        jamField(manifest,"packVersion")!=pack)
      throw std::runtime_error("Invalid Hub manifest or resource pack.");
    const auto& maps = manifest.at("maps");
    if (!maps.is_array() || maps.size()>16) throw std::runtime_error("Invalid Hub plot list.");
    const auto hubCache = cache/"hub";
    try {
      jam::download(client,manifest,hubCache,false,"jam_hub",true);
      JamFs::create_directories(game/"maps");
      const auto stage = [&game](const JamFs::path& source, const std::string& name, const std::string& destination) {
        for (const auto* ext : {".bsp",".lit"}) {
          const auto file = source/"maps"/(name+ext);
          if (JamFs::is_regular_file(file))
            JamFs::copy_file(file,game/"maps"/(destination+ext),JamFs::copy_options::overwrite_existing);
          else if (std::string_view(ext)==".lit") JamFs::remove(game/"maps"/(destination+ext));
        }
      };
      stage(hubCache,"jam_hub","jam_hub");
      for (const auto& entry : maps) {
        const auto id = jamField(entry,"plotId"), revision = jamField(entry,"revisionId");
        if (!jamSafePlot(id) || !jam::hexId(revision,32) ||
            jamField(entry,"mapStem")!="jam_map" || jamField(entry,"packVersion")!=pack)
          throw std::runtime_error("Invalid Hub plot map.");
        const auto mapCache = cache/"published"/revision;
        jam::download(client,entry,mapCache,false,"jam_map",true);
        stage(mapCache,"jam_map","jam_"+std::string{char(id[0]-'A'+'a'),id[1]});
      }
      if (jamField(client.json("GET",client.prefix+"hub/manifest"),"hubVersion")==version) {
        jam::writeBytes(game/"hub-version.txt",version);
        return;
      }
      for (const auto& entry : JamFs::directory_iterator(game/"maps")) JamFs::remove(entry.path());
    } catch (const jam::Error& error) {
      if (error.code!="ARTIFACT_CHANGED" || attempt>=2) throw;
      for (const auto& entry : JamFs::directory_iterator(game/"maps")) JamFs::remove(entry.path());
    }
    if (attempt>=2) throw std::runtime_error("The Hub changed during download. Open it again.");
  }
}

void jamFileRequest(JamFileDialog dialog)
{
  jamLocal.fileDialog = dialog;
  jamLocal.fileError.clear();
  jamLocal.dialogFocus = true;
  if (dialog==JamFileDialog::Save || dialog==JamFileDialog::Compile ||
      dialog==JamFileDialog::Upload || dialog==JamFileDialog::Play)
    jamAssign(jamLocal.mapName,sizeof jamLocal.mapName,documentUntitled ? "" : documentName);
  if (dialog==JamFileDialog::Open) {
    jamLocal.openMaps.clear();
    try {jamLocal.openMaps=jamMaps();}
    catch (const std::exception& error) {jamLocal.fileError=error.what();}
  }
}

bool jamCanSaveTo(const JamFs::path& destination, const std::string& name)
{
  if (!jamEditing()) return !jamStoredPath(destination) && !jamStoredPath(documentFile);
  if (destination.empty() && name!=documentName) return false;
  const auto& target = destination.empty() ? documentFile : destination;
  if (jamMapPath(target) && target.parent_path()==jamLocal.workspace &&
      target.filename()!="jam-draft.map") return true;
  documentError = jamText("Save the map by name in the Jam workspace.","Сохрани карту по названию в рабочей области Jam.");
  return false;
}

bool jamSaveCommand()
{
  if (!jamEditing()) return false;
  if (documentUntitled) jamFileRequest(JamFileDialog::Save);
  else documentSave(documentName,false,documentFile);
  return true;
}

bool jamConfirmDocument()
{
  if (!jamEditing()) return false;
  if (confirmDocument) {
    confirmDocument = false;
    jamFileRequest(JamFileDialog::Confirm);
  }
  return documentError.empty();
}

bool jamBeforeCompile(bool allowLeaks)
{
  if (!jamEditing()) return false;
  if (savingDocument) return true;
  if (!documentUntitled && !cvars->GetFloat("ftebroom_dirty")) return false;
  jamLocal.compileWithLeaks = allowLeaks;
  jamFileRequest(JamFileDialog::Compile);
  return true;
}

std::string jamCompiler(const char* name)
{
  if (!jamEditing()) return {};
  const auto* tools = std::getenv("REYONDBROOM_JAM_TOOLS");
  const auto bundled = settingsPath().parent_path() / "tools" / "ericw";
  auto compiler = (tools && *tools ? JamFs::u8path(tools) : bundled) / name;
#ifdef _WIN32
  compiler += ".exe";
#endif
  std::error_code error;
  return JamFs::is_regular_file(compiler,error) ? compiler.u8string() : std::string{};
}

JamFs::path jamPreviewRuntime()
{
  return jamLocal.resourcesMounted && !jamLocal.pack.empty() ? jamLocal.pack/"lq1" : JamFs::path{};
}

bool jamDocumentResources(std::string& source)
{
  if (!jamEnabled() || !jamMapPath(pendingDocumentFile)) return false;
  if (jamLocal.pack.empty()) throw std::runtime_error("Download the Jam resources first.");
  const bool entering = !jamLocal.offlineProfile;
  if (entering) {
    jamLocal.offlineProfile = gameProfile;
    jamLocal.offlineMapFormat = q1MapFormat;
    jamLocal.offlineResourceDirectory = baseq3;
    gameMapModsKnown = false;
    gameMapResourcePaths.clear();gameMapResourceRoot.clear();
    q1MapWads.clear();q1MissingMapWads.clear();q1DocumentPak.clear();q1DocumentBspWad.clear();
  }
  gameProfile = 2;
  documentTerrain = false;
  q1MapFormat = ftebroom::quake1::valveMap(source) ? 1 : 0;
  if (!jamLocal.resourcesMounted) {
    jamLocal.resourcesMounted = true;
    if (!mountAssets()) {
      const auto error = assetError;
      jamRestoreOfflineProfile();
      mountAssets();
      throw std::runtime_error(error);
    }
  }
  std::size_t cursor=0;
  int depth=0;
  ftebroom::MapModToken key,value;
  while(ftebroom::mapModToken(source,cursor,key)){
    if(!key.quoted){
      if(key.value=="{")++depth;
      if(key.value=="}")--depth;
      continue;
    }
    if(depth!=1 || !ftebroom::mapModToken(source,cursor,value) || !value.quoted)continue;
    const auto name=entityLower(key.value);
    if(name!="wad" && name!="_wad" && name!="_tb_mod" && name!="_tb_def")continue;
    source.erase(key.begin,value.end-key.begin);
    cursor=key.begin;
  }
  return true;
}

bool jamPreviewBeforeCompile()
{
  if (!jamBeforeCompile(false)) return false;
  jamLocal.playAfterSave = jamLocal.fileDialog==JamFileDialog::Compile;
  return true;
}

void jamSaved(bool success)
{
  jamLocal.compileReady = jamLocal.compileAfterSave && success;
  jamLocal.uploadReady = jamLocal.uploadAfterSave && success;
  if (!success) jamLocal.playAfterSave = false;
  if (!success) jamLocal.publishPlayAfterSave = false;
  jamLocal.compileAfterSave = false;
  jamLocal.uploadAfterSave = false;
}

bool jamSaveNamed()
{
  auto title = std::string(jamLocal.mapName);
  const auto first = title.find_first_not_of(" \t");
  if (first!=title.npos) title = title.substr(first,title.find_last_not_of(" \t")-first+1);
  auto lower = entityLower(title);
  if (lower.size()>4 && lower.compare(lower.size()-4,4,".map")==0) {
    title.resize(title.size()-4);
    lower.resize(lower.size()-4);
  }
  const auto base = lower.substr(0,lower.find('.'));
  const bool reserved = base=="con" || base=="prn" || base=="aux" || base=="nul" ||
    (base.size()==4 && (base.compare(0,3,"com")==0 || base.compare(0,3,"lpt")==0) &&
     base.back()>='0' && base.back()<='9');
  const auto characters = std::count_if(title.begin(),title.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
  if (first==title.npos || characters>80 || title.size()>200 || title.front()=='.' || title.back()=='.' || title.back()==' ' ||
      title.find_first_of("/\\:*?\"<>|#")!=title.npos ||
      std::any_of(title.begin(),title.end(),[](unsigned char c){return c<32 || c==127;}) ||
      reserved || lower=="jam-draft") {
    jamLocal.fileError = jamText("Enter a map name of up to 80 characters without a path or reserved characters.",
                                 "Введи название карты до 80 символов без пути и запрещённых символов.");
    return false;
  }
  const auto target = jamLocal.workspace / JamFs::u8path(title+".map");
  if (!jamCanSaveTo(target,documentName)) {
    jamLocal.fileError = std::move(documentError);
    documentError.clear();
    return false;
  }
  std::error_code compareError;
  if (JamFs::exists(target) && !JamFs::equivalent(target,documentFile,compareError)) {
    jamLocal.fileError = jamText("A map with that name already exists on this plot.",
                                 "На этом участке уже есть карта с таким названием.");
    return false;
  }
  if (!documentSave(documentName,false,target)) return false;
  jamLocal.compileAfterSave = jamLocal.fileDialog==JamFileDialog::Compile;
  jamLocal.publishPlayAfterSave = jamLocal.fileDialog==JamFileDialog::Play;
  jamLocal.uploadAfterSave = jamLocal.fileDialog==JamFileDialog::Upload || jamLocal.publishPlayAfterSave;
  return true;
}

void jamOpenDraft()
{
  try {
    auto draft = jamDraft(jamLocal.selection);
    if (jamEditing() && jamLocal.draftPlot==jamLocal.selection) {jamLocal.open=false;return;}
    auto modified = JamFs::file_time_type::min();
    for (const auto& map : jamMaps()) {
      if (map.parent_path()!=draft.parent_path()) continue;
      const auto stamp = JamFs::last_write_time(map);
      if (stamp>modified) {modified=stamp;draft=map;}
    }
    if (!JamFs::exists(draft)) {
      auto bytes = jam::fileBytes(documentPath("maps/mapping-jam-start.map"), 64*1024);
      const auto wad = (jamLocal.pack / "lq1/texture-wads/lq_dev.wad").generic_u8string();
      const auto marker = bytes.find("@jam-wad@");
      if (marker == bytes.npos || wad.find_first_of("\"\n\r") != wad.npos)
        throw std::runtime_error("The Jam template or texture package is invalid.");
      bytes.replace(marker, 9, wad);
      jam::writeBytes(draft, bytes);
    }
    jamOpenMap(draft);
  } catch (const std::exception& error) { jamLocal.error = error.what(); }
}

bool jamPublishing()
{
  return jamLocal.snapshot || !jamLocal.operation.empty() || jamLocal.uploadAfterSave || jamLocal.uploadReady;
}

void jamBeginPublish(const std::string& plot, bool play = false)
{
  jamLocal.inspectLocalBuild = false;
  if (!jamLocal.canPublish) {
    jamLocal.error = jamText("Publishing is closed. You can compile your draft locally.",
                             "Публикация закрыта. Черновик можно скомпилировать локально.");
    return;
  }
  try {
    if (jamBusy() || jamPublishing() || savingDocument) return;
    if (plot != jamLocal.draftPlot || !jamEditing() || !jamOwnedPlot(plot) || !jamMapPath(documentFile) ||
        documentUntitled || gameProfile != 2)
      throw std::runtime_error(jamText("Open this plot's draft in Quake 1 first.","Сначала открой черновик этого участка в Quake 1."));
    const auto cell = jamCell(plot);
    const auto& publication = cell.at("publication");
    JamPublishedMap map{documentFile,plot,{},jam::sha256(jam::fileBytes(documentFile,32*1024*1024)),
                        documentGeneration,buildCurrentContentRevision(),play};
    JamUpload upload{jamRoot()/"snapshots"/jam::randomHex(12)/"source.map",documentName,
                     publication.is_null() ? JamJson(nullptr) : publication.at("revisionId"),std::move(map)};
    if (!documentSave(documentName, false, upload.path, true))
      throw std::runtime_error("Could not export the current MAP.");
    jamLocal.snapshot = std::move(upload);
    jamLocal.error.clear();
    jamLocal.status = jamText("Exporting a publish snapshot…","Экспорт снимка карты…");
  } catch (const std::exception& error) { jamLocal.error = error.what(); }
}

void jamRequestUpload(bool play = false)
{
  if (jamBusy() || jamPublishing() || jamLocal.child || savingDocument ||
      runActive || runExclusive || buildRunning || assetsLoading) return;
  if (!jamEditing() || jamLocal.client.token.empty() || !jamOwnedPlot(jamLocal.draftPlot)) return;
  if (play && cvars->GetFloat("ftebroom_bsp_preview")) buildEdit();
  if (documentUntitled || (!play && cvars->GetFloat("ftebroom_dirty"))) {
    jamFileRequest(play ? JamFileDialog::Play : JamFileDialog::Upload);
  } else if (play && cvars->GetFloat("ftebroom_dirty")) {
    if (documentSave(documentName,false,documentFile)) {
      jamLocal.uploadAfterSave = true;
      jamLocal.publishPlayAfterSave = true;
    } else jamLocal.error = documentError;
  } else jamBeginPublish(jamLocal.draftPlot,play);
}

void jamSendSnapshot(JamUpload upload)
{
  const auto rules = jamLocal.rules;
  const auto map = upload.map;
  jamStart(jamText("Uploading the MAP snapshot…","Отправка снимка MAP…"),
    [upload=std::move(upload),rules](auto client) {
      return jam::sendMap(client,upload.map.plot,rules,upload.baseRevision,upload.title,upload.path);
    },
    [map](auto value) {
      jamLocal.operation = value.at("operationId");
      jamLocal.publishingMap = map;
    });
}

void jamLaunchMap(const std::string& plot, bool hub, bool fromDraft, const std::string& expectedRevision = {});

void jamPollPublish()
{
  const auto operation = jamLocal.operation;
  jamStart(jamText("Checking publication…","Проверка публикации…"), [operation](auto client) {
    const auto result = client.json("GET", client.prefix + "operations/" + operation);
    return jamField(result, "status") == "published" ? JamJson{{"operation",result},{"result",client.board()}}
                                                      : JamJson{{"operation",result}};
  }, [](auto value) {
    const auto& result = value.at("operation");
    const auto phase = jamField(result,"status");
    if (phase == "published") {
      jamApplyBoard(value.at("result"));
      jamLocal.operation.clear();
      jamLocal.status = jamText("Map published.","Карта опубликована.");
      auto published = std::exchange(jamLocal.publishingMap,{});
      published.revision = jamField(result,"revisionId");
      if (jam::hexId(published.revision,32) && !published.draft.empty()) {
        try {
          const JamJson receipt{{"revisionId",published.revision},{"savedHash",published.savedHash}};
          jam::writeBytes(jamPublicationReceipt(published.draft),receipt.dump());
        } catch (const std::exception& error) {
          core->Print((std::string("ReyondBroom Jam: ")+jamDisplayMessage(error.what())+"\n").c_str());
        }
        if (jamMatchesDraft(published)) {
          jamLocal.publishedMap = published;
          if (published.play) jamLaunchMap(published.plot,false,true,published.revision);
          else jamShowCompiled(published);
        }
      }
    } else if (phase == "failed" || phase == "canceled" || phase == "expired") {
      jamLocal.operation.clear();
      jamLocal.publishingMap = {};
      jamLocal.error = result.contains("error") ? jamField(result.at("error"), "title") : phase;
      const auto code = result.contains("error") ? jamField(result["error"],"code") : "";
      if (code=="BUILD_FAILED" || code=="MAP_LEAK" || code=="INVALID_GEOMETRY") {
        jamLocal.inspectLocalBuild = true;
        if (code=="MAP_LEAK")
          jamLocal.error = jamText("The map leaks. Compile locally to see the route, then seal the opening before publishing.",
                                   "Карта негерметична. Запусти локальную проверку, чтобы увидеть маршрут, и закрой утечку перед публикацией.");
        else if (code=="INVALID_GEOMETRY")
          jamLocal.error = jamText("The map has a degenerate face. Compile locally to see its coordinates in the log.",
                                   "В карте есть вырожденная грань. Запусти локальную проверку, чтобы увидеть её координаты в журнале.");
        else if (jamLocal.error=="qbsp failed. Inspect the local build log.")
          jamLocal.error = jamText("The Jam server could not build the BSP. Compile locally to check for a leak and see the compiler error.",
                                   "Сервер Jam не смог собрать BSP. Запусти локальную проверку, чтобы найти утечку и увидеть ошибку компилятора.");
      }
      if (result.contains("error")) for (const auto& item : result["error"].value("violations",JamJson::array()))
        jamLocal.error += "\n" + jamField(item,"metric") + ": " + item.at("actual").dump() + " / " + item.at("limit").dump();
    } else {
      jamLocal.status = phase == "queued" ? jamText("Queued…","В очереди…")
                                          : jamText("Compiling the map…","Компиляция карты…");
    }
  });
}

void jamLaunchMap(const std::string& plot, bool hub, bool fromDraft, const std::string& expectedRevision)
{
  if (jamBusy() || jamLocal.child) return;
  const auto base = jamLocal.pack;
  const auto cache = jamRoot();
  const auto root = cache / "play" / jam::randomHex(12);
  const auto draft = fromDraft ? documentFile : JamFs::path{};
  jamLocal.playCanceled = false;
  jamStart(jamText("Downloading the published map…","Загрузка опубликованной карты…"),
    [plot,base,root,cache,hub,fromDraft,expectedRevision](auto client) {
      const auto pack = base.filename().u8string();
      const auto game = root / "lq1";
      if (hub) jamStageHub(client,game,cache,pack);
      else {
        const auto manifest = client.json("GET",client.prefix+"plots/"+plot+"/play-manifest");
        if (jamField(manifest,"mapStem")!="jam_map" || jamField(manifest,"packVersion")!=pack)
          throw std::runtime_error("The map requires a different runtime package.");
        if (!expectedRevision.empty() && jamField(manifest,"revisionId")!=expectedRevision)
          throw std::runtime_error("The published map changed before it could be launched.");
        jam::download(client,manifest,game,false);
      }
      for (const auto* file : {"pak0.pak","pak1.pak"}) {
        const auto source = base / "lq1" / file;
        if (JamFs::is_regular_file(source))
          JamFs::copy_file(source,game / file);
      }
      if (hub && fromDraft) {
        jam::writeBytes(game/"jam-return-menu.cfg","conmenu jam_return_action\nmenutext 64 81 \"MAPPING JAMS\"\n"
                       "menutext 64 111 \"Return to editing\" edit\n"
                       "menutext 64 135 \"Resume game\" resume\n");
        jam::writeBytes(game/"jam-return-action.cfg","if $option == edit then exec jam-return-to-draft.cfg\n"
                                                     "if $option == resume then menuclear\n");
        jam::writeBytes(game/"jam-return-to-draft.cfg","echo JAM_RETURN_TO_EDITING\nquit force\n");
        jam::writeBytes(game/"jam-return-setup.cfg","alias jam_return_action \"exec jam-return-action.cfg\"\n"
                                                   "bind F10 \"exec jam-return-menu.cfg\"\n"
                                                   "in 4 cprint \"F10 - Return to editing\"\n");
      }
      return JamJson{{"directory",root.u8string()}};
    }, [hub,draft](auto result) {
      if (jamLocal.playCanceled || (draft.empty() ? !jamLocal.open : documentFile!=draft)) {
        jamLocal.status = jamText("Download canceled.","Загрузка отменена.");
        return;
      }
      if (!draft.empty() && cvars->GetFloat("ftebroom_bsp_preview")) buildEdit();
      const auto root = JamFs::u8path(jamField(result,"directory"));
      const auto engine = runBundledEngine();
      if (!JamFs::is_regular_file(engine)) throw std::runtime_error("The trusted FTE engine is missing.");
      auto args = "-noupdates -nohome -quake -condebug -basedir " + modQuote(root.generic_u8string()) +
        " -game lq1 -window -width 1024 -height 768 +set sv_public 0 +set deathmatch 0 +set coop 0";
      if (std::getenv("REYONDBROOM_JAM_QA_STATE") && cvars->GetFloat("ftebroom_skip_setup"))
        args += " +set developer 1 +set log_developer 1";
      args += hub ? " +map jam_hub" : " +map jam_map";
      if (hub && !draft.empty()) args += " +exec jam-return-setup.cfg";
      if (std::getenv("REYONDBROOM_JAM_QA_STATE") && cvars->GetFloat("ftebroom_skip_setup"))
        args += hub && !draft.empty() ? " +in 5 exec jam-return-menu.cfg +in 6 exec jam-return-test.cfg"
                                       " +in 12 echo JAM_RETURN_FALLBACK +in 13 quit force"
                                     : " +in 7 quit force";
      if (hub && !draft.empty() && std::getenv("REYONDBROOM_JAM_QA_STATE") && cvars->GetFloat("ftebroom_skip_setup"))
        jam::writeBytes(root/"lq1"/"jam-return-test.cfg","menucallback edit\n");
      runStartProcess(engine.u8string(),args,root,true);
      jamLocal.child = runLaunchedEngines.back();
      runLaunchedEngines.pop_back();
      jamLocal.gameDraft = draft;
      jamLocal.status = jamText("Playing in the Jam mod.","Запущена игра в моде Jam.");
    });
}

void jamPlay(bool hub = false)
{
  jamLaunchMap(jamLocal.selection,hub,hub && jamEditing());
}

void jamResumeDraft()
{
  if (jamEditing() && !jamLocal.child) {
    if (cvars->GetFloat("ftebroom_bsp_preview")) buildEdit();
    jamLocal.open = false;
  }
}

void jamQuit()
{
  if (jamLocal.child || jamBusy() || jamPublishing() || savingDocument) return;
  jamLocal.open = false;
  if (jamEditing()) documentRequest("__jam_offline");
}

void jamSignOut()
{
  if (jamBusy()) return;
  if (jamEditing()) {
    jamLocal.open = false;
    documentRequest("__jam_signout");
    return;
  }
  jamStart(jamText("Signing out…","Выход…"), [](auto client) {
    return client.json("POST", "/v1/auth/logout", JamJson::object());
  }, [](auto) {
    jam::forgetSession(jamSessionPath());
    jamLocal.rememberSession=false;
    jamLocal.client.token.clear();
    jamLocal.client.prefix.clear();
    jamLocal.canPublish = false;
    jamLocal.account = jamLocal.rules = jamLocal.board = jamLocal.wallet = nullptr;
  jamLocal.players = JamJson::array();
  jamLocal.lastPresence = jamLocal.nextPresence = {};
    jamLocal.selection.clear();
    jamLocal.operation.clear();
    jamLocal.publishingMap = {};
    jamLocal.publishedMap = {};
    jamLocal.retryKey.clear();
    jamLocal.draftPlot.clear();
    jamLocal.avatarTextures.clear();
    jamLocal.screen = JamScreen::SignIn;
  });
}

void jamTick()
{
  if (!jamEnabled()) return;
  jamResumeSession();
  jamPresenceTick();
  if (jamLocal.compileReady) {
    if (!jamEditing() || (int)cvars->GetFloat("ftebroom_revision")!=saveRevision) {
      jamLocal.compileReady = false;
      jamLocal.playAfterSave = false;
    }
    else if (!cvars->GetFloat("ftebroom_dirty")) {
      jamLocal.compileReady = false;
      if (jamLocal.playAfterSave) modCompilePending = true;
      jamLocal.playAfterSave = false;
      buildBegin(jamLocal.compileWithLeaks);
      if (!buildRunning) modCompilePending = false;
    }
  }
  if (jamLocal.uploadReady) {
    if (!jamEditing() || (int)cvars->GetFloat("ftebroom_revision")!=saveRevision) {
      jamLocal.uploadReady = false;
      jamLocal.publishPlayAfterSave = false;
      jamLocal.error = jamText("The map changed while saving. Compile and upload it again.",
                               "Карта изменилась во время сохранения. Запусти сборку и загрузку снова.");
    } else if (!cvars->GetFloat("ftebroom_dirty") && !jamBusy()) {
      jamLocal.uploadReady = false;
      jamBeginPublish(jamLocal.draftPlot,std::exchange(jamLocal.publishPlayAfterSave,false));
    }
  }
  if (jamLocal.work.valid() && jamLocal.work.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto done = std::move(jamLocal.ready);
    try { auto result = jamLocal.work.get(); jamLocal.status.clear(); done(std::move(result)); }
    catch (const jam::Error& error) {
      if (!error.retryable) jamLocal.retryKey.clear();
      jamLocal.error = error.code == "RATE_LIMITED"
        ? jamText("Too many requests. Try again later.","Слишком много запросов. Попробуйте позже.")
        : error.what();
      jamLocal.status.clear();
      if (error.code == "PUBLISHING_CLOSED") jamLocal.canPublish = false;
      if (error.code == "AUTH_REQUIRED" || error.code == "ACCOUNT_RESTRICTED") {
        jam::forgetSession(jamSessionPath());
        jamLocal.rememberSession=false;
        jamLocal.client.token.clear();
        jamLocal.screen = JamScreen::SignIn;
        jamLocal.players = JamJson::array();
        jamLocal.lastPresence = {};
        jamLocal.operation.clear();
        jamLocal.publishingMap = {};
        jamLocal.confirm.clear();
        jamLocal.avatarTextures.clear();
      }
    } catch (const std::exception& error) { jamLocal.error = error.what(); jamLocal.status.clear(); }
  }
  if (!jamBusy() && !savingDocument && jamLocal.snapshot) {
    auto upload = std::move(*jamLocal.snapshot);
    jamLocal.snapshot.reset();
    try {
      if (!documentError.empty()) throw std::runtime_error(documentError);
      if (!jamMatchesDraft(upload.map))
        throw std::runtime_error(jamText("The map changed during export. Compile it again.",
                                        "Карта изменилась во время экспорта. Запусти сборку снова."));
      if (!JamFs::is_regular_file(upload.path)) throw std::runtime_error("The editor could not export the MAP.");
      jamSendSnapshot(std::move(upload));
    } catch (const std::exception& error) { jamLocal.error = error.what(); jamLocal.status.clear(); }
  }
  const auto now = std::chrono::steady_clock::now();
  if (!jamBusy() && !jamLocal.operation.empty() && now >= jamLocal.nextPoll) {
    jamLocal.nextPoll = now + std::chrono::seconds(1);
    jamPollPublish();
  }
  if (!jamLocal.child) return;
#ifdef _WIN32
  if (WaitForSingleObject(jamLocal.child, 0) != WAIT_OBJECT_0) return;
  DWORD exitCode = 1;
  GetExitCodeProcess(jamLocal.child, &exitCode);
  CloseHandle(jamLocal.child);
#else
  int exitCode = 0;
  if (waitpid(jamLocal.child, &exitCode, WNOHANG) == 0) return;
#endif
  jamLocal.child = 0;
  const bool returnToDraft = !jamLocal.gameDraft.empty() && jamEditing() && documentFile==jamLocal.gameDraft;
  jamLocal.gameDraft.clear();
  if (returnToDraft) jamLocal.open = false;
  jamLocal.status = exitCode ? jamText("The game ended with an error.","Игра завершилась с ошибкой.")
                             : returnToDraft ? jamText("Returned to editing.","Возврат к редактированию.")
                                             : jamText("Returned to the Jam board.","Возврат на поле Jam.");
  if (jamLocal.open) ImGui::SetWindowFocus("Mapping Jams###mapping-jams");
}
} // namespace
