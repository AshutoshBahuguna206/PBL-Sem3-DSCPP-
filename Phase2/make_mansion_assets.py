#!/usr/bin/env python3
"""Generates all art and maps for Rastogi Kothi (needs Pillow: pip install pillow):
  assets/tilesets/mansion.png   one tileset; big furniture is cut into 32x32 tiles
  assets/sprites/characters.png walking people (4 directions x 4 frames, 32x48 each)
  assets/maps/<location>.json   the 7 rooms in Tiled JSON format (edit them in Tiled if you like)
Run from the project root:  python3 tools/make_mansion_assets.py"""
import json, math, os, random
from PIL import Image, ImageDraw, ImageOps

T = 32
OUT = (34, 22, 18, 255)

def rgba(c): return tuple(c) if len(c) == 4 else (c[0], c[1], c[2], 255)
def sh(c, k): return (max(0, min(255, c[0] + k)), max(0, min(255, c[1] + k)), max(0, min(255, c[2] + k)), 255)
def blank(wt, ht): return Image.new('RGBA', (wt * T, ht * T), (0, 0, 0, 0))

def R(d, x0, y0, x1, y1, c): d.rectangle([x0, y0, x1 - 1, y1 - 1], fill=rgba(c))
def E(d, x0, y0, x1, y1, c): d.ellipse([x0, y0, x1 - 1, y1 - 1], fill=rgba(c))
def LN(d, pts, c, w=1): d.line(pts, fill=rgba(c), width=w)
def PG(d, pts, c): d.polygon(pts, fill=rgba(c))

def noise(img, amp, seed):
    rnd = random.Random(seed); px = img.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            r, g, b, a = px[x, y]
            if a > 200:
                n = rnd.randint(-amp, amp)
                px[x, y] = (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)), a)

def outline(img, c=OUT, thresh=150):
    src = img.copy(); sp = src.load(); px = img.load(); w, h = img.size
    for y in range(h):
        for x in range(w):
            if sp[x, y][3] <= thresh:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and sp[nx, ny][3] > thresh:
                        px[x, y] = rgba(c); break

def shadow(d, x0, y0, x1, y1, a=70): d.ellipse([x0, y0, x1, y1], fill=(0, 0, 0, a))

def box(d, x0, y0, x1, y1, fill, edge=OUT, light=18, dark=22):
    d.rectangle([x0, y0, x1 - 1, y1 - 1], fill=rgba(fill), outline=rgba(edge))
    LN(d, [(x0 + 1, y0 + 1), (x1 - 3, y0 + 1)], sh(fill, light))
    LN(d, [(x0 + 1, y1 - 2), (x1 - 3, y1 - 2)], sh(fill, -dark))

# ------------------------------------------------------------------ palette
WOOD_L, WOOD_M, WOOD_D = (190, 140, 92), (150, 104, 66), (104, 70, 44)
WOOD_DK = (78, 52, 34)
RED, RED_L, RED_D = (150, 44, 56), (186, 72, 84), (104, 28, 40)
TEAL, TEAL_L, TEAL_D = (46, 112, 124), (74, 146, 158), (30, 78, 88)
CREAM = (238, 228, 206)
STEEL, STEEL_L, STEEL_D = (186, 192, 202), (220, 226, 234), (120, 126, 138)
GOLD = (222, 180, 70)
PLASTER = (216, 200, 170)

# ------------------------------------------------------------------ atlas
class Atlas:
    COLS = 16
    def __init__(self): self.tiles = []; self.stamps = {}
    def add(self, name, img):
        w, h = img.size; assert w % T == 0 and h % T == 0, name
        wt, ht = w // T, h // T; gids = []
        for ty in range(ht):
            row = []
            for tx in range(wt):
                t = img.crop((tx * T, ty * T, tx * T + T, ty * T + T))
                if t.getchannel('A').getbbox() is None: row.append(0)
                else: self.tiles.append(t); row.append(len(self.tiles))
            gids.append(row)
        self.stamps[name] = (wt, ht, gids)
    def gid(self, name): return self.stamps[name][2][0][0]
    def save(self, path):
        rows = math.ceil(len(self.tiles) / self.COLS)
        img = Image.new('RGBA', (self.COLS * T, max(1, rows) * T), (0, 0, 0, 0))
        for i, t in enumerate(self.tiles): img.paste(t, ((i % self.COLS) * T, (i // self.COLS) * T))
        img.save(path)
        return self.COLS * T, max(1, rows) * T, len(self.tiles)

A = Atlas()

# ------------------------------------------------------------------ floors & walls
def planks(base, dark, seed):
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, base)
    for row in range(4):
        y = row * 8; LN(d, [(0, y), (T, y)], dark)
        x = (10 if row % 2 else 22); LN(d, [(x, y), (x, y + 7)], dark)
        LN(d, [(3 + row * 5, y + 4), (13 + row * 5, y + 4)], sh(base, 8))
    noise(img, 5, seed); return img

def checker(c1, c2, grout, seed, sq=16):
    img = blank(1, 1); d = ImageDraw.Draw(img)
    for j in range(T // sq):
        for i in range(T // sq): R(d, i * sq, j * sq, i * sq + sq, j * sq + sq, c1 if (i + j) % 2 == 0 else c2)
    for k in range(0, T, sq): LN(d, [(k, 0), (k, T)], grout); LN(d, [(0, k), (T, k)], grout)
    noise(img, 4, seed); return img

def flagstone():
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, (146, 146, 150))
    for k in (0, 16): LN(d, [(0, k), (T, k)], (98, 98, 106)); LN(d, [((k + 9) % T, k), ((k + 9) % T, k + 15)], (98, 98, 106))
    noise(img, 9, 3); return img

def carpet(vertical):
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, RED)
    if vertical:
        R(d, 0, 0, 3, T, GOLD); R(d, T - 3, 0, T, T, GOLD); R(d, 5, 0, 6, T, RED_D); R(d, T - 6, 0, T - 5, T, RED_D)
    else:
        R(d, 0, 0, T, 3, GOLD); R(d, 0, T - 3, T, T, GOLD); R(d, 0, 5, T, 6, RED_D); R(d, 0, T - 6, T, T - 5, RED_D)
    d.polygon([(16, 9), (23, 16), (16, 23), (9, 16)], fill=rgba(GOLD)); d.polygon([(16, 13), (19, 16), (16, 19), (13, 16)], fill=rgba(RED))
    noise(img, 4, 4); return img

def wall_upper():
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, PLASTER)
    for x in range(0, T, 8): R(d, x, 0, x + 1, T, sh(PLASTER, 10))
    R(d, 0, 0, T, 4, (128, 92, 62)); R(d, 0, 4, T, 5, (170, 130, 90)); R(d, 0, 5, T, 8, sh(PLASTER, -14))
    noise(img, 3, 5); return img

def wall_lower():
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, 10, PLASTER); R(d, 0, 0, T, 3, sh(PLASTER, -10))
    R(d, 0, 10, T, 13, (168, 126, 86)); R(d, 0, 13, T, 28, (112, 76, 50))
    for x in (0, 16): R(d, x + 1, 15, x + 15, 26, (126, 88, 58)); LN(d, [(x + 1, 15), (x + 14, 15)], (146, 106, 70))
    R(d, 0, 28, T, T, (78, 52, 34)); noise(img, 3, 6); return img

def wall_side():
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, (92, 64, 48)); R(d, 0, 0, T, 3, (140, 108, 80))
    R(d, 0, T - 3, T, T, (60, 40, 30)); noise(img, 4, 7); return img

def brick(dark=0):
    img = blank(1, 1); px = img.load()
    for y in range(T):
        for x in range(T):
            off = 8 if (y // 8) % 2 else 0
            m = (y % 8 == 0) or ((x + off) % 16 == 0)
            px[x, y] = (54, 46, 48, 255) if m else (104 - dark, 80 - dark, 72 - dark, 255)
    noise(img, 8, 8 + dark); return img

def wall_base(wt, ht, brickwall=False):
    img = blank(wt, ht)
    for ty in range(ht):
        for tx in range(wt):
            if brickwall: t = brick()
            else: t = wall_upper() if ty == 0 else wall_lower()
            img.paste(t, (tx * T, ty * T))
    return img

# ------------------------------------------------------------------ wall decor (2 tiles tall, includes wall background)
def window_stamp(brickwall=False):
    img = wall_base(2, 2, brickwall); d = ImageDraw.Draw(img)
    R(d, 14, 6, 50, 52, (96, 64, 40)); R(d, 17, 9, 47, 49, (140, 190, 230))
    for k in range(8): R(d, 17, 9 + k * 5, 47, 14 + k * 5, sh((150, 198, 235), -k * 2))
    R(d, 31, 9, 33, 49, (96, 64, 40)); R(d, 17, 28, 47, 30, (96, 64, 40)); R(d, 11, 50, 53, 55, (150, 112, 74))
    for x in (20, 26, 38, 43): LN(d, [(x, 12), (x - 3, 22)], (214, 230, 244))
    if not brickwall:
        for (x0, x1) in ((4, 15), (49, 60)):
            R(d, x0, 4, x1, 54, RED_D)
            for x in range(x0 + 2, x1, 3): LN(d, [(x, 5), (x, 53)], RED)
        R(d, 2, 3, 62, 6, (150, 112, 74))
    noise(img, 2, 9); return img

def door_stamp():
    img = wall_base(1, 2); d = ImageDraw.Draw(img)
    R(d, 3, 4, 29, 64, (70, 46, 30)); R(d, 5, 6, 27, 64, (140, 92, 54))
    R(d, 8, 10, 24, 28, (116, 76, 44)); R(d, 8, 33, 24, 58, (116, 76, 44))
    LN(d, [(8, 10), (23, 10)], (160, 112, 70)); LN(d, [(8, 33), (23, 33)], (160, 112, 70))
    E(d, 21, 40, 25, 44, GOLD); R(d, 3, 2, 29, 5, (150, 112, 74)); noise(img, 2, 10); return img

def door_side_tile():
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, (92, 64, 48))
    R(d, 3, 2, 29, 30, (70, 46, 30)); R(d, 5, 4, 27, 28, (142, 94, 56)); R(d, 8, 7, 24, 14, (118, 78, 46)); R(d, 8, 17, 24, 25, (118, 78, 46))
    E(d, 22, 15, 26, 19, GOLD); noise(img, 2, 11); return img

def painting(kind):
    img = wall_base(1, 2); d = ImageDraw.Draw(img)
    R(d, 3, 4, 29, 29, (176, 132, 58)); R(d, 5, 6, 27, 27, (120, 84, 36))
    if kind == 'land':
        R(d, 7, 8, 25, 25, (150, 200, 235)); PG(d, [(7, 25), (7, 17), (13, 13), (19, 18), (25, 14), (25, 25)], (74, 134, 82))
        E(d, 18, 9, 23, 14, (250, 220, 120))
    else:
        R(d, 7, 8, 25, 25, (46, 40, 52)); E(d, 12, 10, 20, 19, (226, 184, 146)); R(d, 10, 18, 22, 25, (150, 40, 52)); R(d, 11, 9, 21, 12, (30, 24, 24))
    return img

def wall_clock():
    img = wall_base(1, 2); d = ImageDraw.Draw(img)
    E(d, 5, 3, 27, 25, (96, 64, 40)); E(d, 7, 5, 25, 23, (240, 236, 220)); LN(d, [(16, 14), (16, 8)], (30, 22, 18), 1); LN(d, [(16, 14), (21, 16)], (30, 22, 18), 1)
    E(d, 15, 13, 18, 16, (30, 22, 18)); return img

def trophy():
    img = wall_base(1, 2); d = ImageDraw.Draw(img)
    R(d, 4, 4, 28, 30, (84, 56, 36)); E(d, 8, 8, 24, 26, (212, 168, 90)); E(d, 7, 7, 12, 12, (212, 168, 90)); E(d, 20, 7, 25, 12, (212, 168, 90))
    for (x, y) in ((11, 12), (19, 13), (14, 20), (18, 20)): E(d, x, y, x + 3, y + 3, (110, 74, 36))
    E(d, 12, 15, 15, 18, (30, 22, 18)); E(d, 18, 15, 21, 18, (30, 22, 18)); R(d, 14, 21, 19, 23, (50, 34, 28)); return img

def torch():
    img = wall_base(1, 2, True); d = ImageDraw.Draw(img)
    E(d, 4, 4, 28, 34, (255, 170, 60, 70)); R(d, 14, 22, 18, 34, (60, 56, 60)); R(d, 11, 20, 21, 23, (60, 56, 60))
    PG(d, [(16, 6), (21, 16), (19, 22), (13, 22), (11, 16)], (255, 150, 40)); PG(d, [(16, 11), (19, 18), (17, 21), (14, 21), (13, 18)], (255, 226, 120)); return img

def wall_shelf():
    img = wall_base(2, 2); d = ImageDraw.Draw(img)
    R(d, 4, 28, 60, 32, (150, 108, 70)); R(d, 4, 32, 60, 34, (84, 56, 36))
    for x in (8, 20, 36, 48): R(d, x, 14, x + 10, 28, STEEL); R(d, x, 14, x + 10, 17, STEEL_L); R(d, x + 3, 11, x + 7, 14, STEEL_D)
    R(d, 14, 4, 18, 10, (110, 80, 50)); LN(d, [(16, 10), (16, 14)], STEEL_D); return img

def chimney():
    img = wall_base(2, 2); d = ImageDraw.Draw(img)
    R(d, 24, 0, 40, 22, STEEL_D); R(d, 25, 0, 28, 22, STEEL)
    PG(d, [(4, 56), (60, 56), (48, 22), (16, 22)], STEEL); PG(d, [(8, 56), (56, 56), (51, 50), (13, 50)], STEEL_D)
    R(d, 12, 52, 52, 55, (255, 236, 170)); LN(d, [(16, 24), (48, 24)], STEEL_L); outline(img, (50, 54, 62)); return img

# ------------------------------------------------------------------ furniture
def sofa(wt, c, cl, cd):
    img = blank(wt, 2); d = ImageDraw.Draw(img); W = wt * T
    shadow(d, 4, 54, W - 4, 63)
    box(d, 6, 8, W - 6, 30, cd); R(d, 10, 12, W - 10, 26, c)
    n = wt
    for i in range(n):
        x0 = 12 + i * (W - 24) // n; x1 = 12 + (i + 1) * (W - 24) // n - 1
        box(d, x0, 26, x1, 46, cl)
    box(d, 2, 16, 14, 54, cd); box(d, W - 14, 16, W - 2, 54, cd)
    R(d, 14, 46, W - 14, 54, sh(cd, -14)); R(d, 6, 54, 10, 58, WOOD_DK); R(d, W - 10, 54, W - 6, 58, WOOD_DK)
    E(d, 16, 20, 30, 34, sh(CREAM, -10)); outline(img); return img

def armchair2(c=RED, cl=RED_L, cd=RED_D):
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 6, 54, 58, 63)
    box(d, 8, 6, 56, 28, cd); R(d, 12, 10, 52, 24, c); box(d, 4, 18, 16, 54, cd); box(d, 48, 18, 60, 54, cd)
    box(d, 14, 26, 50, 46, cl); R(d, 14, 46, 50, 54, sh(cd, -14)); R(d, 10, 54, 14, 58, WOOD_DK); R(d, 50, 54, 54, 58, WOOD_DK)
    outline(img); return img

def table(wt, cake=False, cloth=None):
    img = blank(wt, 2); d = ImageDraw.Draw(img); W = wt * T
    shadow(d, 4, 52, W - 4, 63)
    R(d, 6, 46, 12, 62, WOOD_DK); R(d, W - 12, 46, W - 6, 62, WOOD_DK)
    box(d, 2, 40, W - 2, 52, WOOD_D)
    box(d, 2, 8, W - 2, 42, WOOD_L)
    if cloth: R(d, 4, 10, W - 4, 40, cloth); R(d, 4, 10, W - 4, 13, sh(cloth, 20)); R(d, 4, 38, W - 4, 40, sh(cloth, -25))
    else: R(d, 12, 14, W - 12, 36, sh(CREAM, -6)); R(d, 12, 14, W - 12, 16, CREAM)
    for x in range(20, W - 20, 26):
        E(d, x, 16, x + 14, 24, (250, 250, 252)); E(d, x + 3, 17, x + 11, 22, (224, 228, 236))
        E(d, x, 28, x + 14, 36, (250, 250, 252)); E(d, x + 3, 29, x + 11, 34, (224, 228, 236))
    if cake:
        cx = W // 2
        E(d, cx - 18, 20, cx + 18, 38, (246, 246, 250)); R(d, cx - 12, 22, cx + 12, 32, (236, 120, 150)); E(d, cx - 12, 18, cx + 12, 28, (252, 170, 190))
        R(d, cx - 12, 28, cx + 12, 32, (214, 96, 128))
        for k in (-8, 0, 8): R(d, cx + k - 1, 12, cx + k + 1, 20, (250, 244, 160)); E(d, cx + k - 2, 8, cx + k + 2, 13, (255, 160, 40))
    outline(img); return img

def chair():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 6, 26, 26, 31)
    box(d, 8, 3, 24, 17, WOOD_D); R(d, 10, 6, 22, 14, RED); box(d, 7, 16, 25, 24, WOOD_M); R(d, 8, 24, 11, 30, WOOD_DK); R(d, 21, 24, 24, 30, WOOD_DK)
    outline(img); return img

def stool():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 6, 24, 26, 30)
    R(d, 9, 18, 12, 28, WOOD_DK); R(d, 20, 18, 23, 28, WOOD_DK); E(d, 6, 10, 26, 22, WOOD_L); E(d, 8, 11, 24, 19, sh(WOOD_L, 14)); outline(img); return img

def bed():
    img = blank(2, 3); d = ImageDraw.Draw(img); shadow(d, 2, 86, 62, 95)
    box(d, 2, 2, 62, 16, WOOD_D); R(d, 6, 5, 58, 13, WOOD_M)
    box(d, 3, 12, 61, 90, WOOD_M); R(d, 6, 15, 58, 86, (240, 240, 246))
    E(d, 8, 16, 30, 34, (252, 252, 255)); E(d, 34, 16, 56, 34, (252, 252, 255)); E(d, 10, 22, 28, 32, (222, 226, 236)); E(d, 36, 22, 54, 32, (222, 226, 236))
    box(d, 6, 40, 58, 86, (58, 92, 150)); R(d, 6, 40, 58, 48, (232, 232, 240)); R(d, 8, 58, 56, 60, (88, 124, 182)); R(d, 8, 72, 56, 74, (88, 124, 182))
    R(d, 6, 88, 10, 94, WOOD_DK); R(d, 54, 88, 58, 94, WOOD_DK); outline(img); return img

def nightstand():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 4, 26, 28, 31)
    box(d, 5, 12, 27, 28, WOOD_M); R(d, 8, 16, 24, 20, WOOD_D); E(d, 15, 17, 17, 19, GOLD)
    R(d, 14, 8, 18, 12, (60, 50, 44)); PG(d, [(9, 8), (23, 8), (20, 1), (12, 1)], (250, 224, 150)); outline(img); return img

def wardrobe():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 2, 58, 62, 63)
    box(d, 2, 0, 62, 60, WOOD_M); R(d, 0, 0, 64, 7, WOOD_D)
    for (x0, x1) in ((6, 30), (34, 58)):
        R(d, x0, 11, x1, 56, WOOD_D); R(d, x0 + 2, 13, x1 - 2, 54, sh(WOOD_M, 10)); LN(d, [(x0 + 2, 13), (x1 - 3, 13)], sh(WOOD_M, 28))
    E(d, 26, 30, 30, 34, GOLD); E(d, 34, 30, 38, 34, GOLD); outline(img); return img

def dresser():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 4, 58, 60, 63)
    box(d, 8, 0, 56, 30, (120, 84, 56)); R(d, 11, 3, 53, 27, (190, 220, 236)); LN(d, [(14, 24), (30, 6)], (236, 246, 252)); LN(d, [(22, 25), (34, 12)], (236, 246, 252))
    box(d, 4, 30, 60, 56, WOOD_M)
    for x in (6, 24, 42): R(d, x + 2, 34, x + 16, 52, WOOD_D)
    for x in (13, 31, 49): E(d, x, 42, x + 3, 45, GOLD)
    E(d, 12, 26, 20, 30, (240, 120, 150)); R(d, 40, 24, 46, 30, (90, 150, 190)); outline(img); return img

def bookshelf():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 2, 58, 62, 63)
    box(d, 2, 0, 62, 60, WOOD_D); R(d, 5, 3, 59, 57, (44, 28, 22))
    rnd = random.Random(21); cols = [(150, 40, 44), (44, 80, 130), (60, 120, 70), (202, 162, 60), (120, 70, 130), (172, 100, 50), (200, 200, 190)]
    for top in (3, 17, 31, 45):
        x = 6
        while x < 57:
            w = rnd.randint(3, 6); h = rnd.randint(8, 12)
            R(d, x, top + 12 - h, min(x + w, 58), top + 12, rnd.choice(cols)); x += w + 1
        R(d, 5, top + 12, 59, top + 14, WOOD_M)
    outline(img); return img

def desk_big():
    img = blank(3, 2); d = ImageDraw.Draw(img); shadow(d, 4, 54, 92, 63)
    box(d, 2, 8, 94, 30, WOOD_L); R(d, 10, 11, 40, 26, (244, 240, 226)); LN(d, [(13, 15), (37, 15)], (150, 150, 160)); LN(d, [(13, 19), (34, 19)], (150, 150, 160))
    R(d, 46, 11, 62, 27, (150, 60, 50)); LN(d, [(46, 19), (62, 19)], (240, 220, 160))
    R(d, 70, 14, 74, 24, (60, 50, 44)); PG(d, [(66, 14), (78, 14), (76, 8), (68, 8)], (250, 224, 150)); E(d, 82, 16, 88, 22, (24, 24, 30))
    box(d, 2, 30, 94, 54, WOOD_M)
    for x in (6, 36, 66): R(d, x, 34, x + 24, 50, WOOD_D); E(d, x + 10, 40, x + 14, 44, GOLD)
    outline(img); return img

def fireplace():
    img = blank(3, 2); d = ImageDraw.Draw(img); shadow(d, 4, 58, 92, 63)
    box(d, 4, 14, 92, 60, (130, 120, 112)); box(d, 0, 6, 96, 16, WOOD_M)
    R(d, 26, 24, 70, 58, (22, 16, 14)); R(d, 22, 20, 74, 25, (100, 92, 86))
    for (x, y, r) in ((36, 46, 7), (48, 42, 9), (58, 47, 7), (46, 50, 6)): E(d, x - r, y - r, x + r, y + r, (255, 140, 40))
    for (x, y, r) in ((46, 48, 4), (40, 50, 3), (54, 50, 3)): E(d, x - r, y - r, x + r, y + r, (255, 226, 120))
    R(d, 34, 54, 62, 58, (90, 60, 40)); LN(d, [(30, 20), (30, 20)], OUT)
    E(d, 8, 6, 18, 14, (230, 220, 200)); R(d, 80, 2, 84, 14, (190, 120, 60)); E(d, 78, 0, 86, 6, (80, 150, 90)); outline(img); return img

def side_table():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 5, 26, 27, 31)
    R(d, 14, 14, 18, 28, WOOD_DK); E(d, 5, 8, 27, 18, WOOD_M); E(d, 7, 9, 25, 15, sh(WOOD_M, 16))
    R(d, 13, 4, 19, 12, (236, 240, 244)); R(d, 14, 8, 18, 12, (250, 250, 250)); outline(img); return img

def coffee_table():
    img = blank(2, 1); d = ImageDraw.Draw(img); shadow(d, 4, 22, 60, 30)
    R(d, 8, 20, 12, 30, WOOD_DK); R(d, 52, 20, 56, 30, WOOD_DK); box(d, 4, 8, 60, 22, WOOD_M); R(d, 8, 10, 56, 18, sh(WOOD_M, 14))
    E(d, 12, 11, 24, 17, (250, 250, 252)); R(d, 34, 10, 46, 16, (214, 90, 70)); outline(img); return img

def console_table():
    img = blank(2, 1); d = ImageDraw.Draw(img); shadow(d, 4, 24, 60, 31)
    R(d, 8, 14, 12, 30, WOOD_DK); R(d, 52, 14, 56, 30, WOOD_DK); box(d, 4, 10, 60, 18, WOOD_L); R(d, 8, 18, 56, 22, WOOD_D)
    PG(d, [(16, 10), (24, 10), (22, 1), (18, 1)], (70, 120, 170)); R(d, 19, 0, 21, 1, (70, 120, 170))
    E(d, 15, -2, 18, 2, (230, 90, 110)); E(d, 22, -2, 25, 2, (240, 190, 70)); outline(img); return img

def grandfather_clock():
    img = blank(1, 2); d = ImageDraw.Draw(img); shadow(d, 4, 58, 28, 63)
    box(d, 6, 0, 26, 60, (104, 66, 40)); R(d, 4, 0, 28, 5, WOOD_DK); E(d, 9, 7, 23, 21, (240, 236, 220))
    LN(d, [(16, 14), (16, 9)], OUT); LN(d, [(16, 14), (20, 16)], OUT); R(d, 10, 26, 22, 54, (40, 28, 22)); R(d, 15, 28, 17, 46, GOLD); E(d, 12, 44, 20, 52, GOLD)
    outline(img); return img

def plant_big():
    img = blank(1, 2); d = ImageDraw.Draw(img); shadow(d, 4, 58, 28, 63)
    for (x, y, w, h, c) in ((2, 6, 14, 26, (60, 138, 72)), (16, 4, 14, 28, (74, 156, 86)), (8, 0, 16, 30, (52, 120, 64)), (0, 22, 12, 16, (74, 156, 86)), (20, 22, 12, 16, (60, 138, 72))):
        E(d, x, y, x + w, y + h, c)
    PG(d, [(8, 40), (24, 40), (22, 60), (10, 60)], (176, 94, 60)); R(d, 7, 38, 25, 43, (200, 116, 76)); outline(img); return img

def plant_small():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 6, 26, 26, 31)
    E(d, 6, 3, 18, 18, (60, 138, 72)); E(d, 14, 5, 26, 20, (74, 156, 86)); E(d, 10, 1, 22, 14, (52, 120, 64))
    PG(d, [(9, 18), (23, 18), (21, 30), (11, 30)], (176, 94, 60)); R(d, 8, 17, 24, 21, (200, 116, 76)); outline(img); return img

def floor_lamp():
    img = blank(1, 2); d = ImageDraw.Draw(img); shadow(d, 8, 58, 24, 63)
    E(d, 0, 0, 32, 28, (255, 236, 170, 50)); PG(d, [(8, 22), (24, 22), (20, 6), (12, 6)], (250, 224, 150)); R(d, 15, 22, 17, 56, (60, 50, 44)); E(d, 9, 54, 23, 61, (60, 50, 44))
    outline(img, thresh=100); return img

def coat_stand():
    img = blank(1, 2); d = ImageDraw.Draw(img); shadow(d, 6, 58, 26, 63)
    R(d, 15, 6, 17, 56, WOOD_DK); E(d, 9, 54, 23, 61, WOOD_DK)
    PG(d, [(6, 12), (26, 12), (24, 34), (8, 34)], (70, 80, 104)); R(d, 11, 4, 21, 8, (90, 64, 44)); E(d, 8, 1, 24, 6, (110, 80, 50)); outline(img); return img

def staircase():
    img = blank(4, 3); d = ImageDraw.Draw(img); W = 128
    shadow(d, 4, 88, 124, 95)
    for i in range(7):
        y = 84 - i * 11; xm = 10 + i * 3
        R(d, xm, y, W - xm, y + 11, sh(WOOD_L, -i * 6)); R(d, xm, y, W - xm, y + 2, sh(WOOD_L, 14 - i * 6))
        R(d, xm + 14, y, W - xm - 14, y + 11, sh(RED, -i * 5)); R(d, xm + 14, y, W - xm - 14, y + 2, sh(RED_L, -i * 5))
    R(d, 24, 0, W - 24, 14, (40, 28, 24))
    for x in (6, 118): R(d, x, 6, x + 4, 92, WOOD_DK)
    LN(d, [(8, 90), (26, 8)], WOOD_D, 3); LN(d, [(W - 8, 90), (W - 26, 8)], WOOD_D, 3)
    for k in range(6):
        x = 10 + k * 3
        LN(d, [(x + k * 2 + 2, 86 - k * 12), (x + k * 2 + 2, 74 - k * 12)], WOOD_M, 2)
        LN(d, [(W - x - k * 2 - 3, 86 - k * 12), (W - x - k * 2 - 3, 74 - k * 12)], WOOD_M, 2)
    outline(img); return img

def gas_stove():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 2, 58, 62, 63)
    box(d, 2, 22, 62, 60, STEEL); R(d, 2, 8, 62, 24, (30, 32, 38)); R(d, 2, 8, 62, 10, (70, 74, 84))
    for (cx, cy) in ((18, 15), (46, 15)):
        E(d, cx - 8, cy - 5, cx + 8, cy + 5, (60, 62, 70)); E(d, cx - 5, cy - 3, cx + 5, cy + 3, (30, 32, 38))
        for k in range(8):
            a = k * math.pi / 4; R(d, int(cx + 6 * math.cos(a)) - 1, int(cy + 3.4 * math.sin(a)) - 1, int(cx + 6 * math.cos(a)) + 1, int(cy + 3.4 * math.sin(a)) + 1, (90, 170, 255))
    for x in (10, 24, 40, 54): E(d, x - 3, 25, x + 3, 31, (40, 42, 50)); R(d, x - 1, 25, x + 1, 28, STEEL_L)
    R(d, 10, 34, 54, 56, (34, 38, 46)); R(d, 12, 36, 52, 54, (60, 70, 86)); LN(d, [(14, 38), (30, 38)], (110, 124, 150)); R(d, 14, 32, 50, 35, STEEL_L)
    outline(img); return img

def gas_cylinder():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 6, 26, 26, 31)
    E(d, 8, 2, 24, 10, (190, 50, 44)); R(d, 8, 6, 24, 26, (190, 50, 44)); E(d, 8, 22, 24, 30, (150, 34, 32)); R(d, 8, 12, 12, 26, (220, 90, 80))
    R(d, 13, 0, 19, 4, STEEL_D); outline(img); return img

def counter(wt, sink=False):
    img = blank(wt, 2); d = ImageDraw.Draw(img); W = wt * T; shadow(d, 2, 58, W - 2, 63)
    box(d, 2, 8, W - 2, 28, (220, 214, 204)); LN(d, [(4, 10), (W - 6, 10)], (244, 240, 232))
    if sink:
        R(d, 26, 11, 62, 25, STEEL); R(d, 29, 13, 59, 23, STEEL_D); R(d, 42, 4, 46, 12, STEEL_L); LN(d, [(46, 5), (54, 5)], STEEL_L, 2)
    else:
        E(d, 10, 12, 26, 26, (60, 130, 140)); R(d, 12, 8, 24, 14, (60, 130, 140)); R(d, 38, 12, 52, 26, (200, 160, 80)); R(d, 38, 12, 52, 15, (230, 196, 110))
    box(d, 2, 28, W - 2, 58, TEAL)
    for x in range(6, W - 6, 28): R(d, x, 32, x + 24, 54, TEAL_L); R(d, x + 2, 34, x + 22, 52, TEAL); E(d, x + 10, 42, x + 13, 45, GOLD)
    outline(img); return img

def fridge():
    img = blank(2, 3); d = ImageDraw.Draw(img); shadow(d, 2, 90, 62, 95)
    box(d, 4, 2, 60, 92, STEEL); R(d, 4, 34, 60, 37, STEEL_D)
    R(d, 50, 10, 54, 28, STEEL_D); R(d, 50, 44, 54, 70, STEEL_D); R(d, 8, 6, 46, 10, STEEL_L)
    R(d, 12, 14, 24, 24, (230, 90, 110)); R(d, 28, 18, 38, 26, (90, 160, 220)); outline(img); return img

def stacked_crates():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 2, 58, 62, 63)
    def crate(x, y, s):
        box(d, x, y, x + s, y + s, (178, 130, 72)); LN(d, [(x + 2, y + 2), (x + s - 3, y + s - 3)], (118, 82, 46)); LN(d, [(x + s - 3, y + 2), (x + 2, y + s - 3)], (118, 82, 46))
    crate(2, 30, 28); crate(32, 34, 28); crate(12, 6, 26); outline(img); return img

def sacks():
    img = blank(2, 1); d = ImageDraw.Draw(img); shadow(d, 2, 24, 62, 31)
    for x in (4, 30):
        E(d, x, 6, x + 26, 28, (206, 180, 130)); E(d, x + 4, 4, x + 22, 12, (186, 160, 112)); LN(d, [(x + 6, 10), (x + 20, 10)], (150, 124, 84))
    outline(img); return img

def matka():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 5, 26, 27, 31)
    E(d, 6, 8, 26, 30, (184, 98, 60)); R(d, 11, 4, 21, 10, (170, 88, 52)); E(d, 9, 8, 23, 14, (140, 70, 40)); R(d, 8, 16, 24, 18, (120, 60, 36)); outline(img); return img

def trunk():
    img = blank(2, 1); d = ImageDraw.Draw(img); shadow(d, 4, 24, 60, 31)
    box(d, 3, 6, 61, 28, (124, 82, 48)); R(d, 3, 6, 61, 12, (156, 108, 66)); R(d, 14, 6, 18, 28, (172, 144, 64)); R(d, 46, 6, 50, 28, (172, 144, 64))
    E(d, 29, 14, 35, 20, GOLD); outline(img); return img

def sheet_chair():
    img = blank(1, 2); d = ImageDraw.Draw(img); shadow(d, 4, 56, 28, 63)
    PG(d, [(8, 8), (24, 8), (28, 58), (4, 58)], (226, 224, 218)); PG(d, [(8, 8), (24, 8), (22, 22), (10, 22)], (242, 240, 236))
    LN(d, [(12, 26), (11, 56)], (196, 194, 188)); LN(d, [(20, 26), (21, 56)], (196, 194, 188)); outline(img, (90, 88, 86)); return img

def old_mirror():
    img = blank(1, 2); d = ImageDraw.Draw(img); shadow(d, 4, 58, 28, 63)
    box(d, 4, 2, 28, 58, (140, 100, 60)); R(d, 7, 5, 25, 55, (176, 196, 206)); LN(d, [(10, 12), (22, 30)], (214, 228, 234)); LN(d, [(10, 26), (18, 40)], (214, 228, 234))
    R(d, 3, 56, 8, 62, WOOD_DK); R(d, 24, 56, 29, 62, WOOD_DK); outline(img); return img

def box_small():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 4, 26, 28, 31)
    box(d, 5, 10, 27, 28, (176, 142, 96)); R(d, 5, 10, 27, 15, (200, 168, 120)); R(d, 14, 10, 18, 28, (214, 200, 150)); outline(img); return img

def suitcase():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 4, 26, 28, 31)
    box(d, 4, 10, 28, 28, (60, 54, 66)); R(d, 11, 6, 21, 10, (30, 26, 30)); R(d, 4, 18, 28, 20, (40, 36, 44)); R(d, 8, 14, 11, 17, GOLD); R(d, 21, 14, 24, 17, GOLD); outline(img); return img

def nameplate():
    img = blank(1, 1); d = ImageDraw.Draw(img); shadow(d, 3, 24, 29, 30)
    box(d, 3, 10, 29, 24, (180, 140, 60)); R(d, 5, 12, 27, 22, (206, 168, 80)); LN(d, [(7, 16), (24, 16)], (110, 80, 30)); LN(d, [(9, 19), (22, 19)], (110, 80, 30))
    LN(d, [(14, 11), (18, 23)], (60, 40, 20)); outline(img); return img

def tijori():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 2, 58, 62, 63)
    box(d, 4, 4, 60, 58, (74, 80, 92)); R(d, 8, 8, 56, 54, (96, 104, 118)); R(d, 8, 8, 56, 11, (130, 140, 156))
    E(d, 22, 18, 42, 38, GOLD); E(d, 26, 22, 38, 34, (150, 112, 40)); R(d, 31, 20, 33, 28, (240, 220, 140)); E(d, 30, 27, 34, 31, (30, 26, 20))
    R(d, 46, 26, 52, 40, STEEL_L); R(d, 8, 44, 14, 54, (60, 66, 76)); outline(img); return img

def document_shelf():
    img = blank(3, 2); d = ImageDraw.Draw(img); shadow(d, 2, 58, 94, 63)
    box(d, 2, 0, 94, 60, (80, 70, 74)); R(d, 5, 3, 91, 57, (34, 30, 34))
    rnd = random.Random(31); cols = [(70, 100, 140), (150, 60, 60), (200, 190, 150), (80, 130, 90), (230, 230, 220)]
    for top in (3, 22, 41):
        x = 7
        while x < 88:
            w = rnd.randint(4, 8); R(d, x, top + 4, min(x + w, 90), top + 17, rnd.choice(cols)); x += w + 1
        R(d, 5, top + 17, 91, top + 19, (110, 100, 104))
    outline(img); return img

def desk_old():
    img = blank(2, 2); d = ImageDraw.Draw(img); shadow(d, 4, 56, 60, 63)
    box(d, 3, 10, 61, 32, WOOD_L); R(d, 8, 13, 30, 28, (244, 240, 226)); LN(d, [(10, 17), (26, 17)], (150, 150, 160)); LN(d, [(10, 21), (24, 21)], (150, 150, 160))
    R(d, 36, 13, 52, 28, (66, 100, 80)); R(d, 36, 13, 52, 16, (96, 134, 112)); LN(d, [(44, 16), (44, 28)], (230, 214, 150))
    E(d, 54, 16, 60, 22, (240, 210, 120)); box(d, 3, 32, 61, 56, WOOD_M)
    for x in (7, 36): R(d, x, 36, x + 22, 52, WOOD_D); E(d, x + 9, 42, x + 13, 46, GOLD)
    outline(img); return img

def rug(wt, ht, base, accent):
    img = blank(wt, ht); d = ImageDraw.Draw(img); W, H = wt * T, ht * T
    R(d, 0, 0, W, H, accent); R(d, 3, 3, W - 3, H - 3, base); R(d, 7, 7, W - 7, H - 7, accent); R(d, 9, 9, W - 9, H - 9, sh(base, 12))
    cx, cy = W // 2, H // 2
    d.polygon([(cx, cy - H // 3), (cx + W // 4, cy), (cx, cy + H // 3), (cx - W // 4, cy)], fill=rgba(accent))
    d.polygon([(cx, cy - H // 5), (cx + W // 7, cy), (cx, cy + H // 5), (cx - W // 7, cy)], fill=rgba(base))
    for x in range(14, W - 14, 12):
        for y in (12, H - 14): E(d, x, y, x + 4, y + 4, accent)
    noise(img, 3, wt * 7 + ht); return img

def cobweb(flip=False):
    img = blank(1, 1); d = ImageDraw.Draw(img); c = (236, 236, 244, 170)
    rays = [(30, 2), (30, 12), (26, 22), (14, 30), (4, 30)]
    for x, y in rays: LN(d, [(0, 0), (x, y)], c)
    for k in (0.35, 0.6, 0.85):
        pts = [(round(x * k), round(y * k)) for x, y in rays]
        for a, b in zip(pts, pts[1:]): LN(d, [a, b], c)
    return ImageOps.mirror(img) if flip else img

def glow():
    img = blank(1, 1); d = ImageDraw.Draw(img); E(d, 0, 0, 32, 32, (255, 170, 60, 38)); E(d, 6, 6, 26, 26, (255, 190, 90, 44)); return img

def iron_gate():
    img = blank(1, 1); d = ImageDraw.Draw(img); R(d, 0, 0, T, T, (70, 74, 84)); R(d, 0, 0, T, 2, (40, 42, 48)); R(d, 0, T - 2, T, T, (40, 42, 48))
    for y in (8, 16, 24): R(d, 0, y, T, y + 1, (50, 52, 60))
    for x, y in ((4, 4), (27, 4), (4, 27), (27, 27)): E(d, x - 1, y - 1, x + 2, y + 2, (160, 162, 170))
    E(d, 8, 8, 24, 24, GOLD); E(d, 11, 11, 21, 21, (160, 124, 40)); E(d, 14, 14, 18, 18, (30, 26, 20)); R(d, 15, 9, 17, 14, (240, 220, 140)); noise(img, 3, 20); return img

def build_atlas():
    A.add('floor_wood', planks((178, 128, 84), (146, 100, 64), 1))
    A.add('floor_dark', planks((112, 78, 52), (84, 56, 38), 2))
    A.add('floor_marble', checker((232, 222, 200), (150, 126, 100), (120, 100, 80), 3))
    A.add('floor_kitchen', checker((74, 140, 148), (232, 236, 236), (150, 170, 170), 4))
    A.add('floor_stone', flagstone())
    A.add('carpet_v', carpet(True)); A.add('carpet_h', carpet(False))
    A.add('wall_upper', wall_upper()); A.add('wall_lower', wall_lower()); A.add('wall_side', wall_side())
    A.add('brick', brick()); A.add('brick_up', brick()); A.add('brick_low', brick(10))
    A.add('window', window_stamp()); A.add('window_brick', window_stamp(True))
    A.add('door_top', door_stamp()); A.add('door_side', door_side_tile())
    A.add('painting_land', painting('land')); A.add('painting_portrait', painting('portrait'))
    A.add('wall_clock', wall_clock()); A.add('trophy', trophy()); A.add('torch', torch()); A.add('wall_shelf', wall_shelf()); A.add('chimney', chimney())
    A.add('sofa3', sofa(3, RED, RED_L, RED_D)); A.add('sofa2', sofa(2, TEAL, TEAL_L, TEAL_D))
    A.add('armchair', armchair2()); A.add('armchair_green', armchair2((60, 110, 76), (88, 140, 102), (40, 76, 52)))
    A.add('table_cake', table(4, cake=True)); A.add('table_plain', table(4)); A.add('table_kitchen', table(3, cloth=(206, 70, 70)))
    A.add('chair', chair()); A.add('stool', stool())
    A.add('bed', bed()); A.add('nightstand', nightstand()); A.add('wardrobe', wardrobe()); A.add('dresser', dresser())
    A.add('bookshelf', bookshelf()); A.add('desk_big', desk_big()); A.add('fireplace', fireplace())
    A.add('side_table', side_table()); A.add('coffee_table', coffee_table()); A.add('console_table', console_table())
    A.add('clock', grandfather_clock()); A.add('plant_big', plant_big()); A.add('plant_small', plant_small())
    A.add('floor_lamp', floor_lamp()); A.add('coat_stand', coat_stand()); A.add('staircase', staircase())
    A.add('stove', gas_stove()); A.add('cylinder', gas_cylinder()); A.add('counter_sink', counter(3, True)); A.add('counter', counter(2))
    A.add('fridge', fridge()); A.add('crates', stacked_crates()); A.add('sacks', sacks()); A.add('matka', matka())
    A.add('trunk', trunk()); A.add('sheet_chair', sheet_chair()); A.add('old_mirror', old_mirror()); A.add('box', box_small())
    A.add('suitcase', suitcase()); A.add('nameplate', nameplate()); A.add('tijori', tijori()); A.add('document_shelf', document_shelf()); A.add('desk_old', desk_old())
    A.add('rug_big', rug(6, 4, (58, 70, 128), GOLD)); A.add('rug_med', rug(4, 3, (140, 44, 56), GOLD)); A.add('rug_small', rug(3, 2, (46, 110, 120), GOLD))
    A.add('cobweb', cobweb()); A.add('cobweb_r', cobweb(True)); A.add('glow', glow()); A.add('iron_gate', iron_gate())

# ------------------------------------------------------------------ rooms
class Room:
    def __init__(self, w, h, floor, wallstyle='plaster'):
        self.w, self.h, self.style = w, h, wallstyle
        n = w * h
        self.ground = [A.gid(floor)] * n; self.walls = [0] * n; self.furn = [0] * n; self.decor = [0] * n; self.coll = [0] * n
        self.objects = []; self.nid = 1
        up, low, side = ('wall_upper', 'wall_lower', 'wall_side') if wallstyle == 'plaster' else ('brick_up', 'brick_low', 'brick')
        for x in range(w):
            self.walls[self.i(x, 0)] = A.gid(up); self.walls[self.i(x, 1)] = A.gid(low); self.coll[self.i(x, 0)] = self.coll[self.i(x, 1)] = 1
            self.walls[self.i(x, h - 1)] = A.gid(side); self.coll[self.i(x, h - 1)] = 1
        for y in range(2, h):
            for x in (0, w - 1): self.walls[self.i(x, y)] = A.gid(side); self.coll[self.i(x, y)] = 1
    def i(self, x, y): return y * self.w + x
    def layer(self, name): return {'ground': self.ground, 'walls': self.walls, 'furn': self.furn, 'decor': self.decor}[name]
    def place(self, name, x, y, layer='furn', block=None):
        wt, ht, gids = A.stamps[name]
        if block is None: block = (layer == 'furn')
        assert 0 <= x and x + wt <= self.w and 0 <= y and y + ht <= self.h, (name, x, y, self.w, self.h)
        lay = self.layer(layer)
        for ty in range(ht):
            for tx in range(wt):
                k = self.i(x + tx, y + ty)
                if block:
                    assert not self.coll[k], f"{name} at {x},{y} overlaps something at {x+tx},{y+ty}"
                    self.coll[k] = 1
                if gids[ty][tx]: lay[k] = gids[ty][tx]
    def fill_all_walls(self, tile):
        for y in range(self.h):
            for x in range(self.w): self.walls[self.i(x, y)] = A.gid(tile); self.coll[self.i(x, y)] = 1
    def carve(self, x0, y0, x1, y1, floor):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.ground[self.i(x, y)] = A.gid(floor); self.walls[self.i(x, y)] = 0; self.coll[self.i(x, y)] = 0
    def floor(self, name, x, y): self.place(name, x, y, 'ground', block=False)
    def obj(self, name, cls, x, y, w=1, h=1, **props):
        self.objects.append({"id": self.nid, "name": name, "class": cls, "type": "", "rotation": 0, "visible": True,
                             "x": x * T, "y": y * T, "width": w * T, "height": h * T,
                             "properties": [{"name": k, "type": "string", "value": str(v)} for k, v in props.items()]}); self.nid += 1
    def point(self, name, cls, px, py):
        self.objects.append({"id": self.nid, "name": name, "class": cls, "type": "", "rotation": 0, "visible": True,
                             "x": px, "y": py, "width": 0, "height": 0, "point": True, "properties": []}); self.nid += 1
    def door_top(self, x, to, name, **props):
        self.place('door_top', x, 0, 'walls', block=False); self.obj(name, 'door', x, 0, 1, 2, to=to, **props)
    def door_side(self, x, y, to, name, **props):
        self.place('door_side', x, y, 'walls', block=False); self.obj(name, 'door', x, y, to=to, **props)
    def npc(self, name, x, y, sprite, **props): self.obj(name, 'npc', x, y, sprite=sprite, **props)
    def save(self, path, cols, iw, ih, count):
        def lyr(i, name, data, visible=True):
            return {"id": i, "name": name, "type": "tilelayer", "width": self.w, "height": self.h, "x": 0, "y": 0, "opacity": 1, "visible": visible, "data": data}
        wall_gid = A.gid('wall_side')
        m = {"compressionlevel": -1, "width": self.w, "height": self.h, "tilewidth": T, "tileheight": T, "infinite": False,
             "orientation": "orthogonal", "renderorder": "right-down", "type": "map", "version": "1.10", "tiledversion": "1.10.2",
             "nextlayerid": 7, "nextobjectid": self.nid,
             "layers": [lyr(1, "Ground", self.ground), lyr(2, "Walls", self.walls), lyr(3, "Furniture", self.furn), lyr(4, "Decor", self.decor),
                        lyr(5, "Collision", [wall_gid if c else 0 for c in self.coll], visible=False),
                        {"id": 6, "name": "Objects", "type": "objectgroup", "draworder": "topdown", "opacity": 1, "visible": True, "x": 0, "y": 0, "objects": self.objects}],
             "tilesets": [{"firstgid": 1, "columns": cols, "image": "../tilesets/mansion.png", "imagewidth": iw, "imageheight": ih,
                           "margin": 0, "spacing": 0, "name": "mansion", "tilecount": count, "tilewidth": T, "tileheight": T}]}
        json.dump(m, open(path, 'w'), indent=1)

def hall():
    r = Room(24, 18, 'floor_marble')
    for x in (2, 8, 15): r.place('window', x, 0, 'walls', False)
    r.door_top(5, 'family_wing', 'Family bedrooms (upstairs)'); r.door_top(18, 'blue_room', 'The Blue Room (upstairs)')
    r.door_top(20, 'attic', 'Narrow attic stair', locked_hint="The attic door is locked. Somebody in this house must know where the key is.")
    r.place('painting_land', 1, 0, 'walls', False); r.place('painting_portrait', 10, 0, 'walls', False); r.place('trophy', 11, 0, 'walls', False)
    r.place('wall_clock', 12, 0, 'walls', False); r.place('painting_land', 13, 0, 'walls', False); r.place('painting_portrait', 22, 0, 'walls', False)
    r.door_side(0, 10, 'library', 'The library'); r.door_side(23, 10, 'kitchen', 'The kitchen')
    r.place('staircase', 10, 2)
    r.place('console_table', 7, 2); r.place('console_table', 15, 2); r.place('clock', 1, 2); r.place('plant_big', 22, 2)
    r.place('rug_big', 9, 9, 'ground'); r.place('rug_med', 15, 9, 'ground'); r.place('rug_small', 2, 12, 'ground')
    r.place('table_cake', 3, 7)
    for (x, y) in ((3, 6), (6, 6), (3, 9), (6, 9)): r.place('chair', x, y)
    r.obj('Birthday table', 'clue', 2, 6, 6, 4, clue_id='c_dinner')
    r.place('sofa3', 16, 7); r.place('coffee_table', 16, 10); r.place('armchair', 20, 9); r.place('floor_lamp', 19, 7)
    r.place('coat_stand', 22, 13); r.place('plant_big', 1, 15); r.place('plant_big', 22, 15); r.place('plant_small', 14, 15)
    r.npc('Advocate Raghav Deshpande', 13, 6, 'raghav', suspect_id='s_raghav')
    r.npc('Meera Bansal', 19, 10, 'meera', suspect_id='s_meera')
    r.point('start', 'spawn', 12.5 * T, 15.5 * T)
    return r

def library():
    r = Room(20, 15, 'floor_dark')
    for x in (4, 14): r.place('window', x, 0, 'walls', False)
    r.place('painting_portrait', 9, 0, 'walls', False); r.place('wall_clock', 1, 0, 'walls', False); r.place('painting_land', 18, 0, 'walls', False)
    for x in (2, 4, 6, 11, 13, 15): r.place('bookshelf', x, 2)
    r.place('fireplace', 8, 2); r.place('floor_lamp', 1, 2); r.place('plant_big', 17, 2)
    r.obj('Third bookcase', 'door', 6, 2, 2, 2, to='passage', hidden_until='c_blueprint')
    r.place('rug_big', 7, 6, 'ground'); r.place('armchair', 9, 6); r.place('side_table', 11, 7); r.place('armchair_green', 13, 8)
    r.place('sofa2', 2, 8); r.place('desk_big', 14, 11); r.place('chair', 15, 13); r.place('plant_small', 1, 12); r.place('floor_lamp', 5, 12)
    r.obj('The armchair', 'clue', 9, 6, 2, 2, clue_id='c_scene')
    r.obj('Papers on the side table', 'clue', 11, 7, 1, 1, clue_id='c_mill_report')
    r.obj("Dayanand's desk", 'clue', 14, 11, 3, 2, clue_id='c_cheque')
    r.door_side(19, 9, 'hall', 'Door to the hall')
    r.npc('Dr. Kavita Reddy', 8, 10, 'reddy', suspect_id='s_reddy')
    r.npc('Rohit Rastogi', 12, 10, 'rohit', suspect_id='s_rohit')
    return r

def kitchen():
    r = Room(18, 13, 'floor_kitchen')
    r.place('chimney', 3, 0, 'walls', False); r.place('wall_shelf', 6, 0, 'walls', False); r.place('wall_shelf', 9, 0, 'walls', False); r.place('window', 14, 0, 'walls', False)
    r.place('stove', 3, 2); r.place('cylinder', 5, 3); r.place('counter_sink', 6, 2); r.place('counter', 9, 2); r.place('fridge', 11, 2)
    r.place('matka', 14, 3); r.place('plant_small', 15, 3)
    r.place('rug_med', 6, 6, 'ground'); r.place('table_kitchen', 7, 7)
    for (x, y) in ((6, 7), (10, 7), (7, 9), (9, 9)): r.place('stool', x, y)
    r.place('crates', 14, 9); r.place('sacks', 12, 11); r.place('plant_small', 1, 11)
    r.door_side(0, 8, 'hall', 'Door to the hall')
    r.obj('Milk jug on the counter', 'clue', 9, 2, 2, 2, clue_id='c_milk_jug')
    r.npc('Shanta Bai', 4, 8, 'shanta', suspect_id='s_shanta')
    return r

def family_wing():
    r = Room(26, 11, 'floor_wood')
    for x in (4, 12, 20): r.place('door_top', x, 0, 'walls', False)
    r.place('painting_land', 1, 0, 'walls', False); r.place('window', 6, 0, 'walls', False); r.place('painting_portrait', 9, 0, 'walls', False)
    r.place('wall_clock', 10, 0, 'walls', False); r.place('window', 15, 0, 'walls', False); r.place('painting_land', 18, 0, 'walls', False)
    r.place('window', 22, 0, 'walls', False)
    for x in range(1, 25): r.floor('carpet_h', x, 5); r.floor('carpet_h', x, 6)
    r.place('console_table', 8, 2); r.place('console_table', 16, 2); r.place('sofa2', 13, 2)
    r.place('plant_big', 1, 2); r.place('plant_big', 24, 2); r.place('plant_small', 23, 8); r.place('floor_lamp', 2, 8)
    r.obj("Kabir's phone on the console table", 'clue', 16, 2, 2, 1, clue_id='c_betting')
    r.door_side(12, 10, 'hall', 'Stairs down to the hall')
    r.npc('Kabir Rastogi', 11, 7, 'kabir', suspect_id='s_kabir')
    return r

def blue_room():
    r = Room(18, 13, 'floor_wood')
    for x in (6, 11): r.place('window', x, 0, 'walls', False)
    r.place('painting_land', 9, 0, 'walls', False); r.place('painting_portrait', 14, 0, 'walls', False)
    r.place('bed', 2, 2); r.place('nightstand', 1, 2); r.place('nightstand', 4, 2)
    r.obj('Phone on the bedside table', 'clue', 4, 2, clue_id='c_wife_text')
    r.place('dresser', 8, 2); r.place('wardrobe', 14, 2)
    r.obj('Writing set on the dresser', 'clue', 8, 2, 2, 2, clue_id='c_pen')
    r.obj('Wardrobe', 'door', 14, 2, 2, 2, to='passage', hidden_until='c_blueprint')
    r.place('rug_med', 6, 5, 'ground'); r.place('armchair', 14, 7); r.place('side_table', 13, 8); r.place('sofa2', 2, 8)
    r.place('suitcase', 10, 9); r.place('plant_big', 1, 10); r.place('floor_lamp', 16, 5)
    r.door_side(9, 12, 'hall', 'Door to the landing')
    return r

def attic():
    r = Room(18, 13, 'floor_dark', 'brick')
    r.place('window_brick', 7, 0, 'walls', False)
    r.place('sheet_chair', 2, 3); r.place('old_mirror', 4, 2); r.place('sheet_chair', 9, 8)
    r.place('trunk', 6, 5); r.obj('Old trunk', 'clue', 6, 5, 2, 1, clue_id='c_blueprint')
    r.place('trunk', 2, 8); r.place('trunk', 11, 3)
    r.place('crates', 13, 2); r.place('crates', 13, 8); r.place('box', 9, 4); r.place('box', 10, 4); r.place('box', 15, 6)
    r.place('nameplate', 11, 9); r.obj('Cracked nameplate', 'clue', 11, 9, clue_id='c_nameplate')
    r.place('rug_small', 5, 7, 'ground'); r.place('floor_lamp', 8, 5)
    r.place('cobweb', 1, 2, 'decor', False); r.place('cobweb_r', 16, 2, 'decor', False)
    r.door_side(9, 12, 'hall', 'Stairs down to the hall')
    return r

def passage():
    r = Room(26, 10, 'floor_stone', 'brick')
    r.fill_all_walls('brick')
    r.carve(1, 3, 16, 5, 'floor_stone'); r.carve(17, 3, 17, 5, 'floor_stone'); r.carve(18, 1, 24, 7, 'floor_dark')
    r.door_side(0, 4, 'library', 'Back to the library (bookcase)'); r.door_side(10, 2, 'blue_room', 'Back to the Blue Room (wardrobe)')
    for x in (5, 14): r.place('torch', x, 2, 'walls', False); r.place('glow', x, 3, 'decor', False)
    r.place('cobweb', 1, 3, 'decor', False); r.place('cobweb_r', 16, 3, 'decor', False)
    r.obj('Glint in the dust', 'clue', 7, 4, clue_id='c_cufflink')
    r.obj('Dial lock', 'dial', 17, 3, 1, 3, clue_id='c_strongroom_open', code='1921', tile=str(A.gid('iron_gate')))
    r.place('document_shelf', 19, 1); r.place('tijori', 23, 3); r.place('desk_old', 20, 5); r.place('floor_lamp', 23, 6)
    r.place('rug_small', 19, 3, 'ground')
    r.obj('Document shelf', 'clue', 19, 1, 3, 2, clue_id='c_new_will')
    r.obj("Dayanand's desk", 'clue', 20, 5, 2, 2, clue_id='c_diary')
    r.obj('Steel tijori', 'clue', 23, 3, 2, 2, clue_id='c_forged_fd')
    return r

# ------------------------------------------------------------------ characters (64x96 cells, drawn 4x and smoothed)
from PIL import ImageFilter, ImageChops
K = 4
CW, CH = 64, 96

class Cv:
    def __init__(self): self.img = Image.new('RGBA', (CW * K, CH * K), (0, 0, 0, 0)); self.d = ImageDraw.Draw(self.img)
    def poly(self, pts, c): self.d.polygon([(x * K, y * K) for x, y in pts], fill=rgba(c))
    def ell(self, cx, cy, rx, ry, c): self.d.ellipse([(cx - rx) * K, (cy - ry) * K, (cx + rx) * K, (cy + ry) * K], fill=rgba(c))
    def ring(self, cx, cy, rx, ry, c, w=0.7): self.d.ellipse([(cx - rx) * K, (cy - ry) * K, (cx + rx) * K, (cy + ry) * K], outline=rgba(c), width=int(w * K))
    def rect(self, x0, y0, x1, y1, c): self.d.rectangle([x0 * K, y0 * K, x1 * K - 1, y1 * K - 1], fill=rgba(c))
    def line(self, p0, p1, w, c): self.d.line([(p0[0] * K, p0[1] * K), (p1[0] * K, p1[1] * K)], fill=rgba(c), width=max(1, int(w * K)))
    def limb(self, p0, p1, w0, w1, c):
        dx, dy = p1[0] - p0[0], p1[1] - p0[1]; L = math.hypot(dx, dy) or 1; nx, ny = -dy / L, dx / L
        self.poly([(p0[0] + nx * w0 / 2, p0[1] + ny * w0 / 2), (p1[0] + nx * w1 / 2, p1[1] + ny * w1 / 2),
                   (p1[0] - nx * w1 / 2, p1[1] - ny * w1 / 2), (p0[0] - nx * w0 / 2, p0[1] - ny * w0 / 2)], c)
        self.ell(p0[0], p0[1], w0 / 2, w0 / 2, c); self.ell(p1[0], p1[1], w1 / 2, w1 / 2, c)
    def finish(self):
        a = self.img.getchannel('A'); dil = a.filter(ImageFilter.MaxFilter(2 * K + 1)); ring = ImageChops.subtract(dil, a)
        base = Image.new('RGBA', self.img.size, (0, 0, 0, 0)); base.paste(Image.new('RGBA', self.img.size, (34, 24, 26, 255)), (0, 0), ring)
        base.alpha_composite(self.img)
        out = Image.new('RGBA', (CW * K, CH * K), (0, 0, 0, 0)); sd = ImageDraw.Draw(out)
        sd.ellipse([14 * K, 87 * K, 50 * K, 95 * K], fill=(0, 0, 0, 80)); out.alpha_composite(base)
        return out.resize((CW, CH), Image.LANCZOS)

SHOE = (36, 28, 28)

def face_front(cv, st, bob):
    skin, hair = st['skin'], st['hair']; sk2 = sh(skin, -26); female = st.get('female', False)
    for sx in (-1, 1):                                                      # eyes
        ex = 32 + sx * 3.9
        cv.ell(ex, 16.4 + bob, 2.3, 1.6, (252, 252, 252)); cv.ell(ex, 16.4 + bob, 1.25, 1.35, (50, 34, 26)); cv.ell(ex + 0.35, 16.0 + bob, 0.4, 0.4, (255, 255, 255))
        cv.line((32 + sx * 6.4, 13.9 + bob), (32 + sx * 2.0, 13.4 + bob - (0.4 if female else 0)), 1.0 if not female else 0.8, hair if hair[0] > 90 and not st.get('grey') else sh(hair, -20))
        if female: cv.line((ex - 2.2, 16.0 + bob), (ex + 2.2, 15.6 + bob), 0.5, (40, 28, 24))   # lashes
    cv.line((32, 16.8 + bob), (32, 20.0 + bob), 0.8, sk2); cv.ell(32, 20.4 + bob, 1.7, 0.9, sk2)    # nose
    cv.ell(30.8, 20.5 + bob, 0.35, 0.3, (80, 50, 40)); cv.ell(33.2, 20.5 + bob, 0.35, 0.3, (80, 50, 40))
    lip = (198, 72, 90) if female else sh(skin, -38)
    cv.ell(32, 23.4 + bob, 2.7, 1.2, lip); cv.line((29.5, 23.2 + bob), (34.5, 23.2 + bob), 0.45, sh(lip, -50))
    if female:
        cv.ell(26.2, 20.0 + bob, 1.6, 1.0, sh(skin, 8)); cv.ell(37.8, 20.0 + bob, 1.6, 1.0, sh(skin, 8))
    if st.get('moustache'):
        cv.poly([(27.6, 21.6 + bob), (32, 20.8 + bob), (36.4, 21.6 + bob), (34.6, 22.9 + bob), (32, 22.1 + bob), (29.4, 22.9 + bob)], st['moustache'])
    if st.get('stubble'): cv.poly([(24.5, 21 + bob), (28, 25.5 + bob), (32, 27 + bob), (36, 25.5 + bob), (39.5, 21 + bob), (36, 23.8 + bob), (32, 25 + bob), (28, 23.8 + bob)], sh(skin, -44))
    if st.get('glasses'):
        cv.ring(28.1, 16.4 + bob, 3.3, 2.6, (40, 34, 36), 0.8); cv.ring(35.9, 16.4 + bob, 3.3, 2.6, (40, 34, 36), 0.8); cv.line((31.4, 16.0 + bob), (32.6, 16.0 + bob), 0.7, (40, 34, 36))
    if st.get('bindi'): cv.ell(32, 11.4 + bob, 0.8, 0.8, (200, 30, 40))

def head_front(cv, st, bob, back=False):
    skin, hair, hs = st['skin'], st['hair'], st['hair_style']
    if back and hs in ('long', 'braid'):                       # seen from behind, the long hair hangs over the back
        cv.poly([(21.5, 12 + bob), (20.4, 30 + bob), (21, 52 + bob), (27, 56 + bob), (37, 56 + bob), (43, 52 + bob), (43.6, 30 + bob), (42.5, 12 + bob)], hair)
    cv.rect(28.2, 24 + bob, 35.8, 31 + bob, sh(skin, -14))                              # neck
    cv.ell(22.9, 17 + bob, 1.7, 2.6, skin); cv.ell(41.1, 17 + bob, 1.7, 2.6, skin)       # ears
    if back:
        cv.ell(32, 14 + bob, 9.6, 10.6, hair)
        if hs == 'bun': cv.ell(32, 4.2 + bob, 5, 4.4, hair)
        if hs == 'braid': cv.limb((32, 22 + bob), (31, 50 + bob), 4.4, 2.6, hair)
    else:
        cv.ell(32, 12.6 + bob, 10.2, 9.6, hair)
        cv.ell(32, 17.0 + bob, 8.6, 9.6, skin)
        cv.poly([(23.4, 14 + bob), (24.4, 9.2 + bob), (32, 6.6 + bob), (39.6, 9.2 + bob), (40.6, 14 + bob), (37.5, 10.8 + bob), (32, 9.6 + bob), (26.5, 10.8 + bob)], hair)
        if hs == 'long': cv.line((32, 7.2 + bob), (28, 11 + bob), 1.2, sh(hair, 14))
        face_front(cv, st, bob)
        if hs == 'bun': cv.ell(32, 4.0 + bob, 5, 4.4, hair)
        if hs == 'braid': cv.limb((38.5, 22 + bob), (38, 48 + bob), 4.2, 2.6, hair)
        if hs == 'quiff': cv.poly([(24, 10 + bob), (28, 4.2 + bob), (36, 3.4 + bob), (41, 8 + bob), (36, 7.6 + bob), (29, 8 + bob)], sh(hair, 6))
        if st.get('earrings'): cv.ell(22.4, 20.6 + bob, 0.9, 1.2, GOLD); cv.ell(41.6, 20.6 + bob, 0.9, 1.2, GOLD)
    hat = st.get('hat')
    if hat == 'fedora':
        cv.ell(32, 8.8 + bob, 13.6, 3.6, (112, 78, 46)); cv.poly([(23.5, 9 + bob), (25, 2.5 + bob), (39, 2.5 + bob), (40.5, 9 + bob)], (128, 90, 54)); cv.rect(23.6, 6.4 + bob, 40.4, 8.6 + bob, (38, 30, 28))
    elif hat == 'cap':
        cv.ell(32, 8 + bob, 10.6, 6.4, st['capcol']); cv.poly([(22, 8.6 + bob), (42, 8.6 + bob), (45.4, 11.4 + bob), (18.6, 11.4 + bob)], sh(st['capcol'], -28))
        if not back: cv.ell(32, 7.4 + bob, 1.7, 1.7, GOLD)
    elif hat == 'nurse':
        cv.poly([(24, 8.4 + bob), (26, 1.8 + bob), (38, 1.8 + bob), (40, 8.4 + bob)], (252, 252, 254))
        if not back: cv.rect(31, 3.2 + bob, 33, 7 + bob, (204, 40, 52)); cv.rect(29.6, 4.6 + bob, 34.4, 5.8 + bob, (204, 40, 52))

def body_front(cv, st, step, bob, back=False):
    skin, top, kind = st['skin'], st['top'], st['kind']; bottom = st.get('bottom', (40, 40, 52))
    swing = step * 2.2
    ankL = (27.6, 86 - (2.6 if step > 0 else 0)); ankR = (36.4, 86 - (2.6 if step < 0 else 0))
    cv.limb((28.4, 54 + bob), ankL, 9.4, 7.2, bottom); cv.limb((35.6, 54 + bob), ankR, 9.4, 7.2, bottom)
    sc = st.get('shoecol', SHOE)
    cv.ell(ankL[0], ankL[1] + 2.4, 5.2, 2.8, sc); cv.ell(ankR[0], ankR[1] + 2.4, 5.2, 2.8, sc)
    hem = {'blazer': 62, 'denim': 60, 'uniform': 57, 'kurta': 70, 'nurse': 68, 'saree': 44}[kind]
    flare = {'kurta': 3, 'nurse': 2, 'saree': 0}.get(kind, 0)
    if kind == 'saree':
        cv.poly([(22.6, 42 + bob), (41.4, 42 + bob), (46, 84), (18, 84)], top)                  # skirt of the saree
        for x in (25, 29, 33, 37): cv.line((x, 46 + bob), (x - 1 + (x - 31) * 0.25, 83), 0.5, sh(top, -34))
        cv.rect(18, 81, 46, 85, GOLD); cv.rect(18, 79.5, 46, 80.6, sh(GOLD, -40))
        cv.ell(ankL[0], ankL[1] + 2.4, 5.2, 2.8, sc); cv.ell(ankR[0], ankR[1] + 2.4, 5.2, 2.8, sc)
    torso_col = st.get('blouse', top) if kind == 'saree' else top
    cv.poly([(19.5, 30.5 + bob), (44.5, 30.5 + bob), (43 + flare, hem + bob), (21 - flare, hem + bob)], torso_col)
    cv.ell(20.6, 32.4 + bob, 4.2, 4.0, torso_col); cv.ell(43.4, 32.4 + bob, 4.2, 4.0, torso_col)
    if kind == 'kurta':
        cv.line((19.5, hem - 1 + bob), (21, hem - 1 + bob), 0.1, top)
        cv.rect(19, hem - 2.4 + bob, 45, hem + bob, st['trim']); cv.line((32, 30.5 + bob), (32, hem - 3 + bob), 0.5, sh(top, -30))
    if not back:
        if kind in ('blazer', 'denim'):
            inner = (250, 250, 252)
            cv.poly([(27.6, 30.5 + bob), (36.4, 30.5 + bob), (33.2, 52 + bob), (30.8, 52 + bob)], inner)
            cv.poly([(27.6, 30.5 + bob), (31.4, 30.5 + bob), (29.2, 35 + bob)], (232, 232, 236)); cv.poly([(36.4, 30.5 + bob), (32.6, 30.5 + bob), (34.8, 35 + bob)], (232, 232, 236))
            if st.get('tie'): cv.poly([(31.1, 33.5 + bob), (32.9, 33.5 + bob), (33.7, 49 + bob), (32, 51 + bob), (30.3, 49 + bob)], st['tie'])
            if st.get('bands'):
                cv.rect(30.2, 31.5 + bob, 31.6, 38.5 + bob, (250, 250, 252)); cv.rect(32.4, 31.5 + bob, 33.8, 38.5 + bob, (250, 250, 252))
            cv.line((28, 30.5 + bob), (30.6, 52 + bob), 1.1, sh(top, -34)); cv.line((36, 30.5 + bob), (33.4, 52 + bob), 1.1, sh(top, -34))
            for yy in (55,): cv.ell(32, yy + bob, 0.9, 0.9, sh(top, -40))
            if kind == 'denim':
                cv.rect(27.6, 31 + bob, 36.4, 56 + bob, (250, 250, 252)); cv.poly([(27.6, 31 + bob), (36.4, 31 + bob), (32, 36 + bob)], (230, 230, 236))
                cv.line((28, 30.5 + bob), (27.6, 58 + bob), 1.2, sh(top, -30)); cv.line((36, 30.5 + bob), (36.4, 58 + bob), 1.2, sh(top, -30))
        if kind == 'uniform':
            cv.poly([(27, 30.5 + bob), (37, 30.5 + bob), (32, 37 + bob)], sh(top, 14)); cv.line((32, 36 + bob), (32, 54 + bob), 0.5, sh(top, -34))
            for (x0, x1) in ((23.4, 29.2), (34.8, 40.6)): cv.rect(x0, 38 + bob, x1, 44 + bob, sh(top, -18)); cv.rect(x0, 38 + bob, x1, 39 + bob, sh(top, -40))
            cv.ell(26.6, 36.4 + bob, 1.5, 1.5, GOLD)
            cv.rect(21, 53.4 + bob, 43, 57 + bob, (86, 56, 36)); cv.rect(30.4, 53.2 + bob, 33.6, 57.2 + bob, GOLD)
        if kind == 'nurse':
            cv.line((27.6, 30.5 + bob), (29.4, 46 + bob), 0.9, (70, 74, 84)); cv.line((36.4, 30.5 + bob), (34.6, 46 + bob), 0.9, (70, 74, 84)); cv.ell(32, 47 + bob, 2.2, 2.2, STEEL)
            cv.line((32, 30.5 + bob), (32, hem + bob), 0.5, sh(top, -22))
        if kind == 'kurta':
            cv.poly([(27, 30.5 + bob), (37, 30.5 + bob), (32, 38 + bob)], sh(top, -34)); cv.line((27, 30.5 + bob), (32, 38 + bob), 1.0, st['trim']); cv.line((37, 30.5 + bob), (32, 38 + bob), 1.0, st['trim'])
            for yy in (42, 48, 54): cv.ell(32, yy + bob, 0.8, 0.8, st['trim'])
        if kind == 'saree':
            cv.poly([(27.6, 30.5 + bob), (36.4, 30.5 + bob), (32, 36 + bob)], sh(skin, -6))
            cv.poly([(20, 31.5 + bob), (28, 30.5 + bob), (45, 58 + bob), (36, 62 + bob)], st['top']); cv.line((20, 31.5 + bob), (36, 62 + bob), 1.2, GOLD); cv.line((28, 30.5 + bob), (45, 58 + bob), 1.2, GOLD)
        if st.get('dupatta'):
            dc = st['dupatta']
            cv.poly([(18.6, 31 + bob), (45.4, 31 + bob), (45.4, 36.6 + bob), (32, 44 + bob), (18.6, 36.6 + bob)], dc); cv.poly([(17, 33 + bob), (24, 37 + bob), (22.6, 64 + bob), (15.6, 61 + bob)], dc)
            cv.line((18.6, 36.6 + bob), (32, 44 + bob), 0.8, sh(dc, -34)); cv.line((32, 44 + bob), (45.4, 36.6 + bob), 0.8, sh(dc, -34))
    else:
        if st.get('dupatta'): cv.poly([(18.6, 31 + bob), (45.4, 31 + bob), (45.4, 62 + bob), (18.6, 62 + bob)], st['dupatta'])
        if kind == 'saree': cv.poly([(19, 31 + bob), (45, 31 + bob), (46, 66 + bob), (20, 66 + bob)], st['top']); cv.rect(19.4, 64 + bob, 45.6, 66.4 + bob, GOLD)
    # arms
    sl = st.get('sleeve', 'long'); armcol = top if kind != 'saree' else st.get('blouse', top)
    for sx, sw in ((-1, 1), (1, -1)):
        shp = (32 + sx * 12.6, 33.6 + bob); elb = (32 + sx * 15.6, 45 + bob + sx * swing * 0.35); wr = (32 + sx * 14.8, 57 + bob + sx * swing)
        if sl == 'long' or kind in ('blazer', 'denim', 'uniform'):
            cv.limb(shp, elb, 6.8, 5.9, armcol); cv.limb(elb, wr, 5.9, 5.1, armcol)
            if kind in ('blazer', 'denim'): cv.rect(wr[0] - 2.6, wr[1] - 3, wr[0] + 2.6, wr[1] - 1.4, (250, 250, 252))
        elif sl == 'short':
            cv.limb(shp, (shp[0] + sx * 1.6, shp[1] + 8), 6.8, 6.2, armcol); cv.limb((shp[0] + sx * 1.6, shp[1] + 8), elb, 5.2, 5.0, skin); cv.limb(elb, wr, 5.0, 4.6, skin)
        else:  # three-quarter
            cv.limb(shp, elb, 6.8, 6.0, armcol); cv.limb(elb, (elb[0] + sx * 0.4, elb[1] + 6.5), 6.0, 5.4, armcol); cv.limb((elb[0] + sx * 0.4, elb[1] + 6.5), wr, 4.6, 4.2, skin)
            if st.get('bangles'): cv.ell(wr[0], wr[1] - 2, 2.6, 0.9, GOLD)
        cv.ell(wr[0], wr[1] + 2.4, 3.1, 3.5, skin); cv.ell(wr[0] - sx * 1.8, wr[1] + 1.2, 1.1, 1.9, skin)

def front(st, frame, back=False):
    cv = Cv(); step = {0: 0, 1: 1, 2: 0, 3: -1}[frame]; bob = -1.2 if frame in (1, 3) else 0.0
    if st['hair_style'] in ('long', 'braid') and not back:    # seen from the front, long hair is BEHIND the body
        cv.poly([(21.5, 12 + bob), (20.4, 30 + bob), (21, 52 + bob), (27, 56 + bob), (37, 56 + bob), (43, 52 + bob), (43.6, 30 + bob), (42.5, 12 + bob)], st['hair'])
    body_front(cv, st, step, bob, back)
    head_front(cv, st, bob, back)
    if st['hair_style'] == 'long' and not back:                                           # long hair falls in front of the shoulders
        for sx in (-1, 1): cv.poly([(32 + sx * 10.2, 20 + bob), (32 + sx * 12.4, 34 + bob), (32 + sx * 11.6, 50 + bob), (32 + sx * 8.2, 48 + bob), (32 + sx * 8.4, 30 + bob)], st['hair'])
    return cv.finish()

def side(st, frame):
    cv = Cv(); step = {0: 0, 1: 1, 2: 0, 3: -1}[frame]; bob = -1.2 if frame in (1, 3) else 0.0
    skin, hair, top, kind = st['skin'], st['hair'], st['top'], st['kind']; bottom = st.get('bottom', (40, 40, 52)); sc = st.get('shoecol', SHOE)
    hs = st['hair_style']
    if hs in ('long', 'braid'): cv.poly([(22, 12 + bob), (21, 30 + bob), (22, 52 + bob), (30, 56 + bob), (31, 30 + bob), (30, 12 + bob)], hair)
    swing = step * 7.0
    backL = (32 - swing, 87.5); frontL = (32 + swing, 87.5)
    cv.limb((32, 54 + bob), (32 - swing * 0.5 - 1, 70), 8.6, 7.8, sh(bottom, -16)); cv.limb((32 - swing * 0.5 - 1, 70), backL, 7.8, 6.6, sh(bottom, -16))
    cv.ell(backL[0] + 2.4, backL[1] + 1.6, 5.4, 2.6, sh(sc, -10))
    cv.limb((32, 54 + bob), (32 + swing * 0.5 + 1, 70), 9.0, 8.0, bottom); cv.limb((32 + swing * 0.5 + 1, 70), frontL, 8.0, 6.8, bottom)
    cv.ell(frontL[0] + 2.6, frontL[1] + 1.6, 5.6, 2.7, sc)
    hem = {'blazer': 62, 'denim': 60, 'uniform': 57, 'kurta': 70, 'nurse': 68, 'saree': 44}[kind]
    if kind == 'saree':
        cv.poly([(24, 42 + bob), (40, 42 + bob), (43, 84), (21, 84)], top); cv.rect(21, 81, 43, 85, GOLD)
        cv.ell(frontL[0] + 2.6, frontL[1] + 1.6, 5.6, 2.7, sc)
    armcol = top if kind != 'saree' else st.get('blouse', top)
    cv.limb((32, 33 + bob), (32 + swing * 0.1 + 2 * step, 46 + bob), 6.2, 5.6, sh(armcol, -22))                    # far arm
    cv.poly([(24.4, 30.5 + bob), (39.6, 30.5 + bob), (40 + (2 if kind in ('kurta', 'nurse') else 0), hem + bob), (24, hem + bob)], armcol if kind == 'saree' else top)
    cv.ell(32, 32.4 + bob, 7.6, 4.2, armcol if kind == 'saree' else top)
    if kind == 'kurta': cv.rect(24, hem - 2.4 + bob, 41.6, hem + bob, st['trim'])
    if kind in ('blazer', 'denim'):
        cv.poly([(36, 30.5 + bob), (39.4, 31 + bob), (38, 50 + bob), (36, 50 + bob)], (250, 250, 252))
        if st.get('tie'): cv.line((37, 34 + bob), (37.6, 49 + bob), 1.8, st['tie'])
        cv.line((36.2, 30.5 + bob), (37.6, 52 + bob), 1.0, sh(top, -34))
    if kind == 'uniform': cv.rect(24, 53.4 + bob, 40.4, 57 + bob, (86, 56, 36)); cv.rect(34.5, 38 + bob, 38.5, 43 + bob, sh(top, -18))
    if kind == 'saree': cv.poly([(26, 31.5 + bob), (34, 30.5 + bob), (40, 56 + bob), (31, 60 + bob)], st['top']); cv.line((34, 30.5 + bob), (40, 56 + bob), 1.1, GOLD)
    if st.get('dupatta'): cv.poly([(25, 31 + bob), (39, 31 + bob), (38, 40 + bob), (27, 62 + bob), (22, 58 + bob)], st['dupatta'])
    near = (32 - swing * 0.1 - 2 * step, 56 + bob - abs(step))
    sl = st.get('sleeve', 'long')
    if sl == 'long' or kind in ('blazer', 'denim', 'uniform'):
        cv.limb((32, 33.6 + bob), (32 - swing * 0.18, 46 + bob), 6.8, 5.9, armcol); cv.limb((32 - swing * 0.18, 46 + bob), near, 5.9, 5.1, armcol)
    elif sl == 'short':
        cv.limb((32, 33.6 + bob), (32 - swing * 0.1, 41 + bob), 6.8, 6.0, armcol); cv.limb((32 - swing * 0.1, 41 + bob), (32 - swing * 0.18, 46 + bob), 5.2, 5.0, skin); cv.limb((32 - swing * 0.18, 46 + bob), near, 5.0, 4.6, skin)
    else:
        cv.limb((32, 33.6 + bob), (32 - swing * 0.18, 46 + bob), 6.8, 6.0, armcol); cv.limb((32 - swing * 0.18, 46 + bob), (32 - swing * 0.25, 52 + bob), 6.0, 5.4, armcol); cv.limb((32 - swing * 0.25, 52 + bob), near, 4.6, 4.2, skin)
    cv.ell(near[0], near[1] + 2.4, 3.1, 3.5, skin)
    cv.rect(29, 24 + bob, 35, 31 + bob, sh(skin, -14))                                                    # neck
    cv.ell(31, 12.4 + bob, 9.8, 9.4, hair)                                                                 # hair
    cv.ell(32.4, 16.6 + bob, 7.8, 9.4, skin)                                                               # head
    cv.poly([(38.6, 15 + bob), (42.2, 19.4 + bob), (38.6, 20.4 + bob)], skin); cv.line((38.6, 15.6 + bob), (41.6, 19.2 + bob), 0.5, sh(skin, -22))   # nose
    cv.ell(35.8, 15.4 + bob, 2.0, 1.5, (252, 252, 252)); cv.ell(36.5, 15.4 + bob, 1.1, 1.3, (50, 34, 26))                                          # eye
    female = st.get('female', False)
    cv.line((33.6, 13.0 + bob), (38, 12.6 + bob), 1.0 if not female else 0.8, hair if not st.get('grey') else (120, 120, 126))
    cv.ell(37.6, 23.4 + bob, 2.0, 1.1, (198, 72, 90) if female else sh(skin, -38))
    if st.get('moustache'): cv.poly([(36, 21.4 + bob), (40, 21.4 + bob), (39.6, 22.8 + bob), (36, 22.4 + bob)], st['moustache'])
    if st.get('glasses'): cv.ring(36, 15.6 + bob, 3.2, 2.6, (40, 34, 36), 0.8); cv.line((32.8, 15.6 + bob), (29, 15 + bob), 0.7, (40, 34, 36))
    cv.ell(29.4, 17 + bob, 1.8, 2.7, skin); cv.ell(30.2, 12 + bob, 8.2, 7.4, hair)                         # ear + hair over ear
    cv.poly([(26, 17 + bob), (28, 24 + bob), (24, 20 + bob)], hair)
    cv.poly([(38.6, 12.6 + bob), (36, 8.6 + bob), (30, 6.8 + bob), (26, 9 + bob), (32, 9.6 + bob)], hair)
    if st.get('bindi'): cv.ell(39.2, 11.6 + bob, 0.7, 0.7, (200, 30, 40))
    if st.get('earrings'): cv.ell(29.4, 20.2 + bob, 0.9, 1.2, GOLD)
    if hs == 'bun': cv.ell(27, 4.6 + bob, 5, 4.4, hair)
    if hs == 'braid': cv.limb((28, 22 + bob), (27, 50 + bob), 4.4, 2.6, hair)
    if hs == 'long': cv.poly([(24, 14 + bob), (23, 34 + bob), (24, 50 + bob), (28, 48 + bob), (28, 24 + bob)], hair)
    if hs == 'quiff': cv.poly([(28, 6 + bob), (34, 3.6 + bob), (40, 7 + bob), (35, 7.6 + bob)], sh(hair, 6))
    hat = st.get('hat')
    if hat == 'fedora': cv.ell(32, 8.6 + bob, 13, 3.4, (112, 78, 46)); cv.poly([(24, 8.8 + bob), (26, 2.5 + bob), (39, 2.5 + bob), (40, 8.8 + bob)], (128, 90, 54)); cv.rect(24, 6.4 + bob, 40, 8.4 + bob, (38, 30, 28))
    if hat == 'cap': cv.ell(31, 7.8 + bob, 10.4, 6.2, st['capcol']); cv.poly([(34, 8.6 + bob), (46, 10 + bob), (46, 11.6 + bob), (34, 11 + bob)], sh(st['capcol'], -28)); cv.ell(37, 7.6 + bob, 1.6, 1.6, GOLD)
    if hat == 'nurse': cv.poly([(24.6, 8.4 + bob), (26.4, 1.8 + bob), (37.6, 1.8 + bob), (39.4, 8.4 + bob)], (252, 252, 254)); cv.rect(30, 3.4 + bob, 32, 7 + bob, (204, 40, 52)); cv.rect(28.6, 4.8 + bob, 33.4, 6 + bob, (204, 40, 52))
    return cv.finish()

CHARS = [  # order must match kSpriteNames in GameWidget.cpp
    ('detective', dict(skin=(232, 188, 150), hair=(48, 36, 30), hair_style='short', hat='fedora', kind='blazer', top=(46, 58, 98), tie=(150, 40, 50), bottom=(38, 40, 54), moustache=(48, 36, 30))),
    ('raghav', dict(skin=(210, 164, 128), hair=(156, 156, 162), grey=True, hair_style='short', kind='blazer', top=(30, 30, 38), bands=True, bottom=(28, 28, 34), glasses=True, moustache=(150, 150, 156))),
    ('reddy', dict(skin=(228, 184, 144), hair=(30, 22, 22), hair_style='braid', female=True, kind='nurse', top=(244, 244, 248), bottom=(60, 70, 96), sleeve='long', earrings=True, glasses=True)),
    ('shanta', dict(skin=(186, 138, 102), hair=(182, 182, 188), grey=True, hair_style='bun', female=True, kind='saree', top=(160, 40, 68), blouse=(206, 156, 62), sleeve='three', bangles=True, bindi=True, shoecol=(150, 110, 60))),
    ('rohit', dict(skin=(214, 168, 130), hair=(42, 38, 40), hair_style='short', kind='blazer', top=(98, 102, 114), tie=(170, 40, 52), bottom=(56, 58, 68), moustache=(42, 38, 40))),
    ('meera', dict(skin=(226, 180, 142), hair=(28, 22, 22), hair_style='long', female=True, kind='kurta', top=(46, 126, 128), trim=GOLD, bottom=(238, 228, 208), dupatta=(224, 178, 74), sleeve='three', bangles=True, bindi=True, earrings=True, shoecol=(150, 110, 60))),
    ('kabir', dict(skin=(230, 186, 148), hair=(30, 24, 22), hair_style='quiff', kind='denim', top=(64, 98, 154), bottom=(54, 72, 114), stubble=True, shoecol=(240, 240, 244))),
]

def make_characters(path):
    sheet = Image.new('RGBA', (16 * CW, CH * len(CHARS)), (0, 0, 0, 0))
    for row, (name, st) in enumerate(CHARS):
        for frame in range(4):
            cells = {0: front(st, frame), 3: front(st, frame, back=True)}
            right = side(st, frame); cells[2] = right; cells[1] = ImageOps.mirror(right)
            for dr, cell in cells.items(): sheet.paste(cell, ((dr * 4 + frame) * CW, row * CH))
    sheet.save(path)

if __name__ == "__main__":
    os.makedirs("assets/tilesets", exist_ok=True); os.makedirs("assets/maps", exist_ok=True); os.makedirs("assets/sprites", exist_ok=True)
    build_atlas()
    iw, ih, count = A.save("assets/tilesets/mansion.png")
    cols = Atlas.COLS
    for name, fn in (("hall", hall), ("library", library), ("kitchen", kitchen), ("family_wing", family_wing),
                     ("blue_room", blue_room), ("attic", attic), ("passage", passage)):
        fn().save(f"assets/maps/{name}.json", cols, iw, ih, count)
    make_characters("assets/sprites/characters.png")
    print(f"wrote tileset ({count} tiles), characters, and 7 maps")
