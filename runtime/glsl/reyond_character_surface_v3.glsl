!!ver 120 450
!!permu SKELETAL
!!permu FRAMEBLEND
!!permu FOG
!!samps diffuse normalmap
// SPDX-License-Identifier: GPL-3.0-or-later
#include "sys/defs.h"
#include "sys/fog.h"

varying vec2 uv;
varying vec4 surfaceColor;
varying vec3 modelNormal;
varying vec3 modelPoint;

#ifdef VERTEX_SHADER
#ifndef PREVIEW
#include "sys/skeletal.h"
#endif
void main() {
    uv = v_texcoord;
    surfaceColor = v_colour;
#ifdef PREVIEW
    modelPoint = v_position.xyz;
    modelNormal = v_normal;
    gl_Position = ftetransform();
#else
    vec3 tangent, bitangent;
    gl_Position = skeletaltransform_wnst(modelPoint, modelNormal, tangent, bitangent);
#endif
}
#endif

#ifdef FRAGMENT_SHADER
const float PI = 3.14159265359;

vec3 srgbToLinear(vec3 value) {
    return mix(value / 12.92, pow((value + .055) / 1.055, vec3(2.4)),
               step(vec3(.04045), value));
}

vec3 linearToSrgb(vec3 value) {
    return mix(value * 12.92, 1.055 * pow(value, vec3(1.0 / 2.4)) - .055,
               step(vec3(.0031308), value));
}

mat3 surfaceFrame(vec3 n) {
    vec3 dpX = dFdx(modelPoint), dpY = dFdy(modelPoint);
    vec2 duvX = dFdx(uv), duvY = dFdy(uv);
    vec3 a = cross(dpY,n), b = cross(n,dpX);
    vec3 t = a*duvX.x + b*duvY.x;
    vec3 v = a*duvX.y + b*duvY.y;
    float weight = max(dot(t,t),dot(v,v));
    if (weight > 1e-14) {
        float lengthFactor = inversesqrt(weight);
        return mat3(t*lengthFactor,v*lengthFactor,n);
    }
    t = normalize(cross(n,abs(n.z)>.8 ? vec3(0,1,0) : vec3(0,0,1)));
    return mat3(t,cross(n,t),n);
}

float fresnel(float cosine,float f0) {
    return f0 + (1.0-f0)*pow(1.0-clamp(cosine,0.0,1.0),5.0);
}

float microfacet(vec3 n, vec3 v, vec3 l, float rough, float f0, mat3 basis) {
    float nl = max(dot(n,l),0.0);
    float nv = max(dot(n,v),.002);
    vec3 h = normalize(v+l);
    float nh = max(dot(n,h),0.0);
    float a = rough*rough;
    float a2 = a*a;
#ifdef HAIR
    // Texture V follows the strands. The broader U lobe makes the highlight
    // follow them as the view moves across the hair.
    float ax = a*1.65, ay = a*.57;
    float hx = dot(normalize(basis[0]),h)/ax;
    float hy = dot(normalize(basis[1]),h)/ay;
    float slope = hx*hx+hy*hy+nh*nh;
    float distribution = 1.0 / max(PI*ax*ay*slope*slope,1e-5);
#else
    float slope = nh*nh*(a2-1.0)+1.0;
    float distribution = a2/max(PI*slope*slope,1e-5);
#endif
    float ggxV = nl*sqrt(nv*nv*(1.0-a2)+a2);
    float ggxL = nv*sqrt(nl*nl*(1.0-a2)+a2);
    return nl * distribution * fresnel(dot(v,h),f0) * .5/max(ggxV+ggxL,.002);
}

vec3 characterLight(vec3 n, vec3 v, vec3 l, vec3 albedo, vec3 energy,
                    float rough, float f0, float lip, mat3 basis) {
    float nl = dot(n,l);
    float diffuse = max(0.0,nl);
    vec3 scatter = vec3(0.0);
#ifdef SKIN
    // A broad diffusion lobe gives skin a warm transition into shadow.
    float wrap = max(0.0,(nl+.36)/1.36) - diffuse;
    scatter = max(wrap,0.0) * vec3(.52,.22,.15)*(1.0-.4*lip);
#endif
    float spec = min(microfacet(n,v,l,rough,f0,basis),1.65);
#ifdef CLOTH
    spec *= .54;
    diffuse += .06 * pow(1.0-max(dot(n,v),0.0),2.0)*diffuse;
#endif
#ifdef HAIR
    spec = min(spec*.45,.27);
    scatter += vec3(.10,.066,.042)*max(0.0,(nl+.48)/1.48-diffuse);
#endif
    return energy * (albedo*(diffuse*(1.0-f0)+scatter) + vec3(spec));
}

void main() {
    vec4 texel = texture2D(s_diffuse,uv);
#ifdef HAIR
    if (texel.a < .055) discard;
#endif
    vec3 v = normalize(e_eyepos-modelPoint);
    vec3 n = normalize(dot(modelNormal,modelNormal)>.01 ? modelNormal :
                       cross(dFdx(modelPoint),dFdy(modelPoint)));
    if (dot(n,v)<0.0) n = -n;
    mat3 basis = surfaceFrame(n);
    vec3 albedo = srgbToLinear(clamp(texel.rgb*surfaceColor.rgb,0.0,1.0));
    float lip = 0.0;
    float rough = .65;
    float f0 = .036;
#ifdef SKIN
    lip = smoothstep(.53,.92,surfaceColor.a);
    float scalp = clamp(1.0-2.0*surfaceColor.a,0.0,1.0);
    vec2 detailUV = uv*mix(vec2(16.0),vec2(23.0,7.0),lip);
    vec4 detail = texture2D(s_normalmap,detailUV);
    float fade = 1.0-smoothstep(.012,.046,length(fwidth(detailUV)));
    vec3 offset = detail.xyz*2.0-1.0;
    n = normalize(n+(basis[0]*offset.x+basis[1]*offset.y)*fade*mix(.45,.62,lip));
    rough = clamp(mix(.52,.30,lip)+.16*(detail.a-.5)+.09*scalp,.24,.70);
    f0 = mix(.030,.040,lip);
#endif
#ifdef EYE
    // The base texture is cross-polarised; glare belongs to the convex cornea.
    rough = .21;
    f0 = .027;
    vec2 radial = uv-vec2(.511,.566);
    float iris = 1.0-smoothstep(.17,.24,length(radial));
    vec2 projectedView = vec2(dot(v,basis[0]),dot(v,basis[1]));
    vec3 recessedIris = texture2D(s_diffuse,uv+projectedView*.026).rgb;
    albedo = mix(albedo,srgbToLinear(recessedIris*surfaceColor.rgb),iris*.85);
    float lid = 1.0-.22*smoothstep(.55,.88,1.0-uv.y);
    albedo *= lid;
#endif
#ifdef HAIR
    // Resolve fibre grooves in close views, then remove them before they
    // cover less than a pixel. Colour stays in the artist's strand atlas.
    float strandU = uv.x*370.0 + uv.y*4.5;
    float strand = sin(strandU*6.2831853) + .35*sin(strandU*10.1664);
    float strandFade = 1.0-smoothstep(.09,.36,fwidth(strandU));
    n = normalize(n+.048*strand*strandFade*basis[0]);
    rough = .52 + .12*(1.0-texel.a) + .015*strand*strandFade;
    f0 = .028;
#endif
#ifdef CLOTH
    float frequency = 210.0;
    vec2 weave = uv*frequency*6.2831853;
    float visibility = 1.0-smoothstep(.7,2.6,length(fwidth(weave)));
    n = normalize(n+visibility*.054*(sin(weave.x)*basis[0]+sin(weave.y)*basis[1]));
    rough = .75;
    f0 = .022;
#endif
#ifdef PREVIEW
    vec3 key = normalize(vec3(-.70,.61,.43));
    vec3 fill = normalize(vec3(-.55,-.62,.31));
    vec3 rim = normalize(vec3(.48,-.22,.74));
    vec3 color = albedo*(vec3(.23,.24,.28)+.13*max(n.z,0.0));
    color += characterLight(n,v,key,albedo,vec3(.86,.80,.74),rough,f0,lip,basis);
    color += characterLight(n,v,fill,albedo,vec3(.26,.32,.40),min(rough+.10,1.0),f0,lip,basis);
    color += characterLight(n,v,rim,albedo,vec3(.24,.27,.34),min(rough+.08,1.0),f0,lip,basis);
#ifdef EYE
    vec3 reflection = reflect(-v,n);
    float windowReflection = smoothstep(.962,.994,dot(reflection,key));
    color += windowReflection*.24*vec3(.92,.95,1.0);
#endif
#else
    vec3 key = normalize(e_light_dir+vec3(-.0001,.0002,.0001));
    vec3 color = albedo*max(e_light_ambient, vec3(.025));
    color += characterLight(n,v,key,albedo,e_light_mul,rough,f0,lip,basis);
#endif
    color = color / (vec3(1.0) + .23*color);
    float opacity = e_colourident.a;
#ifdef HAIR
    opacity *= texel.a;
#endif
    gl_FragColor = fog4(vec4(linearToSrgb(clamp(color,0.0,1.0)),opacity));
}
#endif
