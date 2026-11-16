#!/usr/bin/env python3
"""Decode RB-21 development fixtures without using the editor's exporters."""
import argparse
import json
import math
import struct
from pathlib import Path


def multiply(a,b):
    x,y,z,w = a
    i,j,k,l = b
    return (w*i+x*l+y*k-z*j, w*j-x*k+y*l+z*i, w*k+x*j-y*i+z*l, w*l-x*i-y*j-z*k)


def rotate(q, p):
    conjugate = (-q[0],-q[1],-q[2],q[3])
    return multiply(multiply(q,(*p,0)),conjugate)[:3]


def world_pose(parents, local):
    result = []
    for parent,(position,rotation) in zip(parents,local):
        length = math.sqrt(sum(x*x for x in rotation))
        assert abs(length-1)<.0002
        rotation = tuple(x/length for x in rotation)
        if parent>=0:
            p,q = result[parent]
            position = tuple(x+y for x,y in zip(p,rotate(q,position)))
            rotation = multiply(q,rotation)
        result.append((position,rotation))
    return result


def glb(path):
    data = path.read_bytes()
    magic, version, length, json_size, json_magic = struct.unpack_from("<4sIII4s", data)
    assert (magic, version, length, json_magic) == (b"glTF", 2, len(data), b"JSON")
    doc = json.loads(data[20:20 + json_size])
    binary_size, kind = struct.unpack_from("<I4s", data, 20 + json_size)
    binary = data[28 + json_size:]
    assert kind == b"BIN\0" and len(binary) == binary_size

    def accessor(index):
        item = doc["accessors"][index]
        view = doc["bufferViews"][item["bufferView"]]
        assert view["buffer"] == 0
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[item["type"]]
        field = {5121: "B", 5125: "I", 5126: "f"}[item["componentType"]]
        fmt = struct.Struct("<" + width * field)
        size = fmt.size * item["count"]
        start = view["byteOffset"] + item.get("byteOffset", 0)
        assert start % 4 == 0 and size <= view["byteLength"]
        return list(fmt.iter_unpack(binary[start:start+size]))

    skin, = doc["skins"]
    assert len(skin["joints"]) == len(accessor(skin["inverseBindMatrices"])) == 79
    mesh, = doc["meshes"]
    attrs = mesh["primitives"][0]["attributes"]
    joints, weights = accessor(attrs["JOINTS_0"]), accessor(attrs["WEIGHTS_0"])
    assert len(joints) == len(weights)
    assert all(max(row) < 79 for row in joints)
    assert all(min(row) >= 0 and abs(sum(row) - 1) < 1e-5 for row in weights)
    if "targetNames" in mesh.get("extras", {}):
        expected = ["Smile", "Frown", "Surprise", "Blink", "Anger"]
        assert mesh["extras"]["targetNames"] == expected
        assert mesh["weights"] == [1, 0, 0, 0, 0]
        expressions = mesh["primitives"][0]["targets"]
        assert len(expressions) == len(expected)
        assert all(primitive["targets"] == expressions for primitive in mesh["primitives"])
        for target in expressions:
            positions, normals = accessor(target["POSITION"]), accessor(target["NORMAL"])
            assert len(positions) == len(normals) == len(weights)
            assert all(math.isfinite(value) for row in positions+normals for value in row)
            assert any(abs(value) > 1e-5 for row in positions for value in row)
        print("PASS GLB expression targets, selected weight and matching mesh topology", flush=True)
    assert len(doc["images"]) >= 5
    for image in doc["images"]:
        view = doc["bufferViews"][image["bufferView"]]
        first = view["byteOffset"]
        content = binary[first:first+view["byteLength"]]
        if image["mimeType"] == "image/png":
            assert content.startswith(b"\x89PNG\r\n\x1a\n")
        else:
            assert image["mimeType"] == "image/jpeg" and content.startswith(b"\xff\xd8")
    for image in doc["textures"]:
        assert image["source"] < len(doc["images"])
    by_name = {material["name"]: material for material in doc["materials"]}
    assert by_name["Hair"]["alphaMode"] == "MASK"
    assert all("baseColorTexture" in by_name[name]["pbrMetallicRoughness"]
               for name in ("Skin", "Eyes", "Clothing", "Footwear", "Hair"))
    rotations = 0
    parents = [-1]*79
    sockets = {}
    for parent in range(79):
        for child in doc["nodes"][parent].get("children",[]):
            if child not in skin["joints"]:
                node = doc["nodes"][child]
                assert node["extras"]["reyondGearSocket"] == 1
                sockets[node["name"]] = doc["nodes"][parent]["name"]
                assert any(abs(value)>1e-4 for value in node["translation"])
                continue
            assert parents[child]==-1 and parent<child<79
            parents[child]=parent
    assert sockets == {"gear.hand.L":"wrist.L", "gear.hand.R":"wrist.R",
                       "gear.head":"head", "gear.back":"spine03"}
    rest = [node["translation"] for node in doc["nodes"][:79]]
    poses = {}
    for clip in doc["animations"]:
        changed = False
        local = {}
        for channel in clip["channels"]:
            sampler = clip["samplers"][channel["sampler"]]
            t, keys = accessor(sampler["input"]), accessor(sampler["output"])
            loop = clip["extras"]["loop"]
            count = len(t)-int(loop)
            frames = (0,count//4,count//2,count*3//4,count-1)
            for frame in frames:
                pose = local.setdefault(frame,[[p,(0,0,0,1)] for p in rest])
                target = channel["target"]
                pose[target["node"]][int(target["path"]=="rotation")] = keys[frame]
            assert len(t) == len(keys) and all(a < b for a,b in zip(t,t[1:]))
            assert all(math.isfinite(v) for k in keys for v in k)
            if channel["target"]["path"] == "rotation":
                rotations += 1
                assert all(abs(sum(v*v for v in q) - 1) < 1e-5 for q in keys)
                assert all(sum(x*y for x,y in zip(a,b)) >= -1e-6 for a,b in zip(keys,keys[1:]))
            changed |= any(a != b for a,b in zip(keys,keys[1:]))
        assert changed, clip["name"]
        poses[clip["name"]] = {frame:world_pose(parents,pose) for frame,pose in local.items()}
    assert rotations > 70
    return list(poses),poses


def iqm(path, reference):
    data = path.read_bytes()
    magic, *h = struct.unpack_from("<16s27I", data)
    assert magic == b"INTERQUAKEMODEL\0" and h[:2] == [2,len(data)]
    _, _, _, text_size, text_offset, mesh_count, mesh_offset, array_count, verts, arrays, \
        faces, face_offset, _, joint_count, joints, poses, pose_offset, clip_count, clips, \
        frames, channels, frame_offset, _, _, _, _, _ = h
    assert joint_count == poses == 79 and array_count == 6 and mesh_count >= 5
    assert verts > 1000 and faces > 1000 and frames > 500
    assert frame_offset + frames*channels*2 <= len(data)
    def name(offset):
        assert offset < text_size
        return data[text_offset+offset:text_offset+text_size].split(b"\0",1)[0].decode("ascii")
    parents = []
    for i in range(joint_count):
        offset, parent = struct.unpack_from("<Ii", data, joints+48*i)
        assert name(offset) and (-1 if i == 0 else 0) <= parent < i
        parents.append(parent)
    active = []
    for i in range(poses):
        parent, mask, *fields = struct.unpack_from("<iI20f", data, pose_offset+88*i)
        assert mask & ~127 == 0
        assert parent == struct.unpack_from("<i",data,joints+48*i+4)[0]
        active.append((mask,fields[:10],fields[10:]))
    assert sum(mask.bit_count() for mask,_,_ in active) == channels
    names = []
    max_position_error = 0.
    for i in range(clip_count):
        offset, first, length, fps, loop = struct.unpack_from("<IIIfI",data,clips+20*i)
        assert 0 <= first < first+length <= frames and 29 <= fps <= 33 and loop in (0,1)
        samples = [struct.unpack_from("<" + channels*"H",data,frame_offset+2*f*channels)
                   for f in (first,first+length//3,first+length-1)]
        assert len(set(samples)) > 1, name(offset)
        clip_name = name(offset)
        names.append(clip_name)
        for frame,wanted in reference[clip_name].items():
            cursor = iter(struct.unpack_from("<"+channels*"H",data,frame_offset+2*(first+frame)*channels))
            decoded = []
            for mask,values,steps in active:
                current = [value+(next(cursor)*step if mask & (1 << k) else 0)
                           for k,(value,step) in enumerate(zip(values,steps))]
                decoded.append((current[:3],current[3:7]))
            actual = world_pose(parents,decoded)
            for (a,rotation),(b,reference_rotation) in zip(actual,wanted):
                expected = (-40*b[2],-40*b[0],40*b[1])
                max_position_error = max(max_position_error,math.dist(a,expected))
                basis_rotated = (-reference_rotation[2],-reference_rotation[0],
                                 reference_rotation[1],reference_rotation[3])
                assert abs(sum(u*v for u,v in zip(basis_rotated,rotation)))>0.99999,clip_name
    assert max_position_error<.05,max_position_error
    print(f"PASS independent GLB/IQM world poses, max bone displacement {max_position_error:.6f} Quake units")
    table = dict((kind, (layout, width, offset)) for kind, _, layout, width, offset in
                 struct.iter_unpack("<5I",data[arrays:arrays+array_count*20]))
    assert table[4][:2] == table[5][:2] == (1,4)
    bone_offset, weight_offset = table[4][2],table[5][2]
    assert max(data[bone_offset:bone_offset+verts*4]) < 79
    weights = data[weight_offset:weight_offset+verts*4]
    for i in range(0,len(weights),4):
        row = weights[i:i+4]
        assert sum(row) == 255
        assert row.rstrip(b"\0").count(0) == 0, "IQM bone weights must be contiguous"
    for i in range(mesh_count):
        _, _, first, count, start, length = struct.unpack_from("<6I",data,mesh_offset+24*i)
        indices = struct.iter_unpack("<I",data[face_offset+start*12:face_offset+(start+length)*12])
        assert all(first <= vertex < first+count for vertex, in indices)
    return names


def legacy(folder, names):
    mdl = (folder/"quake1.mdl").read_bytes()
    magic,version = struct.unpack_from("<4sI",mdl)
    assert magic == b"IDPO" and version == 6
    skins,w,h,verts,faces,frames = struct.unpack_from("<6I",mdl,48)
    assert skins == 1 and verts <= 1024 and faces <= 2048 and frames > 200
    assert 0 < h <= 480 and w % 4 == 0
    frame_at = 88+w*h+verts*12+faces*16
    size = 28+4*verts
    assert len(mdl) == frame_at+size*frames
    assert len(set(mdl[88:88+w*h]))>32
    assert mdl[frame_at+28:frame_at+size] != mdl[frame_at+size+28:frame_at+2*size]

    md2 = (folder/"quake2.md2").read_bytes()
    m = struct.unpack_from("<4s16I",md2)
    assert m[0:2] == (b"IDP2",8) and m[6] <= 2048 and m[8] <= 4096
    assert m[10] == frames and m[-1] == len(md2)
    assert 0 < m[3] <= 480
    assert md2[m[11]:m[11]+64].split(bytes(1),1)[0] == b"models/character.pcx"
    assert md2[m[14]+40:m[14]+m[4]] != md2[m[14]+m[4]+40:m[14]+2*m[4]]
    skin = (folder/"character.pcx").read_bytes()
    assert skin[0] == 10 and skin[-769] == 12
    xmin,ymin,xmax,ymax = struct.unpack_from("<4H",skin,4)
    assert (xmax-xmin+1,ymax-ymin+1) == m[2:4]
    assert len(set(skin[128:-769]))>32

    md3 = (folder/"quake3.md3").read_bytes()
    h3 = struct.unpack_from("<4sI64s9I",md3)
    assert h3[:2] == (b"IDP3",15) and h3[4] == frames and h3[-1] == len(md3)
    assert 0 < h3[6] <= 32
    start = h3[-2]
    moved = 0
    for _ in range(h3[6]):
        s = struct.unpack_from("<4s64s10I",md3,start)
        assert s[0] == b"IDP3" and s[3] == frames and s[5] <= 999 and s[6] <= 1999
        at, stride = start+s[-2],s[5]*8
        moved += any(md3[at:at+stride] != md3[at+i*stride:at+(i+1)*stride]
                     for i in (frames//4,frames//2,frames-1))
        start += s[-1]
    assert start == len(md3) and moved == h3[6]

    studio = (folder/"goldsrc.mdl").read_bytes()
    assert studio[:8] == b"IDST\x0a\x00\x00\x00"
    assert struct.unpack_from("<I",studio,72)[0] == len(studio)
    bones,bone_at = struct.unpack_from("<II",studio,140)
    sequences,sequence_at = struct.unpack_from("<II",studio,164)
    assert bones == 79 and sequences == len(names)
    count, textures, texture_data, refs, families, skin_at = struct.unpack_from("<6I",studio,180)
    assert count >= 5 and refs == count and families == 1
    assert textures+80*count <= skin_at < skin_at+2*refs <= texture_data
    assert struct.unpack_from(f"<{refs}H",studio,skin_at) == tuple(range(count))
    body_count, bodies = struct.unpack_from("<2I",studio,204)
    assert 1 < body_count <= 32 and bodies+76*body_count <= texture_data
    total_vertices = 0
    total_triangles = 0
    used_skins = set()
    for i in range(body_count):
        variants, base, model = struct.unpack_from("<3I",studio,bodies+i*76+64)
        assert variants == base == 1
        mesh_count, mesh, vertices, vertex_bones, positions, normal_count, normal_bones, normals = (
            struct.unpack_from("<8I",studio,model+72)
        )
        assert mesh_count == 1 and 0 < vertices == normal_count <= 2048
        assert model+112 <= texture_data and mesh+20 <= texture_data
        assert max(vertex_bones+vertices,positions+12*vertices,
                   normal_bones+normal_count,normals+12*normal_count) <= texture_data
        tris,commands,skin_ref,mesh_normals,norm_base = struct.unpack_from("<5I",studio,mesh)
        assert skin_ref < count and mesh_normals == vertices and norm_base == 0
        assert commands+26*tris+2 <= texture_data
        for t in range(tris):
            length, *corners = struct.unpack_from("<13h",studio,commands+26*t)
            assert length == 3
            assert all(0 <= v < vertices for corner in range(0,12,4) for v in corners[corner:corner+2])
        assert struct.unpack_from("<h",studio,commands+26*tris)[0] == 0
        used_skins.add(skin_ref)
        total_vertices += vertices
        total_triangles += tris
    assert total_vertices > 2048 and total_triangles <= 20000
    assert used_skins == set(range(count))
    next_pixels = texture_data
    hair_masked = False
    for i in range(count):
        texture = textures+80*i
        w,h,pixels = struct.unpack_from("<III",studio,textures+80*i+68)
        material_name = studio[texture:texture+64].split(b"\0",1)[0]
        assert w == h == (512 if material_name == b"material0" else 256) and pixels == next_pixels
        next_pixels = pixels + w*h + 768
        assert len(set(studio[pixels:pixels+w*h]))>32
        if material_name == b"material5":
            assert struct.unpack_from("<I",studio,texture+64)[0] & 0x40
            assert studio[pixels] == 255 and 255 not in studio[pixels+8:pixels+w]
            hair_masked = True
    assert next_pixels == len(studio) and hair_masked
    for i, name in enumerate(names):
        at = sequence_at+176*i
        assert studio[at:at+32].split(b"\0",1)[0].decode() == name
        assert struct.unpack_from("<f",studio,at+32)[0] >= 24
        keys = struct.unpack_from("<I",studio,at+56)[0]
        anim = struct.unpack_from("<I",studio,at+124)[0]
        assert keys > 3 and anim+bones*12 < texture_data
        assert any(studio[anim:anim+bones*12])
    assert struct.unpack_from("<i",studio,bone_at+32)[0] == -1
    groups = json.loads((folder/"framegroups.json").read_text())
    assert [item["name"] for item in groups] == names
    assert sum(item["count"] for item in groups) == frames
    engine_groups = json.loads((folder/"quake3.md3.framegroups").read_text())["framegroups"]
    assert [group["name"] for group in engine_groups] == names
    assert [(group["firstpose"],group["numposes"],group["fps"],group["loop"]) for group in engine_groups] == [
        (group["first"],group["count"],group["fps"],int(group["loop"])) for group in groups
    ]
    print(f"PASS independent classic model decoders, {frames} baked frames, {sequences} studio sequences")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("glb",type=Path)
    parser.add_argument("iqm",type=Path)
    parser.add_argument("--legacy",type=Path)
    args = parser.parse_args()
    names,poses = glb(args.glb)
    assert len(set(names)) >= 25 and names == iqm(args.iqm,poses)
    print(f"PASS independent GLB and IQM decoding with {len(names)} named, moving clips")
    if args.legacy:
        legacy(args.legacy,names)
