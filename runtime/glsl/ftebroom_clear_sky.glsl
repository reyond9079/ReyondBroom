// SPDX-License-Identifier: MIT
!!ver 100-450
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
void main(){
    vec3 rd=normalize(direction),sun=normalize(vec3(-0.45,-0.55,0.85));
    float height=smoothstep(-0.12,0.95,rd.z);
    vec3 sky=mix(vec3(0.70,0.81,0.89),vec3(0.16,0.39,0.70),pow(height,0.65));
    float mu=max(dot(rd,sun),0.0);
    sky+=vec3(1.0,0.76,0.42)*pow(mu,24.0)*0.12;
    float disc=smoothstep(0.999975,0.999987,mu);
    sky=mix(sky,vec3(1.0,0.98,0.91),disc);
    sky=mix(sky,vec3(0.35,0.39,0.43),(1.0-smoothstep(-0.5,0.0,rd.z)));
    gl_FragColor=vec4(sky,1.0);
}
#endif
