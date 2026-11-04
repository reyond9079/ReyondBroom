"""Generate the original shader-pack island and disposable onboarding floor."""
from pathlib import Path
import math,itertools
from PIL import Image
r=Path(__file__).resolve().parents[1]/'runtime'
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def brush(points,material):
 center=tuple(sum(p[a] for p in points)/len(points) for a in range(3));planes=[];lines=['{']
 for a,b,c in itertools.combinations(points,3):
  n=cross(sub(b,a),sub(c,a));length=math.sqrt(dot(n,n))
  if length<1e-5:continue
  n=tuple(v/length for v in n);d=dot(n,a);sides=[dot(n,p)-d for p in points]
  if min(sides)<-1e-4 and max(sides)>1e-4:continue
  if dot(n,sub(center,a))<0:b,c=c,b;n=tuple(-v for v in n);d=-d
  if any(dot(n,old)>.999999 and abs(d-dist)<1e-4 for old,dist in planes):continue
  planes.append((n,d));lines.append(' '.join('( %.6f %.6f %.6f )'%p for p in (a,b,c))+' '+material+' 0 0 0 1 1')
 assert 4<=len(planes)<=64
 return '\n'.join(lines+['}'])+'\n'
def box(lo,hi,mat):return brush(list(itertools.product(*zip(lo,hi))),mat)
(r/'maps').mkdir(exist_ok=True)
(r/'maps/reyond-tutorial.map').write_text('{\n"classname" "worldspawn"\n'+box((-1536,-1536,-32),(1536,1536,0),'fb_grey')+'}\n',encoding='utf-8',newline='\n')
(r/'maps/reyond-tutorial-q3.map').write_text((r/'maps/reyond-tutorial.map').read_text(encoding='utf-8').replace('fb_grey','ftebroom/grid'),encoding='utf-8',newline='\n')
scene='// ReyondBroom profile=when-the-sky-went-dark format=quake3\n{\n"classname" "worldspawn"\n"message" "Reyond Shader Pack / Island"\n'
scene+=box((-24576,-24576,-384),(24576,24576,0),'reyondpack/ocean')
n=32
def ring(radius,z):return [(radius*(1+.055*math.sin(i*2.7))*math.cos(i*math.tau/n),radius*(1+.055*math.sin(i*2.7))*math.sin(i*math.tau/n),z+5*math.sin(i*1.3)) for i in range(n)]
inner=ring(400,92);beach=ring(650,9);outer=ring(760,-120)
for i in range(n):
 j=(i+1)%n
 scene+=brush([(0,0,106),inner[i],inner[j],(0,0,-128),(inner[i][0],inner[i][1],-128),(inner[j][0],inner[j][1],-128)],'reyond_sample/grass')
 for a,b,mat in [(inner,beach,'sand'),(beach,outer,'rock')]:
  p=[a[i],a[j],b[j],b[i]];scene+=brush(p+[(v[0],v[1],-256) for v in p],'reyond_sample/'+mat)
# A sky shell gives the shader rays a complete upper/lower hemisphere.
for axis in range(3):
 for side in [-1,1]:
  lo=[-30000,-30000,-4096];hi=[30000,30000,8192];lo[axis]=(hi[axis]-32 if side>0 else lo[axis]);hi[axis]=(hi[axis] if side>0 else lo[axis]+32);scene+=box(lo,hi,'reyondpack/sky')
scene+='}\n{\n"classname" "info_player_start"\n"origin" "0 -180 180"\n"angle" "90"\n}\n'
(r/'maps/reyond-island-sample.map').write_text(scene,encoding='utf-8',newline='\n')
folder=r/'textures/reyond_sample';folder.mkdir(parents=True,exist_ok=True)
for name,base in [('grass',(75,104,47)),('sand',(182,156,108)),('rock',(96,98,91))]:
 image=Image.new('RGB',(128,128));pixels=[]
 for y in range(128):
  for x in range(128):
   h=((x*73856093)^(y*19349663))&255;detail=(h/255-.5)*16+4*math.sin(x*.2)*math.cos(y*.25)
   pixels.append(tuple(int(max(0,min(255,c+detail))) for c in base))
 image.putdata(pixels);image.save(folder/(name+'.tga'))
print('Generated island (103 brushes), onboarding floor and original ground textures')
