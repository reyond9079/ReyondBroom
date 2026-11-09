#!/usr/bin/env python3
"""Emit the offline botanical catalogue and its C++ resource from one source.

Descriptors use architecture/organ traits, never seed/colour variants as species.
The original thirty IDs remain available. No external assets are downloaded.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAMILIES = {'tree':0,'conifer':1,'shrub':2,'grass':3,'fern':4,'flower':5,
            'mushroom':6,'cactus':7,'palm':8,'bamboo':9,'vine':10,'riparian':12}
ROWS = []

def species(key, name, latin, category, height, **parameters):
    ROWS.append(dict(id=key, name=name, scientificName=latin, category=category,
                     family=FAMILIES[category], parameters=dict(height=height, **parameters)))

# Crown: spreading, oval, columnar, weeping, umbrella, pyramidal, vase,
# multiple leaders, horizontally layered. Leaf geometry is a separate trait.
species('oak','English oak','Quercus robur','tree',240,crown=0,spread=.75,crownBase=.30,radius=9,leafShape=3,leafSize=14,leaves=28,leafWidth=.55,barkPattern=0,rootFlare=1.8,branches=12,branchAngle=62)
species('cork-oak','Cork oak','Quercus suber','tree',190,crown=7,clumps=2,spread=.80,crownBase=.25,radius=11,leafShape=1,leafSize=8,leafWidth=.55,barkPattern=6,evergreen=1,taper=.65,branchAngle=70)
species('birch','Silver birch','Betula pendula','tree',280,crown=1,spread=.38,crownBase=.30,radius=3.8,gravity=.65,leafShape=1,leafSize=8,leafWidth=.75,barkPattern=2,branchTaper=.45,branches=14,bark=[.78,.77,.70])
species('paper-birch','Paper birch','Betula papyrifera','tree',255,crown=7,clumps=3,spread=.42,crownBase=.40,radius=3.5,gravity=.18,leafShape=1,leafSize=11,leafWidth=.68,barkPattern=2,phyllotaxy=0,bark=[.84,.82,.74])
species('maple','Field maple','Acer campestre','tree',210,crown=0,spread=.65,crownBase=.32,phyllotaxy=1,leafShape=4,leaflets=5,leafSize=14,leafWidth=.9,barkPattern=0,branchAngle=60)
species('japanese-maple','Japanese maple','Acer palmatum','tree',130,crown=4,spread=.85,crownBase=.35,radius=3.2,phyllotaxy=1,leafShape=4,leaflets=7,leafSize=12,leafWidth=1,barkPattern=1,branches=10,clumps=2)
species('willow','Weeping willow','Salix babylonica','tree',245,crown=3,spread=.8,crownBase=.42,radius=8,gravity=1.3,branchLength=.9,leafShape=2,leafSize=15,leafWidth=.14,branches=16,leaves=34,barkPattern=0)
species('white-willow','White willow','Salix alba','tree',265,crown=6,spread=.60,crownBase=.24,radius=7,gravity=.5,leafShape=2,leafSize=12,leafWidth=.20,branches=14,clumps=2,barkPattern=0)
species('lombardy-poplar','Lombardy poplar','Populus nigra italica','tree',350,crown=2,spread=.19,crownBase=.12,radius=6,branchAngle=23,leafSize=11,leafWidth=.8,leafShape=1,branches=22,depth=3,barkPattern=0)
species('aspen','European aspen','Populus tremula','tree',260,crown=1,spread=.35,crownBase=.45,radius=4.2,leafShape=1,leafSize=9,leafWidth=1.05,barkPattern=1,petiole=.30,serration=.5)
species('beech','European beech','Fagus sylvatica','tree',285,crown=1,spread=.56,crownBase=.28,radius=10,leafShape=1,leafSize=12,leafWidth=.55,barkPattern=1,crownFill=1.05,branchAngle=57)
species('linden','Small-leaved lime','Tilia cordata','tree',270,crown=1,spread=.50,crownBase=.25,radius=8,leafShape=6,leafProfile=[.12,1.2,.9,.5,0],leafSize=13,leafWidth=.9,barkPattern=0,serration=.65)
species('ash','European ash','Fraxinus excelsior','tree',300,crown=6,spread=.55,crownBase=.40,radius=7,phyllotaxy=1,leafShape=7,leaflets=11,leafSize=24,leafWidth=.32,barkPattern=0,organ=1,crownFill=.7)
species('elm','English elm','Ulmus procera','tree',300,crown=6,spread=.70,crownBase=.35,radius=10,leafShape=1,leafSize=11,leafWidth=.65,barkPattern=0,serration=.8,branchAngle=48)
species('plane','London plane','Platanus acerifolia','tree',320,crown=7,spread=.68,crownBase=.28,radius=12,leafShape=4,leaflets=5,leafSize=23,leafWidth=.95,barkPattern=7,branches=10)
species('horse-chestnut','Horse chestnut','Aesculus hippocastanum','tree',250,crown=0,spread=.7,crownBase=.30,radius=9,phyllotaxy=1,leafShape=7,leaflets=7,leafSize=27,leafWidth=.42,organ=2,barkPattern=3,crownFill=1)
species('walnut','Common walnut','Juglans regia','tree',240,crown=0,spread=.8,crownBase=.32,radius=10,leafShape=7,leaflets=9,leafSize=28,leafWidth=.48,organ=1,barkPattern=0,branches=9)
species('alder','Black alder','Alnus glutinosa','tree',240,crown=5,spread=.45,crownBase=.18,radius=6,leafShape=1,leafSize=10,leafWidth=.85,barkPattern=3,serration=.6)
species('rowan','Rowan','Sorbus aucuparia','tree',180,crown=1,spread=.48,crownBase=.32,radius=4,leafShape=7,leaflets=13,leafSize=21,leafWidth=.28,organ=1,barkPattern=1)
species('apple','Orchard apple','Malus domestica','tree',120,crown=0,spread=.85,crownBase=.25,radius=6,leafShape=1,leafSize=10,leafWidth=.65,barkPattern=0,branches=8,curvature=.48,organ=4,flowerSize=5)
species('cherry','Wild cherry','Prunus avium','tree',185,crown=6,spread=.55,crownBase=.42,radius=5,leafShape=1,leafSize=12,leafWidth=.5,barkPattern=1,serration=.7,organ=4,flowerSize=4)
species('magnolia','Southern magnolia','Magnolia grandiflora','tree',195,crown=5,spread=.50,crownBase=.18,radius=6,leafShape=1,leafSize=24,leafWidth=.5,barkPattern=1,evergreen=1,leafCurl=.15,crownFill=1.05,organ=4,flowerSize=13)
species('baobab','African baobab','Adansonia digitata','tree',240,crown=4,spread=.75,crownBase=.70,radius=32,taper=.3,rootFlare=.6,leafShape=7,leaflets=5,leafSize=20,leafWidth=.4,organ=2,barkPattern=1,branches=7,depth=3)
species('umbrella-acacia','Umbrella thorn acacia','Vachellia tortilis','tree',180,crown=4,crownAspect=.38,crownBase=.60,spread=1,radius=7,branchAngle=75,leafShape=7,leaflets=17,leafSize=22,leafWidth=.16,organ=3,barkPattern=0,crownFill=.75)

species('pine','Scots pine','Pinus sylvestris','conifer',300,crown=4,spread=.48,crownBase=.42,radius=5,branches=11,needleForm=0,leafShape=5,leafSize=11,leafWidth=.8,evergreen=1,barkPattern=3,leaf=[.15,.30,.18])
species('maritime-pine','Maritime pine','Pinus pinaster','conifer',275,crown=6,spread=.60,crownBase=.48,radius=7,branches=10,needleForm=0,leafSize=18,leafShape=5,leafWidth=.9,evergreen=1,barkPattern=3)
species('stone-pine','Siberian stone pine','Pinus sibirica','conifer',290,crown=1,spread=.38,crownBase=.20,radius=6,branches=15,needleForm=0,leafSize=14,leafShape=5,leaflets=5,evergreen=1,barkPattern=3)
species('spruce','Norway spruce','Picea abies','conifer',290,crown=5,spread=.43,crownBase=.08,radius=5,branches=17,needleForm=1,gravity=.7,leafSize=10,leafShape=5,evergreen=1,barkPattern=3,crownFill=1.15,leaf=[.13,.27,.18])
species('blue-spruce','Blue spruce','Picea pungens','conifer',245,crown=5,spread=.44,crownBase=.05,radius=6,branches=15,needleForm=1,gravity=.12,leafSize=8,leafShape=5,evergreen=1,barkPattern=3,leaf=[.26,.39,.37],branchAngle=81)
species('fir','Silver fir','Abies alba','conifer',320,crown=5,spread=.36,crownBase=.13,radius=6,branches=16,needleForm=1,gravity=-.05,leafSize=9,leafShape=5,evergreen=1,barkPattern=1,phyllotaxy=1)
species('larch','European larch','Larix decidua','conifer',295,crown=5,spread=.40,crownBase=.16,radius=5,branches=14,needleForm=3,gravity=.48,leafSize=9,leafShape=5,evergreen=0,barkPattern=0,crownFill=.7)
species('cedar','Cedar of Lebanon','Cedrus libani','conifer',265,crown=8,spread=.78,crownBase=.22,radius=9,branches=9,needleForm=3,leafSize=10,leafShape=5,evergreen=1,barkPattern=0,gravity=.05,crownAspect=.55)
species('cypress','Mediterranean cypress','Cupressus sempervirens','conifer',290,crown=2,spread=.16,crownBase=.08,radius=5,branches=21,needleForm=2,leafSize=12,leafShape=5,evergreen=1,barkPattern=0,crownFill=1.2)
species('tree-juniper','Tree juniper','Juniperus excelsa','conifer',180,crown=7,spread=.50,crownBase=.12,radius=6,branches=11,needleForm=2,leafSize=10,leafShape=5,evergreen=1,barkPattern=4,clumps=2,curvature=.6)
species('redwood','Coast redwood','Sequoia sempervirens','conifer',460,crown=5,spread=.30,crownBase=.27,radius=15,branches=23,needleForm=1,leafSize=10,leafShape=5,evergreen=1,barkPattern=4,taper=.9)
species('araucaria','Monkey puzzle tree','Araucaria araucana','conifer',260,crown=5,spread=.55,crownBase=.18,radius=8,branches=10,needleForm=4,leafSize=12,leafShape=2,evergreen=1,barkPattern=3,phyllotaxy=2,leafWidth=.45)

species('boxwood','Round boxwood','Buxus sempervirens','shrub',64,crown=0,clumps=5,radius=1.2,spread=.65,branches=6,depth=2,leafShape=1,leafSize=5,leafWidth=.65,phyllotaxy=1,evergreen=1,crownFill=1.25)
species('hedge','Clipped hornbeam hedge','Carpinus betulus','shrub',104,crown=2,organ=5,clumps=7,radius=1.8,spread=.50,branches=5,depth=2,leafShape=1,leafSize=8,leafWidth=.58,serration=.8,evergreen=2,crownFill=1.15)
species('wild-shrub','Wild hawthorn shrub','Crataegus monogyna','shrub',82,crown=7,clumps=5,radius=1.8,spread=.72,branches=7,depth=2,leafShape=3,leafSize=7,leafWidth=.6,organ=6,flowerSize=3)
species('flowering-shrub','Flowering dog rose','Rosa canina','shrub',96,crown=6,clumps=4,radius=1.2,spread=.8,branches=6,depth=2,leafShape=7,leaflets=7,leafSize=15,leafWidth=.4,organ=4,flowerSize=7,petals=5,flower=[.88,.37,.51])
species('lilac','Common lilac','Syringa vulgaris','shrub',120,crown=6,clumps=5,radius=2,spread=.6,branches=6,depth=2,leafShape=6,leafProfile=[.15,1.15,.85,.5,0],leafSize=12,leafWidth=.8,phyllotaxy=1,organ=7,flowerSize=11,flower=[.63,.41,.75])
species('hydrangea','Bigleaf hydrangea','Hydrangea macrophylla','shrub',82,crown=0,clumps=7,radius=.9,spread=.7,branches=5,depth=2,leafShape=1,leafSize=16,leafWidth=.8,phyllotaxy=1,organ=8,flowerSize=12,flower=[.57,.62,.88])
species('rhododendron','Rhododendron','Rhododendron ponticum','shrub',110,crown=0,clumps=4,radius=2,spread=.8,branches=7,depth=2,leafShape=2,leafSize=17,leafWidth=.35,phyllotaxy=2,evergreen=1,organ=9,flowerSize=9,flower=[.75,.31,.57])
species('heather','Common heather','Calluna vulgaris','shrub',35,crown=5,clumps=12,radius=.2,spread=.45,branches=5,depth=2,leafShape=2,leafSize=2,leafWidth=.2,phyllotaxy=1,evergreen=1,organ=7,flowerSize=3,flower=[.7,.4,.59])
species('viburnum','Guelder rose','Viburnum opulus','shrub',130,crown=6,clumps=4,radius=2,spread=.75,branches=7,depth=2,leafShape=4,leaflets=3,leafSize=11,leafWidth=.9,phyllotaxy=1,organ=8,flowerSize=8)
species('currant','Blackcurrant','Ribes nigrum','shrub',70,crown=7,clumps=8,radius=.6,spread=.65,branches=4,depth=2,leafShape=4,leaflets=5,leafSize=9,leafWidth=1,organ=10,flowerSize=3)
species('gooseberry','Gooseberry','Ribes uva-crispa','shrub',65,crown=7,clumps=6,radius=.75,spread=.8,branches=5,depth=2,leafShape=4,leaflets=3,leafSize=6,leafWidth=.85,organ=6,branchAngle=84)
species('blackberry','Bramble blackberry','Rubus fruticosus','shrub',95,crown=3,clumps=5,radius=.65,spread=1,branches=5,depth=2,leafShape=7,leaflets=5,leafSize=22,leafWidth=.45,organ=10,gravity=.75)
species('creeping-juniper','Creeping juniper','Juniperus horizontalis','shrub',28,crown=4,clumps=6,radius=.6,spread=1.3,branches=6,depth=2,leafShape=5,leafSize=4,leafWidth=.25,needleForm=2,evergreen=1,crownAspect=.3)
species('barberry','Common barberry','Berberis vulgaris','shrub',88,crown=6,clumps=7,radius=.65,spread=.6,branches=5,depth=2,leafShape=1,leafSize=5,leafWidth=.7,phyllotaxy=2,organ=6,serration=.6)

for args in [
 ('meadow-grass','Meadow grass clump','Poa pratensis',38,0,30,4,.045,.55),
 ('lawn-grass','Low lawn grass','Festuca rubra',12,1,36,8,.055,.22),
 ('blue-fescue','Blue fescue','Festuca glauca',30,2,36,5,.025,.6),
 ('feather-grass','Feather grass','Stipa pennata',68,3,24,5,.018,.7),
 ('miscanthus','Chinese silver grass','Miscanthus sinensis',125,4,24,4,.065,.75),
 ('pampas-grass','Pampas grass','Cortaderia selloana',165,5,32,4,.055,.9),
 ('sedge','Tufted sedge','Carex cespitosa',48,6,28,5,.055,.48),
 ('hair-grass','Tufted hairgrass','Deschampsia cespitosa',75,7,24,4,.023,.40),
 ('foxtail-grass','Meadow foxtail','Alopecurus pratensis',65,8,24,4,.036,.3),
 ('couch-grass','Couch grass','Elymus repens',62,9,22,7,.045,.3),
 ('rye','Rye','Secale cereale',110,10,18,5,.035,.22),
 ('dry-grass','Dry steppe tussock','Stipa capillata',56,11,32,5,.025,.85)]:
 key,name,latin,h,organ,branches,clumps,width,curve=args
 species(key,name,latin,'grass',h,organ=organ,branches=branches,clumps=clumps,leafWidth=width,curvature=curve,radius=.25,dryness=.75 if organ==11 else 0,evergreen=3,flowerSize=9)

for key,name,latin,h,form,leaves,width in [
 ('bracken','Common bracken fern','Pteridium aquilinum',65,0,16,.24),
 ('ostrich-fern','Ostrich fern','Matteuccia struthiopteris',100,1,22,.28),
 ('wood-fern','Male wood fern','Dryopteris filix-mas',75,2,20,.32),
 ('lady-fern','Lady fern','Athyrium filix-femina',70,3,26,.16),
 ('maidenhair','Maidenhair fern','Adiantum capillus-veneris',38,4,15,.75),
 ('birds-nest-fern','Birds nest fern','Asplenium nidus',90,5,10,.24),
 ('polypody','Common polypody','Polypodium vulgare',35,6,18,.35),
 ('tree-fern','Soft tree fern','Dicksonia antarctica',230,7,25,.20)]:
 species(key,name,latin,'fern',h,organ=form,branches=9 if form!=5 else 14,leaves=leaves,leafWidth=width,leafSize=12 if form!=7 else 23,radius=.55 if form!=7 else 9,gravity=.55,evergreen=1 if form in (5,6,7) else 0)

for key,name,latin,h,form,petals,layers,size,shape,clumps in [
 ('daisy','Oxeye daisy','Leucanthemum vulgare',42,0,18,1,13,2,3),
 ('sunflower','Wild sunflower','Helianthus annuus',90,12,26,2,22,6,1),
 ('rose','Garden rose','Rosa gallica',60,1,9,5,13,7,2),
 ('poppy','Corn poppy','Papaver rhoeas',50,2,4,1,17,3,5),
 ('tulip','Garden tulip','Tulipa gesneriana',38,2,6,1,12,2,3),
 ('daffodil','Wild daffodil','Narcissus pseudonarcissus',35,3,6,1,12,2,4),
 ('iris','Bearded iris','Iris germanica',60,4,6,1,17,2,3),
 ('lily','White lily','Lilium candidum',80,5,6,1,18,2,2),
 ('lavender','Lavender','Lavandula angustifolia',45,6,5,1,5,2,9),
 ('lupin','Garden lupin','Lupinus polyphyllus',90,11,5,1,7,7,3),
 ('foxglove','Common foxglove','Digitalis purpurea',115,10,5,1,10,1,2),
 ('bellflower','Peach-leaved bellflower','Campanula persicifolia',65,7,5,1,12,2,4),
 ('clover','Red clover','Trifolium pratense',25,8,5,1,7,7,6),
 ('dandelion','Dandelion seed head','Taraxacum officinale',28,9,20,1,11,3,3)]:
 color={'rose':[.66,.12,.22],'poppy':[.85,.15,.10],'tulip':[.78,.18,.32],'iris':[.40,.26,.69],'lavender':[.52,.38,.70],'lupin':[.4,.3,.7],'foxglove':[.69,.30,.55],'bellflower':[.43,.39,.75],'clover':[.73,.39,.57]}.get(key,[.95,.92,.81] if key in ('daisy','lily','dandelion') else [.96,.70,.10])
 species(key,name,latin,'flower',h,organ=form,petals=petals,flowerLayers=layers,flowerSize=size,leafShape=shape,clumps=clumps,radius=.4 if h<65 else .8,leafSize=11 if h<65 else 17,leafWidth=.5 if shape!=2 else .20,leaflets=3 if key=='clover' else 7,flower=color,phyllotaxy=1 if key=='lavender' else 0)

for key,name,latin,h,form,rad,spread,clumps in [
 ('amanita','Fly agaric','Amanita muscaria',32,0,2.5,.72,3),
 ('boletus','Porcini bolete','Boletus edulis',29,1,4.5,.8,2),
 ('oyster','Oyster mushroom cluster','Pleurotus ostreatus',25,2,1.8,1.15,5),
 ('chanterelle','Golden chanterelle','Cantharellus cibarius',28,3,2.3,.65,4),
 ('woodland','Shaggy inkcap','Coprinus comatus',24,4,.8,.4,6),
 ('honey-fungus','Honey fungus cluster','Armillaria mellea',22,5,1,.50,9),
 ('bracket-fungus','Bracket fungus','Fomes fomentarius',20,6,2,1.1,3),
 ('morel','Common morel','Morchella esculenta',26,7,1.7,.45,3),
 ('fantasy','Giant violet parasol','Legacy fantasy variety',150,0,9,.95,1)]:
 species(key,name,latin,'mushroom',h,organ=form,radius=rad,spread=spread,clumps=clumps,capThickness=.40 if form==1 else .2,gills=.15 if form==1 else .8,bark=[.82,.77,.65],flower=[.7,.10,.055] if key=='amanita' else [.91,.55,.1] if form==3 else [.42,.23,.61] if key=='fantasy' else [.36,.25,.14])

for key,name,latin,h,form,rad,width,leaves in [
 ('saguaro','Saguaro cactus','Carnegiea gigantea',180,0,13,.3,18),
 ('barrel-cactus','Golden barrel cactus','Echinocactus grusonii',48,1,24,.3,18),
 ('prickly-pear','Prickly pear','Opuntia ficus-indica',95,2,8,.8,14),
 ('cereus','Columnar cereus','Cereus repandus',220,3,12,.3,14),
 ('agave','Blue agave','Agave tequilana',70,4,2,.17,24),
 ('aloe','Aloe vera','Aloe barbadensis',46,5,2,.25,17),
 ('echeveria','Echeveria rosette','Echeveria elegans',18,6,1,.55,32),
 ('yucca','Adams needle yucca','Yucca filamentosa',95,7,2,.075,38)]:
 species(key,name,latin,'cactus',h,organ=form,radius=rad,leafWidth=width,leaves=leaves,branches=4 if form==0 else 1,ribs=18 if form==1 else 10,spines=.6,evergreen=1,leaf=[.28,.42,.23],flowerSize=9,spread=.72)

for key,name,latin,h,form,rad,fronds,size,spread in [
 ('date-palm','Date palm','Phoenix dactylifera',280,0,10,15,30,.78),
 ('coconut-palm','Coconut palm','Cocos nucifera',340,0,7,11,38,.95),
 ('washingtonia','Mexican fan palm','Washingtonia robusta',330,1,9,19,35,.55),
 ('chamaerops','European fan palm','Chamaerops humilis',130,1,6,15,27,.7),
 ('royal-palm','Royal palm','Roystonea regia',380,0,11,12,37,.75),
 ('rhapis','Lady palm','Rhapis excelsa',160,1,3,10,25,.6)]:
 species(key,name,latin,'palm',h,organ=form,radius=rad,branches=fronds,leafSize=size,spread=spread,leaves=26,leafWidth=.16,barkPattern=5,evergreen=1,curvature=.65 if key=='coconut-palm' else .2,clumps=3 if key in ('chamaerops','rhapis') else 1)

for key,name,latin,h,form,shape,leaves,width in [
 ('ivy','English ivy vine','Hedera helix',155,0,4,25,.9),
 ('virginia-creeper','Virginia creeper','Parthenocissus quinquefolia',190,1,7,24,.45),
 ('climbing-vine','Wild grapevine','Vitis vinifera',210,2,4,22,.95),
 ('wisteria','Chinese wisteria','Wisteria sinensis',200,3,7,26,.35),
 ('hops','Common hop','Humulus lupulus',210,4,4,26,.85),
 ('clematis','Old mans beard','Clematis vitalba',170,5,7,24,.4)]:
 species(key,name,latin,'vine',h,organ=form,leafShape=shape,leaves=leaves,leafWidth=width,leafSize=17,leaflets=5 if form==1 else 11 if form==3 else 3,phyllotaxy=1 if form in (0,4,5) else 0,curvature=.7,spread=.5,radius=.7,evergreen=1 if form==0 else 0)

for key,name,latin,h,rad,clumps,nodes,size in [
 ('bamboo','Golden bamboo grove','Phyllostachys aurea',210,2,5,10,20),
 ('black-bamboo','Black bamboo','Phyllostachys nigra',250,2.4,4,12,18),
 ('dwarf-bamboo','Dwarf bamboo','Pleioblastus pygmaeus',55,.55,12,7,14),
 ('giant-bamboo','Giant bamboo','Dendrocalamus giganteus',460,6,3,17,28)]:
 species(key,name,latin,'bamboo',h,radius=rad,clumps=clumps,branches=nodes,leafSize=size,leafWidth=.16,leafShape=2,phyllotaxy=1,barkPattern=5,evergreen=1,bark=[.12,.13,.10] if key=='black-bamboo' else [.40,.48,.22])
for key,name,latin,h,form,rad,clumps,width in [
 ('reeds','Common reed stand','Phragmites australis',130,0,.55,11,.09),
 ('cattail','Broadleaf cattail','Typha latifolia',140,1,.65,7,.045),
 ('bulrush','Lakeshore bulrush','Schoenoplectus lacustris',120,2,.8,13,.025),
 ('horsetail','Field horsetail','Equisetum arvense',55,3,.7,9,.015)]:
 species(key,name,latin,'riparian',h,organ=form,radius=rad,clumps=clumps,branches=8,leafSize=30,leafWidth=width,leafShape=2,barkPattern=5,evergreen=3,flowerSize=13)

def emit():
    minimum={'tree':24,'conifer':12,'shrub':14,'grass':12,'fern':8,'flower':14,
             'mushroom':8,'cactus':8,'palm':6,'vine':6,'bamboo':4,'riparian':4}
    assert len({r['id'] for r in ROWS})==len(ROWS)>=120
    for name,count in minimum.items():assert sum(r['category']==name for r in ROWS)>=count
    for row in ROWS:
        # Botanical pigment defaults are separate from editable RGB controls.
        # Keep explicit species colours (blue spruce/fescue, birch, black bamboo)
        # while giving the remaining species a plausible living tissue palette.
        category=row['category'];traits=row['parameters']
        evergreen=traits.get('evergreen')==1
        leaf={'tree':[.25,.36,.15],'conifer':[.17,.29,.18],
              'shrub':[.23,.34,.14],'grass':[.31,.40,.19],
              'fern':[.22,.36,.17],'flower':[.24,.35,.16],
              'palm':[.25,.36,.17],'bamboo':[.26,.37,.18],
              'vine':[.22,.34,.14],'riparian':[.29,.39,.18]}.get(category)
        if leaf:
            if evergreen and category in ('tree','shrub','vine'):leaf=[.19,.30,.15]
            traits.setdefault('leaf',leaf)
        if category in ('tree','conifer','shrub','palm'):
            bark={0:[.32,.29,.24],1:[.43,.43,.37],3:[.35,.29,.23],
                  4:[.34,.27,.21],5:[.44,.41,.33],6:[.37,.33,.27],
                  7:[.45,.44,.35]}.get(traits.get('barkPattern',0),[.32,.29,.24])
            traits.setdefault('bark',bark)
        if row['category'] in ('tree','shrub','fern','palm','bamboo','vine','riparian'):row['parameters'].setdefault('foliageMode',1)
        category=row['category'];traits=row['parameters'];habitat='wetland' if category=='riparian' else 'tropical' if category=='palm' else 'dryland' if category=='cactus' else 'meadow' if category=='grass' else 'woodland' if category in ('tree','conifer','fern','mushroom') else 'garden'
        if row['id'] in ('willow','white-willow','alder'):habitat='wetland'
        if row['id'] in ('umbrella-acacia','acacia'):habitat='savanna'
        tags=[row['scientificName'],category,habitat,'small' if traits['height']<=40 else 'medium' if traits['height']<=140 else 'large']
        if category in ('tree','conifer','shrub'):
            tags.append(['spreading','oval','columnar','weeping','umbrella','pyramidal','vase','multiple leaders','layered'][traits.get('crown',0)])
        if category in ('grass','fern','bamboo','riparian'):tags.append('clumping')
        if category=='flower':tags.append('flowering')
        if traits.get('leafShape')==7:tags.append('compound leaves')
        if category=='conifer':tags.append('needles')
        if traits.get('evergreen')==1:tags.append('evergreen')
        elif category in ('tree','shrub'):tags.append('deciduous')
        row['tags']=tags
    document={'format':'ReyondBroomBotanicalCatalog','version':2,'generatorVersion':4,
              'license':'Original procedural descriptors; GPL-3.0-or-later', 'minimumByCategory':minimum,'presets':ROWS}
    text=json.dumps(document,ensure_ascii=False,sort_keys=True,indent=2)+'\n'
    destination=ROOT/'runtime/plants/builtin/catalog.json';destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text(text)
    (ROOT/'native/plant/catalog_data.h').write_text('// Generated by scripts/build-plant-catalog.py. SPDX-License-Identifier: GPL-3.0-or-later\n#pragma once\nnamespace ftebroom::plant {\ninline constexpr const char botanicalCatalog[] = R"RBPLANT('+text+')RBPLANT";\n}\n')
    print('Botanical catalogue:',len(ROWS),'presets')

if __name__=='__main__':emit()
