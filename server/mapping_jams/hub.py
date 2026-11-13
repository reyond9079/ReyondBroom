"""Build the first-party Quake Hub from the perimeter of the Jam board."""

PORTALS = (
    ("north", ("A1", "A2", "A3", "A4", "A5")),
    ("east", ("B5", "C5", "D5")),
    ("south", ("E5", "E4", "E3", "E2", "E1")),
    ("west", ("D1", "C1", "B1")),
)
PLOTS = tuple(plot for _, plots in PORTALS for plot in plots)
GLYPHS = {
    "A": ("010", "101", "111", "101", "101"),
    "B": ("110", "101", "110", "101", "110"),
    "C": ("011", "100", "100", "100", "011"),
    "D": ("110", "101", "101", "101", "110"),
    "E": ("111", "100", "110", "100", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("110", "001", "010", "100", "111"),
    "3": ("110", "001", "010", "001", "110"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "110", "001", "110"),
}


def brush(low, high, material):
    x, y, z = low
    X, Y, Z = high
    planes = (
        ((x,Y,Z),(x,y,Z),(x,y,z)), ((X,y,z),(X,y,Z),(X,Y,Z)),
        ((X,y,Z),(X,y,z),(x,y,z)), ((x,Y,z),(X,Y,z),(X,Y,Z)),
        ((X,Y,z),(x,Y,z),(x,y,z)), ((x,y,Z),(x,Y,Z),(X,Y,Z)),
    )
    return "{\n" + "".join(" ".join(f"( {a} {b} {c} )" for a,b,c in face)
                           + f" {material} 0 0 0 1 1\n" for face in planes) + "}\n"


def entity(keys, brushes=()):
    return "{\n" + "".join(f'"{key}" "{value}"\n' for key, value in keys.items()) + "".join(brushes) + "}\n"


def source(playable, owned=()):
    stone, trim, digits = "med_csl_brk7_2", "med_tmpl_trim2", "med_tmpl_lit1"
    world = [brush((-800,-800,-24),(800,800,0),"med_csl_flr2_2"),
             brush((-800,-800,224),(800,800,248),"med_dbrick1_t3"),
             brush((-800,-800,0),(-768,800,224),stone),
             brush((768,-800,0),(800,800,224),stone),
             brush((-768,-800,0),(768,-768,224),stone),
             brush((-768,768,0),(768,800,224),stone)]
    entities = [entity({"classname":"info_player_start","origin":"0 0 24","angle":"90"}),
                entity({"classname":"light","origin":"0 0 168","light":"360"})]
    for side, plots in PORTALS:
        for index, plot in enumerate(plots):
            center = (2*index-len(plots)+1)*126

            def point(u, v, z):
                u += center
                if side == "north":
                    return u, v, z
                if side == "east":
                    return v, -u, z
                if side == "south":
                    return -u, -v, z
                return -v, u, z

            def box(u0, v0, z0, u1, v1, z1, material):
                a, b = point(u0,v0,z0), point(u1,v1,z1)
                return brush(tuple(map(min, a, b)), tuple(map(max, a, b)), material)

            world.extend((box(-76,692,0,-54,768,136,trim), box(54,692,0,76,768,136,trim),
                          box(-76,692,112,76,768,136,trim)))
            for letter, ch in enumerate(plot):
                for row, pixels in enumerate(GLYPHS[ch]):
                    for col, pixel in enumerate(pixels):
                        if pixel == "1":
                            x, z = -28+letter*32+col*8, 177-row*8
                            world.append(box(x,758,z,x+7,768,z+7,digits))
            x, y, z = point(0,630,147)
            active = plot in playable
            claimed = active or plot in owned
            world.append(box(-54,761,0,54,768,112,"met_blu_lit6" if active else "med_door4b"))
            entities.append(entity({"classname":"light","origin":f"{x} {y} {z}","light":"315",
                                    "_color":"0.34 0.82 1" if active else "1 0.58 0.29" if claimed else "0.66 0.66 0.66"}))
            state = "" if active else " - no playable map" if claimed else " - available"
            message = f"Plot {plot}{state}"
            entities.append(entity({"classname":"trigger_multiple","wait":"2","message":message},
                                   [box(-54,600,0,54,655,104,"trigger")]))
            if active:
                entities.append(entity({"classname":"trigger_changelevel","spawnflags":"1",
                                        "map":"jam_"+plot.lower()},
                                       [box(-54,704,0,54,755,104,"trigger")]))
    return (entity({"classname":"worldspawn","message":"Mapping Jams Hub","wad":"@jam-wad@",
                    "_minlight":"22"}, world) + "".join(entities)).encode("ascii")
