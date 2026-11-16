# SPDX-License-Identifier: GPL-3.0-or-later
"""Author the Field Technician's work trousers, shirt and belt."""
from collections import Counter, defaultdict
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "runtime/characters"
SOURCE = ROOT / "assets/characters/charcoal-workwear-source.png"
FOLDS = ROOT / "assets/characters/workwear-folds-source.png"
SIZE = 4096
RUNTIME_SIZE = 2048
POCKET_CHART = (.013,.858,.062,.972)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def smooth(low, high, x):
    t = np.clip((x-low)/(high-low), 0., 1.)
    return t*t*(3-2*t)


def sleeve_cuff(x,y,z):
    across = .64*(np.abs(x)-4.32)-.70*(y-10.70)+.32*(z-1.52)
    sleeve = smooth(3.90,4.19,np.abs(x))
    return across,sleeve


def polar_waist(vertices, rim, x, z):
    center = .12
    ring = vertices[rim,:3]
    theta = np.arctan2(ring[:,2]-center,ring[:,0])
    radius = np.hypot(ring[:,0],ring[:,2]-center)
    angle = np.arctan2(z-center,x)
    height = np.interp(angle,theta,ring[:,1],period=2*np.pi)
    return height,np.interp(angle,theta,radius,period=2*np.pi)


def waist_openings(vertices, pants, faces, source_ids):
    edges = Counter(tuple(sorted((source_ids[a],source_ids[b])))
                    for face in faces for a,b in zip(face,face[1:]+face[:1]))
    boundary = {vertex for edge,count in edges.items() if count==1 for vertex in edge}
    indices = {source:i for i,source in enumerate(source_ids) if source in boundary}
    rim = [i for i in indices.values() if pants[i] and vertices[i,1]>9.0]
    hem = [i for i in indices.values() if not pants[i] and vertices[i,1]<9.2]
    if len(rim)<20 or len(hem)<20:
        raise ValueError("Shirt hem or waistband is missing")
    return rim,hem


def neckline_edges(faces, source_ids, heights):
    """Find the open run from one side of the placket to the other."""
    src = lambda edge: tuple(sorted(source_ids[i] for i in edge))
    edges = [(a,b) for face in faces for a,b in zip(face,face[1:]+face[:1])]
    counts = Counter(map(src,edges))
    edge_indices = {}
    neighbors = defaultdict(list)
    for a,b in edges:
        if counts[src((a,b))]!=1 or min(heights[a],heights[b])<13.52:
            continue
        first,second = source_ids[a],source_ids[b]
        neighbors[first].append(second)
        neighbors[second].append(first)
        edge_indices[first,second] = (a,b,True)
        edge_indices[second,first] = (b,a,False)
    ends = sorted(index for index,adjacent in neighbors.items() if len(adjacent)==1)
    if len(ends)!=2 or any(len(adjacent)>2 for adjacent in neighbors.values()):
        raise ValueError("Shirt neckline does not have a continuous open seam")
    previous,current,ordered = None,ends[0],[]
    while current!=ends[1]:
        following, = (index for index in neighbors[current] if index!=previous)
        ordered.append(edge_indices[current,following])
        previous,current = current,following
    if len(ordered)!=len(neighbors)-1 or len(ordered)<20:
        raise ValueError("Shirt neckline seam is incomplete")
    return ordered


def tuck_hem(vertices, pants, rim, hem):
    x,y,z = vertices[:,:3].T.copy()
    top,radius = polar_waist(vertices,rim,x,z)
    old_hem,_ = polar_waist(vertices,hem,x,z)
    trunk = (~pants)*(1-smooth(1.90,2.38,np.abs(x)))
    # Keep a narrow strip under the rim all the way around. Compress the
    # original low hem smoothly instead of flattening its faces to one ring.
    lower = top-.23+np.clip(y-old_hem,0.,1.1)*.23
    height = y+np.maximum(0.,lower-y)*trunk
    near_waist = trunk*(1-smooth(top-.035,top+.44,height))
    old_radius = np.hypot(x,z-.12)
    ease = .038*trunk*smooth(top+.14,top+.43,height)*(1-smooth(top+.62,top+1.21,height))
    factor = 1.+(ease-near_waist*np.maximum(0.,old_radius-(radius-.196)))/np.maximum(old_radius,.01)
    vertices[:,0] = x*factor
    vertices[:,1] = height
    vertices[:,2] = .12+(z-.12)*factor
    hem_points = vertices[hem]
    hem_height,hem_radius = polar_waist(vertices,rim,hem_points[:,0],hem_points[:,2])
    if (np.max(hem_points[:,1]-hem_height)>-.10 or
            np.max(np.hypot(hem_points[:,0],hem_points[:,2]-.12)-hem_radius)>-.153):
        raise ValueError("Shirt hem must stay beneath and inside the waistband")


def cubic(points,t):
    index = min(len(points)-2,int(t*(len(points)-1)))
    u = t*(len(points)-1)-index
    a,b,c,d = (points[max(0,index-1)],points[index],points[index+1],
                points[min(len(points)-1,index+2)])
    return .5*((2*b)+(-a+c)*u+(2*a-5*b+4*c-d)*u*u+(-a+3*b-3*c+d)*u*u*u)


def collar_contact(point, triangles, direction):
    # Intersect the shirt near the authored collar edge, along the local
    # outward direction. This keeps the tips against the front of the shirt
    # and the short back of the collar against the shoulder yoke.
    a = triangles[:,0]
    first,second = triangles[:,1]-a,triangles[:,2]-a
    across = np.cross(direction,second)
    denominator = np.sum(first*across,axis=1)
    usable = np.abs(denominator)>1e-8
    inverse = np.divide(1.,denominator,out=np.zeros_like(denominator),where=usable)
    origin = point-a
    u = np.sum(origin*across,axis=1)*inverse
    perpendicular = np.cross(origin,first)
    v = (perpendicular@direction)*inverse
    depth = np.sum(second*perpendicular,axis=1)*inverse
    hits = depth[usable & (u>=-1e-6) & (v>=-1e-6) & (u+v<=1.+1e-6) & (np.abs(depth)<.42)]
    if not hits.size:
        return point
    contact = hits[np.argmin(np.abs(hits))]
    return point+direction*(contact+.031)


def relax_shirt_panels(vertices, faces, source_ids, weight):
    # Weld the working positions by source ID so the UV seams follow the
    # same curve. Relax depth along the original mesh edges while keeping
    # the side seam, armholes and neckline anchored.
    _,first,weld = np.unique(source_ids,return_index=True,return_inverse=True)
    mesh = weld[np.asarray(faces)]
    edges = np.concatenate((mesh[:,[0,1]],mesh[:,[1,2]],mesh[:,[2,0]]))
    edges = np.unique(np.concatenate((edges,edges[:,::-1])),axis=0)
    left,right = edges.T
    degree = np.maximum(1.,np.bincount(left,minlength=len(first)))
    depth = vertices[first,2].copy()
    blend = weight[first]
    for strength in (.48,-.23,.48,-.23,.48):
        mean = np.bincount(left,weights=depth[right],minlength=len(first))/degree
        depth += strength*blend*(mean-depth)
    vertices[:,2] = depth[weld]


def make_garment():
    wardrobe = RUNTIME / "wardrobe"
    base = wardrobe / "male_casualsuit03.rbw"
    data = base.read_bytes()
    magic, version, count, face_count, hidden_count = struct.unpack_from("<4s4I", data)
    if (magic, version) != (b"RBW1", 1):
        raise ValueError("Unexpected workwear mesh")
    records = [bytearray(data[52+i*92:52+(i+1)*92]) for i in range(count)]
    vertices = np.array([struct.unpack_from("<5f", r, 4) for r in records])
    source_ids = [struct.unpack_from("<I", r)[0] for r in records]
    start = 52+count*92
    faces = list(struct.iter_unpack("<3I", data[start:start+face_count*12]))
    parent = {i:i for i in source_ids}

    def root(i):
        while parent[i]!=i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for a,b,c in faces:
        first = root(source_ids[a])
        parent[root(source_ids[b])] = first
        parent[root(source_ids[c])] = first
    pants_group = root(source_ids[int(np.argmin(vertices[:,1]))])
    pants = np.array([root(i)==pants_group for i in source_ids])
    x,y,z = vertices[:,:3].T.copy()
    width = np.abs(x)
    waist = smooth(6.9,9.44,y)*pants
    pelvis = smooth(7.03,8.13,y)*(1-smooth(9.10,9.58,y))*pants
    # Continue the waistband into the side seam so that the empty space just
    # below the belt does not turn into a sharp flare at the upper thigh.
    upper_hip = smooth(7.59,8.35,y)*(1-smooth(8.73,9.32,y))
    upper_thigh = smooth(5.71,6.85,y)*(1-smooth(7.22,8.15,y))
    outer = smooth(.92,1.58,width)*pants
    vertices[:,0] = (x*(1.-.088*pelvis)
                      + np.sign(x)*(outer*(.211*upper_hip-.162*upper_thigh)
                                    -.053*smooth(.30,1.45,width)*waist))

    # Lower the rise on both panels. Spread the deformation into the inside
    # leg and keep the other side of each leg clear during the stride.
    center = (1-smooth(.25,.92,width))*pants
    saddle = smooth(5.65,7.53,y)*(1-smooth(7.84,9.23,y))
    vertices[:,1] = y+.22*waist-.58*center*saddle
    inner_leg = (1-smooth(.78,1.42,width))*smooth(4.42,5.71,y)*(1-smooth(6.61,7.68,y))
    vertices[:,0] -= np.sign(x)*np.minimum(.247,width*.49)*inner_leg*pants

    front = smooth(.32,1.10,z)
    back = 1-smooth(-.66,-.12,z)
    seat = smooth(5.69,7.40,y)*(1-smooth(8.22,9.24,y))*pants
    hip_depth = .058*front-.068*back
    front_rise = front*center*saddle
    vertices[:,2] = z-.036*np.clip(z-.12,-.9,1.)*waist+hip_depth*seat-.078*front_rise

    # Follow the seat into the back of the thigh with a higher crown and a
    # soft transition into the centre seam. Keep the waistband fixed.
    rear_panel = pants*(1-smooth(-.46,-.16,z))
    seat_crown = np.exp(-((width-.75)/.62)**2-((y-7.93)/.76)**2)
    under_seat = np.exp(-((width-.68)/.74)**2-((y-7.00-.16*width)/.48)**2)
    vertices[:,2] += rear_panel*(-.122*seat_crown+.044*under_seat)

    # Give the cloth clearance over the shin and boot shaft. The folds sit at
    # different heights around each ankle and disappear above the knee.
    shin = pants*(1-smooth(3.96,5.78,y))
    leg_center = np.interp(y,(.8,2.,4.,5.75),(2.18,2.09,1.70,1.42))
    shin_x = x-np.sign(x)*leg_center
    shin_z = z-np.interp(y,(.8,3.2,5.75),(.03,.03,.31))
    shin_radius = np.maximum(np.hypot(shin_x,shin_z),.22)
    fold_angle = np.arctan2(shin_z,np.sign(x)*shin_x)
    drop = .19*np.sin(fold_angle+.86*np.sign(x))+.105*np.cos(2*fold_angle-1.7*x)
    fullness = .73+.27*np.sin(fold_angle+.53*np.sign(x))
    stack = (.215*fullness*np.exp(-((y-1.28-drop)/.24)**2)
             -.082*np.exp(-((y-1.64-.73*drop)/.17)**2)
             +.129*np.exp(-((y-2.02+drop)/.23)**2))
    spread = shin*(.049+.061*smooth(2.6,4.12,y)+stack)/shin_radius
    vertices[:,0] += shin_x*spread-.072*shin*np.sign(x)*smooth(.68,1.87,y)
    vertices[:,1] += .033*shin*(1-smooth(1.04,1.54,y))
    vertices[:,2] += shin_z*spread

    back = (~pants)*(1-smooth(-.47,-.09,z))*(1-smooth(1.31,2.31,width))
    panel = smooth(9.80,10.60,y)*(1-smooth(11.85,12.63,y))*(~pants)
    sides = smooth(.64,1.46,np.abs(x))*(1-smooth(1.79,2.05,np.abs(x)))
    lower_side = smooth(9.52,9.95,y)*(1-smooth(10.24,10.92,y))
    vertices[:,0] += (.147*panel-.054*lower_side*(~pants))*sides*np.sign(x)
    back_profile = smooth(9.37,10.17,y)*(1-smooth(12.02,13.71,y))
    vertices[:,2] -= .475*back*back_profile
    abdomen = smooth(9.26,9.98,y)*(1-smooth(10.92,12.57,y))
    ribs = smooth(10.78,11.70,y)*(1-smooth(12.56,13.69,y))
    front = (~pants)*smooth(.50,1.31,z)*(1-smooth(.97,1.90,width))
    vertices[:,2] -= front*(.51*abdomen+.158*ribs)
    panels = (~pants)*(1-smooth(1.25,2.05,width))*smooth(9.52,10.24,y)*(1-smooth(13.25,13.81,y))
    relax_shirt_panels(vertices,faces,source_ids,panels)
    chest_face = (~pants)*smooth(.96,1.30,z)
    pocket = smooth(.30,.45,width)*(1-smooth(1.03,1.20,width))
    pocket *= smooth(11.44,11.66,y)*(1-smooth(12.45,12.67,y))
    flap = np.exp(-((y-12.38)/.13)**2)*pocket
    placket = (1-smooth(.094,.148,width))*smooth(9.86,10.08,y)*(1-smooth(13.19,13.36,y))
    vertices[:,2] += chest_face*(.029*pocket+.027*flap+.018*placket)
    cuff_pos,cuff_side = sleeve_cuff(x,y,z)
    cuff_edge = (~pants)*cuff_side*np.exp(-((cuff_pos-.015)/.074)**2)
    vertices[:,2] += .036*cuff_edge*np.clip((z-1.53)/.22,-1.,1.)
    rim,hem = waist_openings(vertices,pants,faces,source_ids)
    top,_ = polar_waist(vertices,rim,vertices[:,0],vertices[:,2])
    radius = np.hypot(vertices[:,0],vertices[:,2]-.12)
    forward = (vertices[:,2]-.12)/np.maximum(radius,.01)
    target = 9.39-.046*forward
    # Straighten the waistband, leaving its small natural front dip.
    vertices[:,1] += (target-top)*smooth(top-.68,top-.07,vertices[:,1])*pants
    tuck_hem(vertices,pants,rim,hem)

    # Relax the raised points of the source neckline into the shoulder slope.
    neckline = (~pants)*smooth(13.47,13.94,y)*(1-smooth(.90,1.59,width))
    neckline_height = 13.91-.36*smooth(.60,1.12,z)
    vertices[:,1] -= np.maximum(y-neckline_height,0.)*neckline

    for record, point in zip(records, vertices):
        struct.pack_into("<3f", record, 4, *point[:3])

    first_free = max(source_ids)+1

    def garment_vertex(point,u,v,nearest=None, source=None):
        if nearest is None:
            distances = np.sum((vertices[:count,:3]-point)**2,axis=1)
            nearest = int(np.argmin(np.where(pants,np.inf,distances)))
        record = bytearray(records[nearest])
        source = first_free+len(records)-count if source is None else source
        struct.pack_into("<I5f",record,0,source,*point,u,v)
        records.append(record)

    def add_grid(rows, columns, point_for, reverse=False, attachments=None):
        offset = len(records)
        for row in range(rows):
            for col in range(columns):
                point,u,v = point_for(row/(rows-1),col/(columns-1))
                if callable(attachments):
                    anchor = attachments(point)
                else:
                    anchor = None if attachments is None else attachments[col]
                garment_vertex(point,u,v,anchor)
        for row in range(rows-1):
            for col in range(columns-1):
                a = offset+columns*row+col
                pair = [(a,a+columns,a+1),(a+1,a+columns,a+columns+1)]
                faces.extend([(i,k,j) for i,j,k in pair] if reverse else pair)
        return offset

    # The collar starts at the shirt's actual open edge. Each UV-split seam
    # vertex keeps the same source index, position and deformation anchors.
    collar_edge = np.array([
        (.55,13.23,1.29),(.74,13.48,1.15),(.96,13.67,.80),(1.09,13.81,.34),
        (.97,13.79,-.23),(.62,13.75,-.54),(.0,13.72,-.69),
    ])

    seam_edges = neckline_edges(faces,source_ids,y)
    seam_indices = [seam_edges[0][0]]+[b for _,b,_ in seam_edges]
    seam_points = vertices[seam_indices,:3]
    arc = np.r_[0.,np.cumsum(np.linalg.norm(np.diff(seam_points,axis=0),axis=1))]
    middle = int(np.argmin(seam_points[:,2]))
    around = np.interp(arc,(0.,arc[middle],arc[-1]),(0.,.5,1.))
    first_side = float(np.sign(seam_points[0,0]))

    def symmetric_curve(points,s):
        value = cubic(points,2*min(s,1-s))
        value[0] *= first_side if s<.5 else -first_side
        return value

    shirt_faces = np.asarray(faces)
    shirt_faces = shirt_faces[(~pants[shirt_faces]).all(axis=1) &
                              (y[shirt_faces].max(axis=1)>13.) &
                              (width[shirt_faces].min(axis=1)<1.34)]
    triangles = vertices[shirt_faces,:3]
    outer_edge = []
    for s in around:
        edge = symmetric_curve(collar_edge,s)
        pitch = smooth(.52,1.18,edge[2])*np.pi*.5
        direction = np.array((0.,np.cos(pitch),np.sin(pitch)))
        outer_edge.append(collar_contact(edge,triangles,direction))

    collar_start = len(records)
    columns = len(seam_indices)
    rings = (0.,.26,.63,1.)
    for row,drop in enumerate(rings):
        for col,(s,anchor) in enumerate(zip(around,seam_indices)):
            point = seam_points[col]*(1.-drop)+outer_edge[col]*drop
            if row in (1,2):
                pitch = smooth(.53,1.18,point[2])*np.pi*.5
                outward = np.array((0.,np.cos(pitch),np.sin(pitch)))
                if row==2:
                    point = collar_contact(point,triangles,outward)
                crown = .040+.046*smooth(.29,.47,min(s,1.-s))
                point += outward*(np.sin(np.pi*drop)*crown)
            source = source_ids[anchor] if row==0 else None
            garment_vertex(point,.846+.132*s,.958-.110*drop,anchor,source)

    for row in range(len(rings)-1):
        for col,(_,_,forward_edge) in enumerate(seam_edges):
            a = collar_start+columns*row+col
            quad = ((a,a+columns,a+1),(a+1,a+columns,a+columns+1))
            faces.extend(quad if forward_edge else ((i,k,j) for i,j,k in quad))

    # A separate crew neckline follows the base of the neck under the shirt.
    def tee(t,s):
        edge = abs(2*s-1)
        x = (s-.5)*(1.105-.51*t)
        y = 13.68+.22*edge**1.6-.67*t
        z = 1.035+.341*t-.305*edge**1.7*(1-t)
        z += .048*smooth(.32,.74,edge)*(1-smooth(.30,.60,t))
        return np.array((x,y,z)),.848+.121*s,.826-.023*t
    add_grid(10,23,tee)

    # The belt is an external continuous strip attached to the trouser
    # opening. The back and flanks carry the same hardware as the front.
    ring = np.asarray(rim)
    angles = np.arctan2(vertices[ring,2]-.12,vertices[ring,0])
    order = np.argsort(angles)
    ring = ring[order]
    angles = angles[order]
    belt_count = 65
    theta = np.linspace(-np.pi,np.pi,belt_count)
    def angle_attachment(value):
        delta = np.abs(np.arctan2(np.sin(angles-value),np.cos(angles-value)))
        return int(ring[np.argmin(delta)])
    attachments = [angle_attachment(value) for value in theta]

    waist_faces = np.array([face for face in faces[:face_count]
                            if pants[face[0]] and np.max(vertices[list(face),1])>9.1])
    waist_mesh = vertices[waist_faces,:3]
    first = waist_mesh[:,0]
    e1,e2 = waist_mesh[:,1]-first,waist_mesh[:,2]-first

    def belt_point(angle, below, lift):
        c,d = np.cos(angle),np.sin(angle)
        top,radius = polar_waist(vertices,rim,c,.12+d)
        y = float(top)-below
        # Roll the webbing around the pants at this precise height. The
        # waistband grows below its upper edge, especially over the seat.
        direction = np.array((c,0.,d))
        origin = np.array((0.,y,.12))
        h = np.cross(direction,e2)
        determinant = np.einsum("ij,ij->i",e1,h)
        valid = np.abs(determinant)>1e-7
        inverse = np.divide(1.,determinant,out=np.zeros_like(determinant),where=valid)
        relative = origin-first
        u = inverse*np.einsum("ij,ij->i",relative,h)
        q = np.cross(relative,e1)
        v = inverse*(q @ direction)
        depth = inverse*np.einsum("ij,ij->i",e2,q)
        hits = valid&(u>=-1e-6)&(v>=-1e-6)&(u+v<=1.000001)&(depth>0.)
        if np.any(hits):
            radius = float(depth[hits].max())
        radius += lift
        return np.array((radius*c,y,.12+radius*d))

    strip = [(.056,.021),(.076,.040),(.247,.040),(.265,.021)]
    def strap(t,s):
        below,lift = strip[round(t*(len(strip)-1))]
        return belt_point(-np.pi+2*np.pi*s,below,lift),.846+.131*s,.987-.013*t
    add_grid(len(strip),belt_count,strap,True,attachments)

    # Seven individually placed loops run over the strap.
    for angle in (-2.88,-2.04,-1.13,-.23,.76,2.28,2.98):
        layer = [(.025,.022),(.078,.078),(.247,.078),(.292,.026)]
        def belt_loop(t,s):
            below,lift = layer[round(t*(len(layer)-1))]
            return belt_point(angle+.073*(s-.5),below,lift),.941+.03*s,.961-.013*t
        loop_attachments = [angle_attachment(angle+.073*(s-.5)) for s in (0,1)]
        add_grid(len(layer),2,belt_loop,True,loop_attachments)

    # A low-profile buckle frames a clear opening for the webbing.
    for dx,dy,width,height in ((0,-.077,.37,.044),(0,.077,.37,.044),
                                (-.161,0,.047,.155),(.161,0,.047,.155),(.019,0,.034,.139)):
        def metal(t,s):
            x = dx+width*(s-.5)
            below = .160-dy+height*(t-.5)
            angle = np.pi/2-x/.98
            raise_edge = .026*np.sin(np.pi*t)*np.sin(np.pi*s)
            return belt_point(angle,below,.080+raise_edge),.849+.064*s,.964-.015*t
        add_grid(4,5,metal,False,[angle_attachment(np.pi/2)]*5)

    # Cargo pockets follow the outside panel of each leg. Project the padded
    # pocket and folded flap onto that surface, with the edges sewn flat.
    thigh_faces = np.array([face for face in faces[:face_count] if pants[face[0]] and
                            np.min(vertices[list(face),1])<7.51 and np.max(vertices[list(face),1])>5.95])
    thigh_mesh = vertices[thigh_faces,:3]
    side0 = thigh_mesh[:,0]
    side1,side2 = thigh_mesh[:,1]-side0,thigh_mesh[:,2]-side0
    side_det = side1[:,1]*side2[:,2]-side2[:,1]*side1[:,2]
    side_valid = np.abs(side_det)>1e-8
    side_inverse = np.divide(1.,side_det,out=np.zeros_like(side_det),where=side_valid)
    u0,v0,u1,v1 = POCKET_CHART

    def pants_attachment(point):
        error = np.sum((vertices[:count,:3]-point)**2,axis=1)
        return int(np.argmin(np.where(pants,error,np.inf)))

    for sign in (-1,1):
        def pocket(t,s):
            inset = .126*(1-smooth(0,.19,1-t))
            y = 7.22-1.13*t+sign*.023*(1-t)
            z = .16+(.91-inset)*(s-.5)+.048*(t-.5)
            yy,zz = y-side0[:,1],z-side0[:,2]
            u = (yy*side2[:,2]-zz*side2[:,1])*side_inverse
            v = (zz*side1[:,1]-yy*side1[:,2])*side_inverse
            x = sign*(side0[:,0]+u*side1[:,0]+v*side2[:,0])
            inside = side_valid&(u>=-1e-7)&(v>=-1e-7)&(u+v<=1.0000001)
            if not np.any(inside&(x>0)):
                raise ValueError("Cargo pocket extends beyond the leg panel")
            x = np.where(inside,x,-np.inf).max()
            edge = smooth(0,.17,min(s,1-s))*smooth(0,.15,min(t,1-t))
            flap = np.exp(-((t-.31)/.085)**2)
            thickness = .008+edge*(.035+.031*flap)
            return np.array((sign*(x+thickness),y,z)),u0+(u1-u0)*s,v1-(v1-v0)*t

        add_grid(12,9,pocket,sign>0,pants_attachment)

    name = "male_utility_field"
    asset = wardrobe / (name+".rbw")
    header = struct.pack("<4s4I",magic,version,len(records),len(faces),hidden_count)+data[20:52]
    asset.write_bytes(header+b"".join(records)+b"".join(struct.pack("<3I",*face) for face in faces)
                      +data[start+face_count*12:])
    registry = wardrobe / "provenance.json"
    manifest = json.loads(registry.read_bytes())
    original = next(item for item in manifest["assets"] if item["id"]==base.stem)
    entry = {"id":name,"file":asset.name,"sha256":digest(asset),"sources":original["sources"],
             "license":original["license"],"material":"charcoal-utility","derivedFrom":base.name}
    manifest["assets"] = [item for item in manifest["assets"] if item["id"]!=name]+[entry]
    registry.write_text(json.dumps(manifest,indent=2)+"\n",encoding="utf-8")
    return base,vertices,faces[:face_count],pants


def swatch(sheet, bounds, period):
    pic = np.asarray(sheet.crop(bounds).resize((period,period),Image.Resampling.LANCZOS))
    return np.concatenate((np.concatenate((pic,pic[:,::-1]),axis=1),
                           np.concatenate((pic[::-1],pic[::-1,::-1]),axis=1)))


def build_material(sheet, geometry, asset):
    base,vertices,faces,pants = geometry
    sheet = sheet.resize((1536,1024),Image.Resampling.LANCZOS)
    image = np.asarray(sheet)
    fold_sheet = Image.open(FOLDS).convert("RGB").resize((1536,1024),Image.Resampling.LANCZOS)
    crease = np.asarray(fold_sheet,dtype=np.float32).mean(axis=2)
    cotton = swatch(sheet,(31,21,479,476),448)
    # Keep fine broken-twill threads and remove the old ripstop grid.
    trouser_crop = fold_sheet.crop((286,796,506,1005)).convert("L")
    cloth = np.asarray(trouser_crop,dtype=np.float32)
    broad = np.asarray(trouser_crop.filter(ImageFilter.GaussianBlur(8)),dtype=np.float32)
    neutral = np.clip(47+1.64*(cloth-broad),31,66).astype(np.uint8)
    twill = swatch(Image.fromarray(neutral).convert("RGB"),(0,0,*trouser_crop.size),354)
    blank = cotton[np.arange(SIZE)[:,None]%cotton.shape[0],np.arange(SIZE)%cotton.shape[1]].copy()
    uv = vertices[:,3:].copy()
    uv[:,1] = 1-uv[:,1]
    uv *= SIZE-1
    def stitch_line(across, along, spacing=.058):
        holes = np.clip((.29-np.abs(np.mod(along/spacing+.5,1)-.5))/.16,0,1)
        groove = np.exp(-(across/.016)**2)
        threads = np.exp(-((np.abs(across)-.031)/.011)**2)
        return -5.5*groove+(2.5+6.5*holes)*threads

    for indices in faces:
        ids = list(indices)
        is_pants = pants[ids[0]]
        points = vertices[ids,:3]
        chart = uv[ids]
        u0,v0 = chart[0]
        a,b = chart[1]-chart[0],chart[2]-chart[0]
        det = a[0]*b[1]-a[1]*b[0]
        if abs(det)<1e-9:
            continue
        low = np.maximum(0,np.floor(chart.min(axis=0)).astype(int)-1)
        high = np.minimum(SIZE-1,np.ceil(chart.max(axis=0)).astype(int)+1)
        if np.any(high<low):
            continue
        yy,xx = np.ogrid[low[1]:high[1]+1,low[0]:high[0]+1]
        u = ((xx-u0)*b[1]-(yy-v0)*b[0])/det
        v = (a[0]*(yy-v0)-a[1]*(xx-u0))/det
        keep = (u>=-.012)&(v>=-.012)&(u+v<=1.012)
        if not np.any(keep):
            continue
        x,y,z = [points[0,i]+(points[1,i]-points[0,i])*u+(points[2,i]-points[0,i])*v
                 for i in range(3)]
        ax = np.abs(x)
        normal = np.cross(points[1]-points[0],points[2]-points[0])
        length = np.linalg.norm(normal)
        front = length>1e-8 and normal[2]>.30*length
        cloth = twill if is_pants else cotton
        rgb = cloth[yy%cloth.shape[0],xx%cloth.shape[1]].astype(np.float32)

        def source_pixels(horizontal,vertical,x0,x1,y0,y1,crop,feather):
            u = (horizontal-x0)/(x1-x0)
            v = (y1-vertical)/(y1-y0)
            opacity = smooth(0.,feather,u)*smooth(0.,feather,1-u)*smooth(0.,feather,v)*smooth(0.,feather,1-v)
            x0c,y0c,x1c,y1c = crop
            cx = np.rint(x0c+np.clip(u,0.,1.)*(x1c-x0c)).astype(int)
            cy = np.rint(y0c+np.clip(v,0.,1.)*(y1c-y0c)).astype(int)
            return cx,cy,opacity

        def detail(horizontal,vertical,x0,x1,y0,y1,crop,strength=1.):
            cx,cy,opacity = source_pixels(horizontal,vertical,x0,x1,y0,y1,crop,.073)
            rgb[:] += (image[cy,cx]-rgb)*(opacity*strength)[...,None]

        def folds(horizontal,vertical,x0,x1,y0,y1,crop,strength):
            cx,cy,opacity = source_pixels(horizontal,vertical,x0,x1,y0,y1,crop,.22)
            a,b,c,d = crop
            fold = (crease[cy,cx]-float(crease[b:d,a:c].mean()))*opacity*strength
            rgb[:] += fold[...,None]

        if is_pants:
            front_fabric = smooth(.18,.51,z)
            rear_fabric = 1-smooth(-.37,-.17,z)
            # Bias the folds across each leg independently. Preserve the
            # flatter cloth beside the seams, pockets and lower shins.
            slope = y+.105*np.sign(x)*ax
            span = ax-(.052*(8.7-y))
            folds(span,slope,.31,2.05,6.75,9.16,(15,548,496,1000),.30*front_fabric)
            folds(span,slope,.50,2.23,3.15,5.75,(534,543,1000,997),.84*front_fabric)
            folds(span,y+.08*x,.44,2.14,3.12,5.76,(1048,556,1510,991),.92*rear_fabric)
            folds(ax,y+.071*x,.18,1.88,6.89,8.93,(63,547,486,1009),.52*rear_fabric)
            folds(ax,y+.16*x+.19*z,1.02,2.72,.83,2.72,(1050,622,1512,1012),.66)
            if front:
                detail(x,y,-.21,.26,8.67,9.45,(1280,737,1397,974),.41)
        else:
            back = 1-smooth(-.20,.065,z)
            front_fabric = smooth(.32,.74,z)
            back_drape = .61-.14*smooth(11.75,13.40,y)
            folds(x,y,-1.68,1.71,9.74,13.61,(26,22,490,500),back_drape*back)
            folds(ax,y,.16,1.96,9.12,11.60,(1039,21,1510,500),.27*front_fabric)
            sleeve = smooth(1.83,2.52,ax)
            side_shift = np.sign(x)
            distance = .63*(ax-3.13)-.69*(y-11.66)+.415*(z-.13)+.11*side_shift
            around = z-(.67*np.maximum(0,ax-3.13))-.12*side_shift
            folds(around,distance,-.58,.94,-.94,.76,(640,35,877,475),
                  (.51+.078*side_shift)*sleeve)
            folds(around,distance,-.56,.97,.93,2.33,(659,130,906,462),.30*sleeve)
            if front:
                detail(x,y,-.14,.14,9.89,13.30,(549,63,629,500),.94)
                detail(ax,y,.32,1.26,11.51,12.70,(651,118,1009,478),.94)
                if np.mean(x)>0:
                    detail(x,y,.40,1.19,12.75,13.07,(676,16,971,116),.98)
        if not is_pants:
            wrist,sleeve_tip = sleeve_cuff(x,y,z)
            cuff = smooth(-.23,-.15,wrist)*(1-smooth(.15,.22,wrist))*sleeve_tip
            if length>1e-8 and normal[2]>.21*length:
                detail(ax,wrist,3.98,4.58,-.23,.16,(1177,287,1455,435),.74)
            rgb *= (1-.11*cuff)[...,None]
            cuff_stitch = (stitch_line(wrist-.123,z)+stitch_line(wrist+.155,z))*sleeve_tip
            rgb += cuff_stitch[...,None]
        if is_pants:
            # Localised wear at the knees, inner thighs and cuffs. It is
            # evaluated on the garment so folds do not repeat across UV seams.
            knee = np.exp(-((y-3.80)/.64)**2)*smooth(.45,1.05,z)
            cuffs = np.exp(-((y-1.28)/.33)**2)
            fine_folds = 1.6*np.sin(24*y+4.6*z+2.1*x)*cuffs
            outer = smooth(1.39,1.90,ax)*(1-smooth(8.72,9.40,y))*smooth(1.25,1.85,y)
            outer_seam = outer*stitch_line(z-(.14-.018*y),y)
            seat = (1-smooth(-.42,-.19,z))*smooth(7.06,7.40,y)*(1-smooth(9.16,9.37,y))
            seat_seam = seat*stitch_line(x+.002,y,.052)
            darts = seat*smooth(8.42,8.72,y)*np.exp(-((ax-(.57-.12*(9.2-y)))/.024)**2)
            fly = smooth(7.86,8.24,y)*(1-smooth(9.16,9.32,y))*smooth(.61,1.04,z)
            fly_curve = .135*smooth(7.86,8.49,y)
            fly_seam = fly*(1.18*stitch_line(x-fly_curve,y)-4.9*np.exp(-((x+.024)/.023)**2))
            slash_gate = smooth(8.15,8.37,y)*(1-smooth(9.12,9.28,y))*smooth(.44,.78,z)
            slash_edge = ax-(1.14+.30*(y-8.58)+.09*(y-8.58)**2)
            slash_seam = slash_gate*(.92*stitch_line(slash_edge,y)-6.2*np.exp(-(slash_edge/.028)**2))
            rear_yoke = seat*(1-smooth(1.31,1.61,ax))*smooth(.10,.29,ax)
            rear_yoke *= stitch_line(y-(8.66+.115*ax+.11*ax**2),x)*.63
            rear_drape = rear_fabric*smooth(.15,.38,ax)*(1-smooth(1.04,1.57,ax))
            drape_line = y-(7.16+.33*ax+.06*np.sign(x))
            rear_fold = rear_drape*(2.3*np.exp(-((drape_line-.11)/.22)**2)
                                    -3.1*np.exp(-((drape_line+.05)/.18)**2))
            seat_light = 2.4*rear_fabric*np.exp(-((y-7.89)/.76)**2-((ax-.77)/.65)**2)
            gusset = (1-smooth(.13,.34,ax))*(1-smooth(.25,.74,np.abs(z-.10)))
            gusset *= smooth(5.9,6.47,y)*(1-smooth(7.16,7.72,y))
            tailoring = (2.8*knee+fine_folds+outer_seam+seat_seam+fly_seam+slash_seam+rear_yoke+rear_fold
                         +seat_light-3.8*darts-5.5*gusset)
            rgb = rgb*.93+np.array((4.4,5.3,6.9))+tailoring[...,None]
        else:
            yoke = (1-smooth(-.28,.02,z))*(1-smooth(1.06,1.63,ax))
            shoulder_seam = yoke*stitch_line(y-(12.91-.13*ax**1.8),x)
            side_seam = smooth(1.20,1.62,ax)*(1-smooth(1.70,2.03,ax))*smooth(9.41,9.76,y)
            rib_seam = side_seam*stitch_line(z-(.17+.033*(y-10.7)),y)
            sleeve_set = smooth(11.91,12.18,y)*(1-smooth(13.57,13.92,y))
            sleeve_set *= smooth(1.21,1.42,ax)*(1-smooth(2.05,2.30,ax))
            armhole = ax-(1.48+.262*(y-12.15)+.055*np.cos(4*z))
            sleeve_seam = sleeve_set*stitch_line(armhole,y-.11*z)
            rgb = rgb*1.04+9+(shoulder_seam+rib_seam+sleeve_seam)[...,None]
            rgb[:] = .63*rgb+.37*rgb.mean(axis=2,keepdims=True)
            if front:
                # Carry the exposed grey undershirt to the inner collar edge
                # between the overlapping placket and the raised flaps.
                opening = smooth(13.02,13.57,y)*.33+.031
                inset = (1-smooth(opening-.028,opening+.018,ax))*smooth(13.03,13.13,y)
                inset *= 1-smooth(13.66,13.78,y)
                knit = 65.+.58*(rgb.mean(axis=2)-49.)
                topstitch = 2.6*np.exp(-((ax-opening+.023)/.014)**2)
                rgb += (knit[...,None]+topstitch[...,None]-rgb)*inset[...,None]
        rgb = np.clip(rgb,0,255).astype(np.uint8)
        selected = blank[yy,xx]
        blank[yy,xx] = np.where(keep[...,None],rgb,selected)

    # Collar occupies a previously unused UV island. Edge piping and two rows
    # of fine stitches follow the collar instead of scrolling across the shirt.
    box = (int(.83*SIZE),int(.025*SIZE),int(.98*SIZE),int(.175*SIZE))
    img = Image.fromarray(blank,"RGB")
    patch = sheet.crop((46,102,447,384)).resize((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS)
    collar = np.asarray(patch.filter(ImageFilter.GaussianBlur(5)),dtype=np.float32)
    y,x = np.ogrid[:collar.shape[0],:collar.shape[1]]
    seam_x = np.minimum(np.abs(x-.022*SIZE),np.abs(x-.142*SIZE))
    seam_y = np.minimum(np.abs(y-.124*SIZE),np.abs(y-.119*SIZE))
    stitching = ((7+10*(y%13<8))*np.exp(-(seam_x/2.1)**2) +
                 (6+9*(x%13<8))*np.exp(-(seam_y/2.0)**2))
    collar += stitching[...,None]
    collar = collar*1.04+8
    collar = .63*collar+.37*collar.mean(axis=2,keepdims=True)
    tab_u = ((x+box[0])/SIZE-.846)/.132
    tab_v = ((y+box[1])/SIZE-.086)/.066
    tab_region = (tab_u>-.015)&(tab_u<1.015)&(tab_v>-.015)&(tab_v<1.015)
    edge = np.minimum(np.minimum(tab_u,1-tab_u),1-tab_v)
    seam = np.exp(-((edge-.052)/.020)**2)*(3.+2.4*(y%11<6))
    fold = 3.8*np.exp(-((tab_u-.37-.10*tab_v)/.30)**2)
    collar += (tab_region*(fold+seam-3.0*np.exp(-(edge/.026)**2)))[...,None]
    collar = np.clip(collar,0,255).astype(np.uint8)
    img.paste(Image.fromarray(collar,"RGB"), box[:2])
    knit_box = (int(.836*SIZE),int(.171*SIZE),int(.98*SIZE),int(.204*SIZE))
    knit = np.asarray(sheet.crop((83,61,424,169)).resize((knit_box[2]-knit_box[0],knit_box[3]-knit_box[1])),
                      dtype=np.float32).mean(axis=2)
    yy,xx = np.ogrid[:knit.shape[0],:knit.shape[1]]
    rim = np.exp(-((yy-20)/17.)**2)
    knit = .47*knit+45-5*rim+2.7*np.sin(xx*.85)*(.4+.6*rim)
    img.paste(Image.fromarray(np.uint8(np.clip(np.repeat(knit[...,None],3,axis=2),0,255)),"RGB"),knit_box[:2])
    def paste_atlas_piece(source, u0,v0,u1,v1):
        box = (round(u0*SIZE),round((1-v1)*SIZE),round(u1*SIZE),round((1-v0)*SIZE))
        img.paste(source.resize((box[2]-box[0],box[3]-box[1]),Image.Resampling.LANCZOS),box[:2])
    pocket = sheet.crop((552,537,991,988)).convert("L")
    weave = np.asarray(pocket,dtype=np.float32)
    contrast = weave-np.asarray(pocket.filter(ImageFilter.GaussianBlur(4)),dtype=np.float32)
    tone = np.clip(46+.32*(weave-weave.mean())+.21*contrast,23,78)
    pocket_color = np.stack((tone*.97,tone,tone*1.025),axis=2).astype(np.uint8)
    paste_atlas_piece(Image.fromarray(pocket_color,"RGB"),*POCKET_CHART)
    webbing = np.array(sheet.crop((1134,584,1249,637)),dtype=np.float32)
    webbing = 24.+.64*(webbing-webbing.mean())
    row = np.arange(webbing.shape[0])[:,None]
    edge_tone = 2.6*np.exp(-((row-14)/4.)**2)+2.3*np.exp(-((row-42)/3.7)**2)
    webbing += (edge_tone+4)[...,None]
    paste_atlas_piece(Image.fromarray(np.uint8(np.clip(webbing,0,255)),"RGB"),.840,.971,.983,.994)
    hardware = np.empty((65,128,3),dtype=np.uint8)
    rows = np.arange(65)[:,None]
    tone = 63.+30*np.exp(-((rows-12)/12.)**2)+16*np.exp(-((rows-55)/9.)**2)
    hardware[:] = np.stack((tone*.96,tone,tone*1.044),axis=2).astype(np.uint8)
    paste_atlas_piece(Image.fromarray(hardware,"RGB"),.843,.947,.919,.967)
    loop = fold_sheet.crop((302,810,489,999))
    paste_atlas_piece(loop,.936,.947,.976,.964)
    img.resize((RUNTIME_SIZE,RUNTIME_SIZE),Image.Resampling.LANCZOS).save(
        asset,quality=97,subsampling=0,optimize=True)
    return base


def build_boots(materials, manifest):
    source = materials/"hd-shoes04.jpg"
    fabric = ROOT/"assets/characters/fabric-macro-source-v4.png"
    pixels = np.array(Image.open(source).convert("RGB"),dtype=np.float32)
    image = Image.open(fabric).convert("L")
    midx,midy = image.width//2,image.height//2
    grain = image.crop((midx+15,midy+15,image.width-15,image.height-15))
    tile = np.asarray(grain.resize((512,512),Image.Resampling.LANCZOS),dtype=np.float32)
    tile -= float(tile.mean())
    yy,xx = np.ogrid[:RUNTIME_SIZE,:RUNTIME_SIZE]
    wr = smooth(16,42,pixels.mean(axis=2))
    fine = tile[yy%512,xx%512]
    pixels = pixels*.96+(fine*wr*.21)[...,None]
    # The worn sock panel in the original atlas is nearly white, including
    # the patch visible above the heel. Replace it with charcoal ribbing.
    panel = smooth(.828,.837,xx/RUNTIME_SIZE)*(1-smooth(.263,.274,yy/RUNTIME_SIZE))
    rib = 30.+3.8*np.sin(xx*.54)+1.55*np.sin(yy*1.34)+.12*fine
    pixels = pixels*(1-panel[...,None])+rib[...,None]*panel[...,None]
    asset = materials/"field-boots.jpg"
    Image.fromarray(np.uint8(np.clip(pixels,0,255)),"RGB").save(
        asset,quality=96,subsampling=0,optimize=True)
    manifest["textures"][asset.stem] = {
        "file":asset.name,"sha256":digest(asset),"size":RUNTIME_SIZE,
        "source":"Original ReyondBroom shoes04 leather atlas with charcoal ribbing and grain",
        "authoringInputs":{p.relative_to(ROOT).as_posix():digest(p) for p in (source,fabric)},
    }


def build_uniform(materials, manifest):
    asset = materials / "charcoal-utility.jpg"
    base = build_material(Image.open(SOURCE).convert("RGB"),make_garment(),asset)
    manifest["textures"][asset.stem] = {
        "file":asset.name,"sha256":digest(asset),"size":RUNTIME_SIZE,
        "source":"Original photographic cloth source sheet and folded workwear mesh",
        "authoringInputs":{p.relative_to(ROOT).as_posix():digest(p) for p in (base,SOURCE,FOLDS)},
    }
    for source in (SOURCE,FOLDS):
        manifest["pbrInputs"][source.relative_to(ROOT).as_posix()] = digest(source)
    build_boots(materials,manifest)
