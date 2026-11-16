// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace {
using JamPoseClock=std::chrono::steady_clock;
struct JamPoseSample { double time; std::array<float,4> pose; };
struct JamPoseTrack {
  std::string account,nickname,document;
  std::vector<JamPoseSample> samples;
  std::int64_t sequence=0;
  double received=0, cursor=0;
  bool fast=false;
};
struct JamAvatar { int slot; std::string account,nickname; std::array<float,4> pose; };
struct JamPoseReply { JamJson body; JamPoseClock::time_point arrived; };
struct JamPoseLane {
  std::future<JamPoseReply> work;
  std::shared_ptr<jam::PoseHttpConnection> connection;
  JamPoseClock::time_point sent;
  unsigned generation=0;
};
struct JamPoseState {
  std::array<JamPoseLane,3> lanes;
  std::map<std::string,JamPoseTrack> tracks;
  std::vector<JamAvatar> avatars;
  std::string session,document,drawn;
  JamPoseClock::time_point next;
  std::int64_t sequence=0;
  double rtt=0;
  unsigned generation=0,requests=0,accepted=0;
} jamPose;

double jamPoseTime(JamPoseClock::time_point time)
{
  return std::chrono::duration<double>(time.time_since_epoch()).count();
}

std::array<float,4> jamPoseInterpolate(const std::vector<JamPoseSample>& points, double target)
{
  const auto to=std::upper_bound(points.begin(),points.end(),target,[](double time,const auto& a){return time<a.time;});
  if(to==points.end()) return points.back().pose;
  if(to==points.begin()) return to->pose;
  const auto& from=*(to-1);
  const float alpha=static_cast<float>(std::clamp((target-from.time)/(to->time-from.time),0.0,1.0));
  auto result=from.pose;
  for(int axis=0;axis<3;++axis) result[axis]+=(to->pose[axis]-from.pose[axis])*alpha;
  result[3]=std::remainder(from.pose[3]+std::remainder(to->pose[3]-from.pose[3],360.f)*alpha,360.f);
  return result;
}

void jamPoseAccept(const std::string& plot,const JamJson& item,const JamJson& values,int offset,
                   std::int64_t sequence,double sampled,double received,bool fast)
{
  if(plot.size()!=2 || plot[0]<'A' || plot[0]>'E' || plot[1]<'1' || plot[1]>'5') return;
  const auto account=jamField(item,"accountId"), document=jamField(item,"document");
  if(!jam::hexId(account,32) || (fast && !jam::hexId(document,32))) return;
  if(!values.is_array() || values.size()!=std::size_t(offset+4)) return;
  std::array<float,4> pose{};
  for(int axis=0;axis<4;++axis) {
    if(!values[axis+offset].is_number())return;
    pose[axis]=values[axis+offset].get<float>();
    if(!std::isfinite(pose[axis]) || std::abs(pose[axis])>(axis==3?360.f:32768.f))return;
  }
  auto& track=jamPose.tracks[plot];
  if(!fast && track.fast && received-track.received<1.4) return;
  if(track.account!=account || track.fast!=fast || (fast && track.document!=document)) {
    // Ignore an older response from a lane that completed after a document change.
    if(fast && track.fast && sampled<track.received && !track.samples.empty() && sampled<=track.samples.back().time) return;
    track={};
  }
  if(fast && sequence<=track.sequence) return;
  track.account=account;track.nickname=jamField(item,"nickname");track.document=document;
  track.sequence=sequence;track.fast=fast;track.received=received;
  if(!track.samples.empty()) {
    const auto& previous=track.samples.back();
    const double dx=pose[0]-previous.pose[0],dy=pose[1]-previous.pose[1],dz=pose[2]-previous.pose[2];
    const double gap=sampled-previous.time;
    const double jumpLimit=std::max(360.0,11000*gap);
    if(gap>1.5 || dx*dx+dy*dy+dz*dz>jumpLimit*jumpLimit) track.samples.clear();
    else sampled=std::max(sampled,previous.time+.001);
  }
  track.samples.push_back({sampled,pose});
  if(track.samples.size()>24) track.samples.erase(track.samples.begin());
  if(fast) ++jamPose.accepted;
}

void jamPoseLegacy(const JamJson& state,JamPoseClock::time_point now)
{
  const auto time=jamPoseTime(now);
  for(const auto& item:state.at("plots")) if(item.contains("player")) {
    const auto& player=item.at("player");
    jamPoseAccept(jamField(item,"plot"),player,player.at("pose"),0,0,time,time,false);
  }
}

void jamPoseTick(bool editing,const jam::Client& client,const std::string& document,const std::string& plot,
                 int interval,JamPoseClock::time_point now)
{
  using namespace std::chrono;
  const auto liveDocument=editing?document:std::string{};
  if(jamPose.session!=client.token || jamPose.document!=liveDocument) {
    ++jamPose.generation;
    jamPose.session=client.token;jamPose.document=liveDocument;
    jamPose.tracks.clear();jamPose.next={};jamPose.sequence=0;jamPose.rtt=0;
  }
  const auto time=jamPoseTime(now);
  JamPoseLane* free=nullptr;
  for(auto& lane:jamPose.lanes) {
    if(lane.work.valid() && lane.work.wait_for(milliseconds(0))==std::future_status::ready) {
      try {
        auto reply=lane.work.get();
        if(editing && lane.generation==jamPose.generation && reply.body.contains("players")) {
          const double arrival=jamPoseTime(reply.arrived),sent=jamPoseTime(lane.sent);
          const double rtt=std::clamp(arrival-sent,0.0,.35);
          jamPose.rtt=jamPose.rtt ? .9*jamPose.rtt+.1*rtt : rtt;
          const double midpoint=(sent+arrival)*.5;
          for(const auto& item:reply.body.at("players")) {
            const auto plot=jamField(item,"plot");
            for(const auto& sample:item.at("samples")) {
              if(!sample.is_array() || sample.size()!=6 || !sample[0].is_number_integer() || !sample[1].is_number_integer())continue;
              const auto age=std::clamp(sample[1].get<int>(),0,2000);
              jamPoseAccept(plot,item,sample,2,sample[0].get<std::int64_t>(),midpoint-age*.001,arrival,true);
            }
          }
        }
      } catch(const std::exception&) {
        lane.connection.reset();
      }
    }
    if(!lane.work.valid() && !free) free=&lane;
  }
  if(editing && interval>=65 && interval<=100 && free && now>=jamPose.next) {
    const auto duration=milliseconds(interval);
    jamPose.next=std::max(jamPose.next+duration,now-duration/2);
    if(jamPose.next<=now) jamPose.next+=duration;
    char camera[512]{};float pose[7]{};
    cvars->GetString("ftebroom_camera_state",camera,sizeof camera);
    if(cameraParse(camera,pose)) {
      for(int i=0;i<3;++i) pose[i]=std::isfinite(pose[i])?std::clamp(pose[i],-32768.f,32768.f):0;
      const float yaw=std::isfinite(pose[4])?std::remainder(pose[4],360.f):0;
      JamJson after=JamJson::object();
      for(const auto& [remote,track]:jamPose.tracks)if(track.fast)
        after[remote]={track.document,track.sequence};
      const auto data=JamJson{{"document",document},{"sequence",++jamPose.sequence},{"after",std::move(after)},
                              {"pose",{pose[0],pose[1],pose[2],yaw}}}.dump();
      free->generation=jamPose.generation;free->sent=now;
      if(!free->connection)free->connection=std::make_shared<jam::PoseHttpConnection>();
      free->work=std::async(std::launch::async,[client,data,connection=free->connection] {
        auto response=jam::request(client.endpoint,"POST",client.prefix+"world/pose",client.token,{},data,32*1024,nullptr,connection.get());
        return JamPoseReply{response.status==200 ? JamJson::parse(response.body) : JamJson(),JamPoseClock::now()};
      });
      ++jamPose.requests;
    }
  }

  jamPose.avatars.clear();
  std::ostringstream model;
  model.imbue(std::locale::classic());model<<std::setprecision(7);
  if(editing) for(auto it=jamPose.tracks.begin();it!=jamPose.tracks.end();) {
    auto& [remote,track]=*it;
    if(time-track.received>3) {it=jamPose.tracks.erase(it);continue;}
    ++it;
    if(remote==plot || track.samples.empty() || track.account==jamField(jamLocal.account,"accountId"))continue;
    const int slot=(remote[0]-'A')*5+remote[1]-'1';
    const double buffer=track.fast ? .15+.65*jamPose.rtt : .82;
    track.cursor=std::max(track.cursor,time-buffer);
    auto pose=jamPoseInterpolate(track.samples,track.cursor);
    pose[0]+=float(remote[1]-plot[1])*2048;pose[1]-=float(remote[0]-plot[0])*2048;
    jamPose.avatars.push_back({slot,track.account,track.nickname,pose});
    model<<slot<<' '<<pose[0]<<' '<<pose[1]<<' '<<pose[2]-20<<' '<<pose[3]<<';';
  }
  const auto rendered=model.str();
  if(rendered!=jamPose.drawn) {
    jamPose.drawn=rendered;
    cvars->SetString("ftebroom_jam_players",jamPose.drawn.c_str());
  }
}
} // namespace
