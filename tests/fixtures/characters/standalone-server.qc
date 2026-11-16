// SPDX-License-Identifier: GPL-3.0-or-later
#include "csplat.qc"

void() main = {};
void() SetNewParms = {};
void() SetChangeParms = {};
void() StartFrame = {};
void() PlayerPreThink = {};
void() PlayerPostThink = {};
void() ClientConnect = {};
void() ClientDisconnect = {};
void() ClientKill = {};
void() worldspawn = { lightstyle(0, "m"); };
void() light = {};
void() info_player_start = {};

void() PutClientInServer = {
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    setorigin(self, '-154 0 68');
};
