!!ver 400-450
!!cvar3f ftebroom_color_brush_edges=0.65,0.65,0.7
#include "sys/defs.h"
varying vec4 vc;
#ifdef VERTEX_SHADER
void main(){vc=v_colour;precise vec3 delta=e_eyepos-vec3(v_lmcoord.xy,v_lmcoord2.x);precise vec3 product=delta*v_normal.xyz;precise float side=(product.x+product.y)+product.z;gl_Position=side < -0.01 ? vec4(2.0,2.0,2.0,1.0) : ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
uniform vec3 cvar_ftebroom_color_brush_edges;
void main(){gl_FragColor=vec4(clamp(cvar_ftebroom_color_brush_edges,0.0,1.0),vc.a);}
#endif
