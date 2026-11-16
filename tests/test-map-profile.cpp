#include "../native/ftebroom_map_profile.h"
#include <cassert>
int main(){using ftebroom::mapProfile;
 assert(mapProfile("// FTEBroom profile=quake1 format=valve220").profile==2);
 assert(mapProfile("// FTEBroom profile=when-the-sky-went-dark format=fte-terrain").terrain);
 assert(mapProfile("// FTEBroom profile=quake3 format=quake3").profile==0);
 assert(mapProfile("// Game: Quake\n// Format: Standard\n{\n}").profile==2);
 assert(mapProfile("// Game: Quake\r\n// Format: Valve\r\n{").format==1);
 assert(mapProfile("\xef\xbb\xbf// Game: Quake 3\n// Format: Quake3 (Valve)\n{").profile==0);
 assert(mapProfile("// Game: When the sky went dark\n// Format: Quake3\n{").profile==1);
 assert(mapProfile("// Game: Half-Life\n// Format: Valve\n{").profile==4);
 assert(mapProfile("// Game: Quake2RE\n// Format: Quake2 (Valve)\n{").profile==3);
 assert(mapProfile("// Game: objectivesurvive\n// Format: Quake2 (Valve)\n{").profile==3);
 assert(mapProfile("// Game: Quake2\n// Format: Valve\n{").profile==3);
 assert(mapProfile("// Format: Valve\n{").profile==-1);
 assert(mapProfile("{\n// Game: Quake\n// Format: Standard").profile==-1);
 assert(mapProfile("// ReyondBroom profile=when-the-sky-went-dark format=quake3\n// Game: Quake\n// Format: Standard").profile==1);
 assert(mapProfile("// ReyondBroom profile=when-the-sky-went-dark format=fte-terrain").terrain);
 using ftebroom::mapDocumentHeader;
 assert(mapDocumentHeader(2,true,false,true).find("// Game: Quake\n// Format: Valve\n")==0);
 assert(mapDocumentHeader(2,false,false,true).find("// Game: Quake\n// Format: Standard\n")==0);
 assert(mapDocumentHeader(3,true,false,true).find("// Game: Quake 2\n// Format: Quake2 (Valve)\n")==0);
 assert(mapDocumentHeader(4,true,false,true).find("// Game: Half-Life\n// Format: Valve\n")==0);
 auto fte=mapDocumentHeader(1,true,false,true);
 assert(fte.find("// Game: Quake 3\n// Format: Quake3 (Valve)\n")==0 && mapProfile(fte).profile==1);
 auto terrain=mapDocumentHeader(1,true,true,true);
 assert(terrain.find("// Game:")==terrain.npos && mapProfile(terrain).terrain);
 assert(mapDocumentHeader(1,true,false,false).find("// Game:")==std::string::npos);
 using ftebroom::trenchBroomMapCompatible;
 assert(trenchBroomMapCompatible("{\n\"message\" \"patchDef3\"\n}",1));
 assert(trenchBroomMapCompatible("\tpatchDef2\n\t{\n\t\t\"textures/stone wall\"\n( 3 3 0 0 0 )",1));
 assert(!trenchBroomMapCompatible("\tpatchDef2\n\t{\n\t\t\"texture\"\n( 3 5 0 0 0 )",1));
 assert(trenchBroomMapCompatible("\tpatchDef2\n\t{\n\t\t\"texture\"\n( 3 5 0 0 0 )",1,true));
 assert(!trenchBroomMapCompatible("\tpatchDef2\n\t{\n\t\t\"texture\"\n( 4 5 0 0 0 )",1,true));
 assert(!trenchBroomMapCompatible("terrain\n{\n}",1,true));
 assert(!trenchBroomMapCompatible("\tpatchDef2WS\n",0));
 assert(!trenchBroomMapCompatible("\tpatchDef3\n",1));
 assert(!trenchBroomMapCompatible("\tpatchDef2\n",3));
}
