!!ver 400-450
!!cvar3f ftebroom_grid_color=0.666666667,0.666666667,0.666666667
!!cvarf ftebroom_grid_aa=1.005
!!cvarf ftebroom_grid_fade_start=512
!!cvarf ftebroom_grid_fade_end=1024
#include "sys/defs.h"
varying vec2 tc;
varying vec4 vc;
varying vec3 viewdir;
#ifdef VERTEX_SHADER
void main() {
    tc = v_texcoord;
    vc = v_colour;
    viewdir = e_eyepos - v_position.xyz;
    precise vec3 delta=e_eyepos-vec3(v_lmcoord.xy,v_lmcoord2.x);precise vec3 product=delta*v_normal.xyz;precise float side=(product.x+product.y)+product.z;
    gl_Position = side < -0.01 ? vec4(2.0,2.0,2.0,1.0) : ftetransform();
}
#endif
#ifdef FRAGMENT_SHADER
uniform vec3 cvar_ftebroom_grid_color;
uniform float cvar_ftebroom_grid_aa;
uniform float cvar_ftebroom_grid_fade_start;
uniform float cvar_ftebroom_grid_fade_end;
void main() {
    // Distance to the nearest grid line in screen pixels, independent of spacing.
    vec2 derivative = max(fwidth(tc), vec2(0.000001));
    float fadeEnd = max(0.0, cvar_ftebroom_grid_fade_end);
    float fadeStart = clamp(cvar_ftebroom_grid_fade_start, 0.0, max(0.0, fadeEnd-1.0));
    float distanceToEye = length(viewdir);
    if (fadeEnd > 0.0 && distanceToEye >= fadeEnd) discard;
    float distanceFade = fadeEnd > 0.0 ? 1.0-smoothstep(fadeStart, fadeEnd, distanceToEye) : 1.0;
    float feather = clamp(cvar_ftebroom_grid_aa, 0.05, 2.0);
    float edgeStart = max(0.0, 0.75-feather*0.5), edgeEnd = 0.75+feather*0.5;
    float density = max(derivative.x, derivative.y);
    float coverage;
    if(density<=0.5){
        // Resolved selected-grid lines dominate every coarser level. Keep this
        // common close-surface path to one AA evaluation and no LOD/parity math.
        vec2 distanceToLine = abs(fract(tc + 0.5) - 0.5) / derivative;
        coverage = 1.0-smoothstep(edgeStart,edgeEnd,min(distanceToLine.x,distanceToLine.y));
    }else{
        // Blend adjacent levels continuously without reducing coarse-grid
        // density. A parity-only fade would clip AA fringes at cell boundaries.
        float level = clamp(log2(4.0*density),0.0,18.0);
        float spacing = exp2(floor(level));
        vec2 coordinate = tc/spacing, width = derivative/spacing;
        vec2 distanceToLine = abs(fract(coordinate+0.5)-0.5)/width;
        vec2 nextDistance = abs(fract(coordinate*0.5+0.5)-0.5)/(width*0.5);
        float coarse = 1.0-smoothstep(edgeStart,edgeEnd,min(distanceToLine.x,distanceToLine.y));
        float nextCoarse = 1.0-smoothstep(edgeStart,edgeEnd,min(nextDistance.x,nextDistance.y));
        coverage = mix(coarse,nextCoarse,smoothstep(0.0,1.0,fract(level)));
        if(min(derivative.x,derivative.y)<1.0){
            // Preserve alpha.133's selected fine grid until it is subpixel.
            vec2 fineDistance = abs(fract(tc+0.5)-0.5)/derivative;
            vec2 fine = vec2(1.0)-smoothstep(vec2(edgeStart),vec2(edgeEnd),fineDistance);
            fine *= vec2(1.0)-smoothstep(vec2(0.5),vec2(1.0),derivative);
            coverage = max(max(fine.x,fine.y),coverage);
        }
    }
    gl_FragColor = vec4(clamp(cvar_ftebroom_grid_color,0.0,1.0), vc.a * coverage * distanceFade);
}
#endif
