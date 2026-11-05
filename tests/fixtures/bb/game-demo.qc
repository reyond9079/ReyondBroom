// SPDX-License-Identifier: GPL-3.0-or-later
// Test-only Quake basemod extension. Export the monster template to progs/reyond_monster.mdl.

.float reyondSamples;
void() reyond_walk = {
 self.frame=21+floor(time*10)%10;
 self.nextthink=time+.1;
 self.reyondSamples+=1;
 if(self.reyondSamples==10||self.reyondSamples==15)_bprint("REYOND_MODEL_GAME_FRAME ",ftos(self.frame)," origin ",vtos(self.origin),"\n");
};
void() reyond_monster = {
 precache_model("progs/reyond_monster.mdl");
 setmodel(self,"progs/reyond_monster.mdl");
 setsize(self,'-16 -16 -24','16 16 32');
 self.movetype=MOVETYPE_NONE;self.solid=SOLID_BBOX;
 self.frame=21;self.think=reyond_walk;self.nextthink=time+.1;
};
