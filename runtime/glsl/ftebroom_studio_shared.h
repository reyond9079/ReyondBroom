// SPDX-License-Identifier: MIT
// Original studio lighting, shared by ordinary and retained editor meshes.
#ifdef FRAGMENT_SHADER
vec3 reyondStudio(vec3 albedo,vec3 worldpos,vec3 eye){
    // Polygon soups do not carry normals. Derivatives also keep flat faces flat.
    vec3 n=normalize(cross(dFdx(worldpos),dFdy(worldpos)));
    vec3 v=normalize(eye-worldpos);
    if(dot(n,v)<0.0)n=-n;
    vec3 l=normalize(vec3(-0.45,-0.55,0.85));
    vec3 h=normalize(v+l);
    float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.001);
    float nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
    float a2=0.18,d=nh*nh*(a2-1.0)+1.0;
    float distribution=a2/(3.141593*d*d);
    float geometry=nl/(nl*0.35+0.65)*nv/(nv*0.35+0.65);
    float fresnel=0.04+0.96*pow(1.0-vh,5.0);
    float spec=distribution*geometry*fresnel/max(4.0*nl*nv,0.001);
    vec3 ambient=mix(vec3(0.23,0.22,0.21),vec3(0.48,0.56,0.68),n.z*0.5+0.5);
    vec3 colour=albedo*(ambient+vec3(0.82,0.77,0.68)*nl);
    colour+=vec3(1.0,0.96,0.9)*min(spec*nl,0.3);
    // Gentle shoulder, keeping existing diffuse textures recognizable.
    colour=colour/(vec3(0.35)+colour)*0.88;
    return colour;
}
#endif
