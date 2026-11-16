!!ver 120 450
!!permu SKELETAL
!!permu FRAMEBLEND
!!permu FOG
!!samps diffuse
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/defs.h"
#include "sys/fog.h"
varying vec2 uv;
varying vec4 faceColor;
varying vec3 normalWS;
varying vec3 modelPoint;

#ifdef VERTEX_SHADER
#ifndef PREVIEW
#include "sys/skeletal.h"
#endif
void main() {
    uv = v_texcoord;
    faceColor = v_colour;
#ifdef PREVIEW
    modelPoint = v_position.xyz;
    normalWS = vec3(0.0);
    gl_Position = ftetransform();
#else
    vec3 s, t;
    gl_Position = skeletaltransform_wnst(modelPoint, normalWS, s, t);
#endif
}
#endif

#ifdef FRAGMENT_SHADER
// Cell noise is filtered out with screen distance, keeping the pore detail
// from shimmering on distant character models.
float cell(vec2 p) {
    return fract(sin(dot(p, vec2(37.17, 73.19))) * 10291.73);
}
float heightField(vec2 p) {
    vec2 base = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(cell(base), cell(base+vec2(1,0)), f.x),
               mix(cell(base+vec2(0,1)), cell(base+vec2(1,1)), f.x), f.y);
}
void main() {
    vec3 albedo = texture2D(s_diffuse, uv).rgb * faceColor.rgb;
    float lip = smoothstep(0.10, 0.76, faceColor.a);
    vec2 detailUV = uv * vec2(mix(680.0, 520.0, lip), mix(680.0, 125.0, lip));
    float detailWeight = 1.0 - smoothstep(0.48, 1.50, length(fwidth(detailUV)));
    float detail = (heightField(detailUV)-0.5)*detailWeight;
    vec3 viewDirection = normalize(e_eyepos-modelPoint);
#ifdef PREVIEW
    vec3 n = normalize(cross(dFdx(modelPoint),dFdy(modelPoint)));
    if (dot(n, viewDirection)<0.0) n = -n;
    vec3 l = normalize(vec3(-0.73, 0.45, 0.51));
    vec3 energy = vec3(0.78, 0.80, 0.83);
#else
    vec3 n = normalize(normalWS);
    vec3 l = normalize(e_light_dir + vec3(-0.002, 0.0, 0.001));
    vec3 energy = max(e_light_mul, vec3(0.10));
#endif
    float ndl = dot(n,l);
    vec3 halfDirection = normalize(l+viewDirection);
    float h = max(0.0,dot(n,halfDirection));
    float roughness = clamp(mix(0.58,0.38,lip) + detail*.23, 0.27, 0.71);
    float gloss = pow(h,mix(72.0,11.0,roughness));
    float fresnel = pow(1.0-max(0.0,dot(n,viewDirection)),5.0);
    float sheen = gloss * (0.029+0.09*lip) * (0.62+0.38*max(ndl,0.0));
    sheen += fresnel * (0.010+0.014*lip);
    albedo *= 1.0 + detail*mix(0.070,0.10,lip);
    float transmission = max(0.0, clamp((ndl+0.36)/1.36,0.0,1.0)-max(ndl,0.0));
#ifdef PREVIEW
    vec3 lit = albedo * (1.0 + transmission*vec3(.20,.08,.04));
#else
    vec3 diffuse = max(e_light_ambient, vec3(.045)) + max(ndl,0.0)*e_light_mul;
    vec3 lit = albedo * (diffuse + transmission*energy*vec3(.38,.16,.075));
#endif
    gl_FragColor = fog4(vec4(clamp(lit + sheen*energy, 0.0, 1.0),e_colourident.a));
}
#endif
