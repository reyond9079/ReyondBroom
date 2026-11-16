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
vec3 rollOffHighlights(vec3 color) {
    float peak=max(max(color.r,color.g),color.b);
    float shoulder=max(0.0,peak-0.76);
    float mapped=peak-shoulder+shoulder/(1.0+1.44*shoulder);
    return color*(mapped/max(0.0001,peak));
}
void main() {
    vec3 albedo = texture2D(s_diffuse, uv).rgb * faceColor.rgb;
    float lip = smoothstep(0.55, 0.88, faceColor.a);
    float scalp = clamp(1.0 - 2.0*faceColor.a, 0.0, 1.0);
    vec2 detailUV = uv * vec2(mix(680.0, 520.0, lip), mix(680.0, 125.0, lip));
    float detailWeight = 1.0 - smoothstep(0.48, 1.50, length(fwidth(detailUV)));
    float detail = (heightField(detailUV)-0.5)*detailWeight;
    // Pigmentation varies at a larger scale than pores and follows the UVs.
    float pigment = heightField(uv * 146.0)-0.5;
    float faceLight = smoothstep(.24,.76,dot(albedo,vec3(.2126,.7152,.0722)));
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
    float roughness = clamp(mix(0.63,0.51,lip) + detail*.25+pigment*.12, 0.34, 0.78);
    float gloss = pow(h,mix(72.0,11.0,roughness));
    float fresnel = pow(1.0-max(0.0,dot(n,viewDirection)),5.0);
    float sheen = gloss * (0.024+0.034*lip) * (0.62+0.38*max(ndl,0.0));
    sheen += fresnel * (0.005+0.009*lip);
    albedo *= 1.0 + detail*mix(0.10,0.11,lip);
    albedo *= 1.0 + pigment * mix(vec3(.025,.038,.044),vec3(.049,.085,.095),faceLight)*(1.0-lip);
    // A few tapered fibres remain visible at the softly tinted scalp edge.
    vec2 rootUV = vec2(uv.y*690.0 + .72*sin(uv.x*82.0),uv.x*155.0);
    float rootAA = 1.0-smoothstep(.56,1.65,length(fwidth(rootUV)));
    float fibres = smoothstep(.60,.92,heightField(rootUV))*rootAA;
    float rootEdge = smoothstep(.13,.53,scalp)*(1.0-smoothstep(.54,.96,scalp));
    albedo *= 1.0 - .21*rootEdge*fibres;
    float transmission = max(0.0, clamp((ndl+0.36)/1.36,0.0,1.0)-max(ndl,0.0));
#ifdef PREVIEW
    vec3 lit = albedo * (1.0 + transmission*vec3(.13,.058,.052));
#else
    vec3 diffuse = max(e_light_ambient, vec3(.045)) + max(ndl,0.0)*e_light_mul;
    vec3 lit = albedo * (diffuse + transmission*energy*vec3(.31,.14,.09));
#endif
    gl_FragColor = fog4(vec4(clamp(rollOffHighlights(lit + sheen*energy),0.0,1.0),e_colourident.a));
}
#endif
