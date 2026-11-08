var float 	autocvar_ca_brush_view = 0;		//0=normal, 1=x, 2=y, 3=z
var float 	autocvar_ca_brush_viewsize = 1024;	//for different views.
var string	autocvar_ca_newbrushtexture = "metal4_2";
var float	autocvar_ca_newbrushheight = 64;
var float	autocvar_ca_grid = 16;
var float	autocvar_ca_explicitpatch = 0;
var float	autocvar_ca_cpwidth = 2;
var float	autocvar_ca_cpheight = 2;


#define EPSILON (1.0 / 32)	//inprecision sucks.

#define MAX_FACEPOINTS 64
vector facepoints[MAX_FACEPOINTS];	//for temp usage

#define MAX_BRUSHFACES 64
struct
{
	//brush state
	brushface_t faces[MAX_BRUSHFACES];
	int numfaces;
	int contents;

	//patch state
	patchvert_t *cp;
	int numcp, maxcp;
	patchinfo_t patchinfo;
} tmp;



//int selected;
int brushlist[2048];

enum : int
{
	BT_NONE,	//selection
	BT_MOVE = BT_NONE,
	BT_ROTATE,
	BT_MERGE,
	BT_PUSHFACE,
	BT_CREATEBRUSH,
	BT_CREATEPATCH,
	BT_CREATEDRAG,
	BT_CLONEDISPLACE,
	BT_SLICE,
	BT_MOVETEXTURE,
	BT_VERTEXEDIT
};
int mousetool;
int brushtool;
int bt_points;
vector bt_displace;
vector bt_point[64];
int nogrid;	//+nogrid, temporarily disables grid locks





typedef struct
{
	int model;
	int id;
    int faceonly;
	__uint64 facemask;	//MAX_BRUSHFACES is 64... handy.
} selbrush_t;
// Compare the words explicitly: the legacy QC target miscompiles 64-bit comparisons.
int selbrush_whole(selbrush_t *item)
{
    if(item->faceonly) return FALSE;
    int *words=(int*)&item->facemask;
    if(words[0]!=-1) return FALSE;
    return words[1]==-1;
}
int selbrush_same_mask(selbrush_t *a,selbrush_t *b)
{
    if(a->faceonly!=b->faceonly) return FALSE;
    int *left=(int*)&a->facemask;
    int *right=(int*)&b->facemask;
    if(left[0]!=right[0]) return FALSE;
    return left[1]==right[1];
}
void selbrush_reset_mask(selbrush_t *item,int whole)
{
    int *words=(int*)&item->facemask;
    words[0]=words[1]=0;
    if(whole) {words[0]=-1;words[1]=-1;}
    item->faceonly=!whole;
}
// Use two 32-bit words: shifts and comparisons of uint64 are unreliable on FTE_5768.
int selbrush_face(selbrush_t *item,int face)
{
    if(selbrush_whole(item)) return TRUE;
    if(face<0 || face>=64) return FALSE;
    int *words=(int*)&item->facemask;
    int word=0;if(face>=32) {word=1;face-=32;}
    int bit=1i<<face;return (words[word] & bit)!=0;
}
void selbrush_set_face(selbrush_t *item,int face,int selected)
{
    if(face<0 || face>=64) return;
    int *words=(int*)&item->facemask;
    int word=0;if(face>=32) {word=1;face-=32;}
    int bit=1i<<face;
    if(selected) words[word]|=bit;else words[word]&=~bit;
    item->faceonly=TRUE;
}
void fb_face_limit()
{
    cvar_set("ftebroom_selection_status","Face selection supports indices 0-63 only.");
    print("ReyondBroom: face selection supports indices 0-63 only.\n");
}
selbrush_t *selectedbrushes;
int selectedbrushcount;

var int selectedbrushmodel = 1;	//by default, the worldmodel. this is tracked to know which submodel to insert new brushes into.

hashtable brush_selection_index;
int brush_selection_index_valid,fb_selection_epoch;
void brush_selection_invalidate() {brush_selection_index_valid=FALSE;fb_selection_epoch++;}
void brush_selection_index_prepare()
{
    if(brush_selection_index_valid)return;
    if(brush_selection_index)hash_destroytab(brush_selection_index);
    brush_selection_index=hash_createtab(max(64,selectedbrushcount*2),EV_STRING);
    for(int i=0;i<selectedbrushcount;i++)
        brush_selection_index[sprintf("%i:%i",selectedbrushes[i].model,selectedbrushes[i].id)]=itos(i+1);
    brush_selection_index_valid=TRUE;
}
int(int modelidx, int brushid) brush_isselected =
{
    if(!selectedbrushcount)return FALSE;
    brush_selection_index_prepare();
    return stoi(brush_selection_index[sprintf("%i:%i",modelidx,brushid)]);
};
int(int modelidx, int brushid, int faceid) brushface_isselected =
{
    int i=brush_isselected(modelidx,brushid);
    if(!i)return FALSE;
    return selbrush_face(&selectedbrushes[i-1],faceid-1);
};
int(int modelidx, int brushid) brush_deselect =
{
	int i = brush_isselected(modelidx, brushid);
	if (!i)
		return FALSE;
	brush_selected(modelidx, brushid, -1, FALSE);
	memcpy(&selectedbrushes[i-1], &selectedbrushes[i], sizeof(selbrush_t)*(selectedbrushcount-i));
	selectedbrushcount--;
    brush_selection_invalidate();
	return TRUE;
};
int(int modelidx, int brushid, int face) brushface_deselect =
{
    if(face<1 || face>64) {fb_face_limit();return FALSE;}
    int selected=brush_isselected(modelidx,brushid);
    if(!selected) return FALSE;
    if(!selbrush_face(&selectedbrushes[selected-1],face-1))return TRUE;
    selbrush_set_face(&selectedbrushes[selected-1],face-1,FALSE);
    int *words=(int*)&selectedbrushes[selected-1].facemask;
    if(words[0]==0 && words[1]==0) brush_deselect(modelidx,brushid);
    else fb_selection_epoch++;
    return TRUE;
};
void() brush_deselectall =
{
	for (int i = 0; i < selectedbrushcount; i++)
		brush_selected(selectedbrushes[i].model, selectedbrushes[i].id, -1, FALSE);

	selectedbrushcount = 0;
    brush_selection_invalidate();
};
void(int modelidx, int brushid) brush_select =
{
	if (!brush_isselected(modelidx, brushid))
	{
		selbrush_t *n = memalloc(sizeof(selbrush_t) * (selectedbrushcount+1));
		memcpy(n, selectedbrushes, sizeof(selbrush_t)*selectedbrushcount);
		memfree(selectedbrushes);
		selectedbrushes = n;
		n[selectedbrushcount].model = modelidx;
		n[selectedbrushcount].id = brushid;
		selbrush_reset_mask(&n[selectedbrushcount],TRUE);
        n[selectedbrushcount].faceonly=FALSE;
		selectedbrushcount++;
        brush_selection_index[sprintf("%i:%i",modelidx,brushid)]=itos(selectedbrushcount);
		brush_selected(modelidx, brushid, -1, TRUE);
		// The ID index was updated in place; only selection-dependent caches
		// need invalidation. Rebuilding the index per added brush is quadratic.
		fb_selection_epoch++;
	}
	selectedbrushmodel = modelidx;
};
void(int modelidx, int brushid, int faceid) brushface_select =
{
	if(faceid<1 || faceid>64) {fb_face_limit();return;}
    faceid--;
	int i = brush_isselected(modelidx, brushid);	//get the brush selection index
	if (!i)
	{	//brush not selected yet
		selbrush_t *n = memalloc(sizeof(selbrush_t) * (selectedbrushcount+1));
		memcpy(n, selectedbrushes, sizeof(selbrush_t)*selectedbrushcount);
		memfree(selectedbrushes);
		selectedbrushes = n;
		n[selectedbrushcount].model = modelidx;
		n[selectedbrushcount].id = brushid;
		selbrush_reset_mask(&n[selectedbrushcount],FALSE);
        n[selectedbrushcount].faceonly=TRUE;
		selectedbrushcount++;
        brush_selection_index[sprintf("%i:%i",modelidx,brushid)]=itos(selectedbrushcount);
		brush_selected(modelidx, brushid, -1, TRUE);
		i = selectedbrushcount;
	}
	int changed=!selbrush_face(&selectedbrushes[i-1],faceid) || selbrush_whole(&selectedbrushes[i-1]);
	selbrush_set_face(&selectedbrushes[i-1],faceid,TRUE);
	if(changed)fb_selection_epoch++;
	selectedbrushmodel = modelidx;
};
