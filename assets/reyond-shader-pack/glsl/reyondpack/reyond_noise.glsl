// SPDX-License-Identifier: MIT
#ifndef REYOND_NOISE
#define REYOND_NOISE
uint RynBits(ivec3 cell){
 uvec3 u=uvec3(cell);
 uint h=(u.x*15485863u)^(u.y*32452843u)^(u.z*49979687u)^982451653u;
 h^=h<<7;h^=h>>9;h*=2654435761u;h^=h>>11;
 return h;
}
float RynRandom(ivec3 cell){return float(RynBits(cell)>>8)*(1./16777216.);}
float RynNoise(vec3 p){
 ivec3 cell=ivec3(floor(p));vec3 f=fract(p);
 vec3 w=f*f*f*(f*(f*6.-15.)+10.);
 float a=mix(RynRandom(cell),RynRandom(cell+ivec3(1,0,0)),w.x);
 float b=mix(RynRandom(cell+ivec3(0,1,0)),RynRandom(cell+ivec3(1,1,0)),w.x);
 float c=mix(RynRandom(cell+ivec3(0,0,1)),RynRandom(cell+ivec3(1,0,1)),w.x);
 float d=mix(RynRandom(cell+ivec3(0,1,1)),RynRandom(cell+ivec3(1,1,1)),w.x);
 return mix(mix(a,b,w.y),mix(c,d,w.y),w.z);
}
#endif
