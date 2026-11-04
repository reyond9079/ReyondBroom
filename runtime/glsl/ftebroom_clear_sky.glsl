// SPDX-License-Identifier: MIT
!!ver 100-450
!!cvar3f ftebroom_sky_color=0.16,0.39,0.70
#include "sys/defs.h"
// Original analytic cloudless atmosphere for the editor background.
varying vec3 direction;
#ifdef VERTEX_SHADER
void main(){
    direction=v_position.xyz-e_eyepos;
    gl_Position=ftetransform();
    gl_Position.z=gl_Position.w*0.999999;
}
#endif
#ifdef FRAGMENT_SHADER
uniform vec3 cvar_ftebroom_sky_color;
void main(){
    vec3 tint=clamp(cvar_ftebroom_sky_color,0.0,1.0)/vec3(0.16,0.39,0.70);
    float brightness=clamp(dot(cvar_ftebroom_sky_color,vec3(0.2126,0.7152,0.0722))/0.363484,0.0,1.0);
    vec3 rd=normalize(direction),sun=normalize(vec3(-0.45,-0.55,0.85));
    float height=smoothstep(-0.12,0.95,rd.z);
    vec3 sky=mix(clamp(vec3(0.70,0.81,0.89)*tint,0.0,1.0),cvar_ftebroom_sky_color,pow(height,0.65));
    float mu=max(dot(rd,sun),0.0);
    sky+=vec3(1.0,0.76,0.42)*pow(mu,24.0)*0.12*brightness;
    float disc=smoothstep(0.999975,0.999987,mu);
    sky=mix(sky,vec3(1.0,0.98,0.91)*brightness,disc);
    sky=mix(sky,clamp(vec3(0.35,0.39,0.43)*tint,0.0,1.0),(1.0-smoothstep(-0.5,0.0,rd.z)));
    gl_FragColor=vec4(sky,1.0);
}
#endif
