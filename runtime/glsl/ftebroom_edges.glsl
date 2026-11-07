!!ver 130-450
!!cvar3f ftebroom_color_brush_edges=0.65,0.65,0.7
#include "sys/defs.h"
varying vec4 vc;
#ifdef VERTEX_SHADER
void main(){vc=v_colour;gl_Position=ftetransform();}
#endif
#ifdef FRAGMENT_SHADER
uniform vec3 cvar_ftebroom_color_brush_edges;
void main(){gl_FragColor=vec4(clamp(cvar_ftebroom_color_brush_edges,0.0,1.0),vc.a);}
#endif
