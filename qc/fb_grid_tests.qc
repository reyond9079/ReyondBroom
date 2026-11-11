// SPDX-License-Identifier: GPL-2.0-or-later
void fb_grid_expect(int condition,string name)
{
    print(strcat(condition?"FTEBROOM_GRID_PASS ":"FTEBROOM_GRID_FAIL ",name,"\n"));
}
float fb_grid_testcommand()
{
    if(argv(0)!="ftebroom_test_grid") return FALSE;
    if(!cvar("ftebroom_skip_setup")) return TRUE;
    string mode=argv(1);
    if(mode=="expect") {
        fb_grid_expect(fabs(fb_grid()-stof(argv(2)))<0.00001,argv(3));
    } else if(mode=="math") {
        cvar_set("ca_grid","0.125");cvar_set("ftebroom_grid_snap","1");
        fb_grid_expect(fabs(fb_snap_scalar(-0.18)+0.125)<0.00001,"negative_fraction");
        fb_grid_expect(fb_snap_up(-0.18)==-0.125 && fb_snap_down(-0.18)==-0.25,"up_down");
        vector normal=normalize('1 2 3'),point='2.31 -4.71 8.17';
        vector snapped=fb_snap_to_plane(point,normal,7);
        fb_grid_expect(fabs(snapped*normal-7)<0.001,"inclined_plane");
        fb_grid_expect(fabs(snapped_x/fb_grid()-rint(snapped_x/fb_grid()))<0.001,"plane_grid_axis");
        static vector vertices[1];vertices[0]='0.1 0 0';
        fb_grid_expect(fabs(fb_snap_move_distance(0.027,vertices,1,'1 0 0')-0.025)<0.001,"extrude_vertex_candidate");
        fb_grid_expect(fb_snap_angle(44,15)==45,"angle");
        cvar_set("ftebroom_grid_rotation","37");
        vector rotated=fb_grid_local(fb_snap_vec(point));
        fb_grid_expect(fabs(rotated_x/fb_grid()-rint(rotated_x/fb_grid()))<0.001&&fabs(rotated_y/fb_grid()-rint(rotated_y/fb_grid()))<0.001,"rotated_lattice");
        fb_grid_expect(fabs(fb_snap_to_plane(point,normal,7)*normal-7)<0.001,"rotated_plane_constraint");
        cvar_set("ftebroom_grid_rotation","0");
        cvar_set("ftebroom_grid_snap","0");
        fb_grid_expect(fb_snap_vec(point)==point && fb_snap_delta(point)==point,"snap_disabled");
        fb_grid_expect(fb_snap_to_plane(point,normal,7)==point,"plane_disabled");
        fb_grid_expect(fb_snap_up(0.17)==0.17 && fb_snap_down(0.17)==0.17 && fb_snap_angle(44,15)==44,"rounding_disabled");
        fb_grid_expect(fb_snap_move_distance(0.027,vertices,1,'1 0 0')==0.027,"extrude_disabled");
        cvar_set("ftebroom_grid_snap","1");cvar_set("ca_grid","16");
    } else if(mode=="fractional") {
        cvar_set("ca_grid","0.125");
        fb_grid_expect(fb_createbox('0 0 0.0625','0.125 0.125 0.125')!=0,"fractional_brush");
    } else if(mode=="unsnapped") {
        cvar_set("ftebroom_grid_snap","0");cvar_set("ca_grid","16");
        static vector bounds[2];
        brush_getfacepoints(selectedbrushes[0].model,selectedbrushes[0].id,0,bounds,2);
        vector pixel=project((bounds[0]+bounds[1])*0.5);pixel_z=0;
        fb_begin(pixel,0,cvar("ca_brush_view"));fb_finish(pixel+'37 19 0',0,cvar("ca_brush_view"));
        fb_grid_expect(fabs(fb_offset_x-rint(fb_offset_x))>0.001 || fabs(fb_offset_y-rint(fb_offset_y))>0.001,"fractional_mouse_offset");
    } else if(mode=="flags") {
        fb_grid_expect(cvar("ftebroom_grid_visible")==stof(argv(2)) && cvar("ftebroom_grid_snap")==stof(argv(3)),argv(4));
    } else if(mode=="preview") {
        int brush=fb_createbox('0 0 64','128 96 128');
        int captured=fb_transform_capture();
        fb_transform_delta='96 0 0';
        fb_transform_preview=captured;
        fb_grid_expect(brush && captured && fb_preview_replaces(1,brush),"preview_replaces_source_brush");
    }
    return TRUE;
}
