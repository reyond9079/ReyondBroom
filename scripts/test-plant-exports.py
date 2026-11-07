#!/usr/bin/env python3
"""Independently load and re-export the generated GLBs with trimesh/Pillow (QA only)."""
import argparse
import json
from pathlib import Path
import numpy as np
import trimesh

p=argparse.ArgumentParser(description=__doc__);p.add_argument('artifacts',type=Path);a=p.parse_args()
results=[]
for path in sorted(a.artifacts.glob('*.glb')):
 scene=trimesh.load(path,force='scene')
 assert scene.geometry,path
 count=0
 for mesh in scene.geometry.values():
  count+=len(mesh.faces)
  assert np.isfinite(mesh.vertices).all() and np.isfinite(mesh.vertex_normals).all()
  assert np.max(mesh.faces)<len(mesh.vertices)
  assert mesh.visual.uv.shape==(len(mesh.vertices),2)
  assert np.isfinite(mesh.visual.uv).all()
  material=mesh.visual.material
  assert material.baseColorTexture is not None and material.normalTexture is not None
  assert material.metallicRoughnessTexture is not None and material.occlusionTexture is not None
  assert material.alphaMode=='MASK' and material.doubleSided
  texture=np.asarray(material.baseColorTexture.convert('RGBA'))
  assert texture[:,:,3].min()==0 and texture[:,:,3].max()==255
 exported=scene.export(file_type='glb')
 reimport=trimesh.load(file_obj=__import__('io').BytesIO(exported),file_type='glb',force='scene')
 assert sum(len(m.faces) for m in reimport.geometry.values())==count
 assert np.allclose(scene.bounds,reimport.bounds,atol=.0001)
 results.append({'file':path.name,'triangles':count,'meshes':len(scene.geometry),'material_images':3,'reimport':True})
 print('PASS independent GLB roundtrip',path.name,count,flush=True)
assert results,'No GLB exports found'
for path in sorted(a.artifacts.glob('*.obj')):
 scene=trimesh.load(path,force='scene');assert scene.geometry
 for mesh in scene.geometry.values():
  assert len(mesh.faces)>0 and mesh.visual.uv is not None
  assert mesh.visual.material.image is not None
 results.append({'file':path.name,'triangles':sum(len(m.faces) for m in scene.geometry.values()),'reimport':True,'format':'OBJ/MTL/PNG'})
 print('PASS independent OBJ material reimport',path.name,flush=True)
(a.artifacts/'independent-glb-results.json').write_text(json.dumps(results,indent=2))
