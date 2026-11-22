#!/usr/bin/env python3
"""Generates a sample tileset PNG and a Tiled-format map (compound.json) so the game runs out of the box.
Run from the project root:  python3 tools/make_sample_assets.py
Later you replace these with your own art / maps made in Tiled; the format is identical."""
import json, random, struct, zlib, os

def write_png(path, w, h, px):
    raw = b''.join(b'\x00' + bytes(c for p in row for c in p) for row in px)
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) \
        + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(data)

def clamp(v): return max(0, min(255, int(v)))

def tile_pixel(kind, x, y, rnd):
    n = rnd.randint(-7, 7)
    if kind == 1:   # stone floor
        c = (222, 205, 170)
        if x in (0, 31) or y in (0, 31): c = (200, 183, 150)
        return (clamp(c[0]+n), clamp(c[1]+n), clamp(c[2]+n), 255)
    if kind == 2:   # brick wall
        row = y // 8
        off = 8 if row % 2 else 0
        mortar = (y % 8 == 0) or ((x + off) % 16 == 0)
        c = (62, 54, 72) if mortar else (96, 82, 108)
        return (clamp(c[0]+n), clamp(c[1]+n), clamp(c[2]+n), 255)
    if kind == 3:   # grass
        n = rnd.randint(-16, 16)
        return (clamp(92+n), clamp(150+n), clamp(80+n), 255)
    if kind == 4:   # door
        c = (150, 96, 54)
        if x % 8 == 0: c = (110, 68, 36)
        if 22 <= x <= 25 and 15 <= y <= 18: c = (240, 200, 70)
        return (clamp(c[0]+n), clamp(c[1]+n), clamp(c[2]+n), 255)
    if kind == 5:   # marigold garland (transparent background)
        for cx, cy in [(6, 8), (14, 12), (22, 8), (28, 13), (10, 18), (20, 19), (4, 15), (26, 20)]:
            if (x-cx)**2 + (y-cy)**2 <= 9:
                return (255, 150 + (cx % 3) * 30, 20, 255)
        if y in (10, 11): return (60, 130, 60, 255)
        return (0, 0, 0, 0)
    if kind == 6:   # carpet
        c = (150, 40, 52)
        if x < 3 or x > 28 or y < 3 or y > 28: c = (212, 170, 60)
        return (clamp(c[0]+n), clamp(c[1]+n), clamp(c[2]+n), 255)
    if kind == 7:   # crate / object marker
        c = (176, 128, 70)
        if x in (0, 31) or y in (0, 31) or abs(x-y) <= 1 or abs(x+y-31) <= 1: c = (110, 76, 40)
        return (clamp(c[0]+n), clamp(c[1]+n), clamp(c[2]+n), 255)
    # 8: water
    c = (70, 120, 200)
    if (y + x // 4) % 8 < 2: c = (120, 170, 235)
    return (clamp(c[0]+n), clamp(c[1]+n), clamp(c[2]+n), 255)

def make_tileset(path):
    T, cols, rows = 32, 4, 2
    px = [[(0, 0, 0, 0)] * (T * cols) for _ in range(T * rows)]
    for k in range(8):
        rnd = random.Random(k)
        ox, oy = (k % cols) * T, (k // cols) * T
        for y in range(T):
            for x in range(T):
                px[oy + y][ox + x] = tile_pixel(k + 1, x, y, rnd)
    write_png(path, T * cols, T * rows, px)

ROWS = [
    "####################", "#..................#", "#..####....####....#", "#..#..#....#..#....#",
    "#..#..#....#..#....#", "#..##.#....##.#....#", "#..................#", "#.......o..........#",
    "#..................#", "#....####..........#", "#....#..#....o.....#", "#....#..#..........#",
    "#..................#", "#D................D#", "####################",
]
W, H, T = 20, 15, 32
FLOOR, WALL, DOOR, MARIGOLD, CARPET, CRATE = 1, 2, 4, 5, 6, 7
CARPET_TILES = [(4,3),(5,3),(4,4),(5,4),(12,3),(13,3),(12,4),(13,4),(13,5),(6,10),(7,10),(6,11),(7,11)]

def layer(id_, name, data, visible=True):
    return {"id": id_, "name": name, "type": "tilelayer", "width": W, "height": H, "x": 0, "y": 0,
            "opacity": 1, "visible": visible, "data": data}

def prop(name, value): return {"name": name, "type": "string", "value": value}

def obj(id_, name, cls, tx, ty, props, point=False):
    return {"id": id_, "name": name, "class": cls, "type": "", "rotation": 0, "visible": True,
            "x": tx * T if not point else tx, "y": ty * T if not point else ty,
            "width": 0 if point else T, "height": 0 if point else T,
            **({"point": True} if point else {}), "properties": props}

def make_map(path):
    ground, walls, coll, decor = [], [], [], []
    for ty, row in enumerate(ROWS):
        for tx, ch in enumerate(row):
            g = FLOOR
            if (tx, ty) in CARPET_TILES: g = CARPET
            if ch == 'D': g = DOOR
            ground.append(g)
            walls.append(WALL if ch == '#' else 0)
            coll.append(WALL if ch == '#' else 0)
            d = 0
            if ch == 'o': d = CRATE
            if ty == 1 and 8 <= tx <= 11: d = MARIGOLD
            decor.append(d)
    objects = [
        obj(1, "start", "spawn", 80, 48, [], point=True),
        obj(2, "Where he fell", "clue", 8, 7, [prop("clue_id", "c_fall_spot")]),
        obj(3, "Dr. Reddy", "clue", 9, 8, [prop("clue_id", "c_bruise")]),
        obj(4, "Videographer", "clue", 13, 10, [prop("clue_id", "c_garba_video")]),
        obj(5, "To cabin", "door", 1, 13, [prop("to", "cabin")]),
        obj(6, "To society office", "door", 18, 13, [prop("to", "society_office")]),
        obj(7, "To builder office", "door", 18, 7, [prop("to", "builder_office")]),
        obj(8, "To flat 701", "door", 5, 5, [prop("to", "flat")]),
        obj(9, "To flat 601", "door", 13, 5, [prop("to", "neighbour_flat")]),
        obj(10, "To terrace", "door", 6, 11, [prop("to", "terrace")]),
        obj(11, "Mr. Gill", "npc", 15, 7, [prop("suspect_id", "s_gill")]),
        obj(12, "Pooja", "npc", 3, 8, [prop("suspect_id", "s_pooja")]),
    ]
    m = {"compressionlevel": -1, "width": W, "height": H, "tilewidth": T, "tileheight": T,
         "infinite": False, "orientation": "orthogonal", "renderorder": "right-down", "type": "map",
         "version": "1.10", "tiledversion": "1.10.2", "nextlayerid": 7, "nextobjectid": 13,
         "layers": [layer(1, "Ground", ground), layer(2, "Walls", walls), layer(3, "Decor", decor),
                    layer(4, "Collision", coll, visible=False),
                    {"id": 5, "name": "Objects", "type": "objectgroup", "draworder": "topdown",
                     "opacity": 1, "visible": True, "x": 0, "y": 0, "objects": objects}],
         "tilesets": [{"firstgid": 1, "columns": 4, "image": "../tilesets/tileset.png", "imagewidth": 128,
                       "imageheight": 64, "margin": 0, "spacing": 0, "name": "tileset", "tilecount": 8,
                       "tilewidth": T, "tileheight": T}]}
    json.dump(m, open(path, 'w'), indent=1)

if __name__ == "__main__":
    os.makedirs("assets/tilesets", exist_ok=True)
    os.makedirs("assets/maps", exist_ok=True)
    make_tileset("assets/tilesets/tileset.png")
    make_map("assets/maps/compound.json")
    print("wrote assets/tilesets/tileset.png and assets/maps/compound.json")
