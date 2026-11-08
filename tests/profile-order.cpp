// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/ftebroom_profile_order.h"
#include "../native/ftebroom_run_profile.h"
#include <cassert>
#include <iostream>
int main(){
 std::vector<RunEngineProfile> engines={{"z","zulu","engine-z","z"},{"first","Alpha","engine-1","one"},{"second","alpha","engine-2","two"}};
 int selected=2;
 assert(ftebroom::reorderProfiles(engines,selected,"sort"));
 assert(engines[0].id=="first"&&engines[1].id=="second"&&engines[2].id=="z"&&selected==1);
 assert(engines[selected].path=="engine-2"&&engines[selected].parameters=="two");
 assert(ftebroom::reorderProfiles(engines,selected,"up")&&selected==0&&engines[0].id=="second");
 assert(!ftebroom::reorderProfiles(engines,selected,"up")&&selected==0);
 assert(ftebroom::reorderProfiles(engines,selected,"down")&&selected==1&&engines[1].id=="second");
 std::vector<RunProfile> profiles(3);profiles[0].name="same";profiles[1].name="beta";profiles[2].name="same";
 for(int i=0;i<3;i++)profiles[i].work="work-"+std::to_string(i);
 selected=2;assert(ftebroom::reorderProfiles(profiles,selected,"sort"));
 assert(selected==2&&profiles[selected].work=="work-2"&&profiles[1].work=="work-0");
 assert(!ftebroom::reorderProfiles(profiles,selected,"down")&&selected==2);
 selected=-1;assert(!ftebroom::reorderProfiles(profiles,selected,"up"));
 std::vector<RunProfile> empty;assert(!ftebroom::reorderProfiles(empty,selected,"sort")&&selected==-1);
 std::cout<<"PASS stable duplicate-name order, payload/selection identity, moves and boundaries\n";
}
