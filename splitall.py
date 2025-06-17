import math
import random
import itertools
import numpy as np
import pyclipper
import matplotlib as mpl
import matplotlib.pyplot as plt
import dashing

random.seed(38)

def pattern(frac, degrees):
    a = 30 * frac
    b = 30 - a
    angle = math.radians(degrees)
    dx = math.cos(angle)
    dy = math.sin(angle)
    return dashing.HatchPattern.fromString(f"""
*
{degrees},{-a*dx/2},{-a*dy/2},{10*dy},{-10*dx},{a},{-b}""", 2)

def canon(path):
    """Convert a path to the canonical version of itself

    Paths are guaranteed to be CCW by clipper, so it suffices to place the "min"
    point as the 0th element, however "min" is defined. (The usual Python definition
    of tuple ordering is fine)"""

    path = tuple(tuple(pi) for pi in path)
    i = np.argmin(path)
    res = path[i:] + path[:i]
    return res

def splitall(parts):
    parts = [canon(pi) for pi in parts]
    pc = pyclipper.Pyclipper()

    def clip(p1, p2, op):
        pc.Clear()
        pc.AddPath(p1, pyclipper.PT_SUBJECT, True)
        pc.AddPath(p2, pyclipper.PT_CLIP, True)
        r = pc.Execute(op, pyclipper.PFT_EVENODD, pyclipper.PFT_EVENODD)
        return r

    def split_one(p1, p2):
        yield from clip(p1, p2, pyclipper.CT_INTERSECTION) # p1 & p2
        yield from clip(p1, p2, pyclipper.CT_DIFFERENCE) # p1 - p2

    result = set()
    for pi in parts:
        sub_parts = set((pi,))
        for pj in parts: 
            if pi is pj: continue
            sub_parts = set(canon(p) for ps in sub_parts for p in split_one(ps, pj) if p)
        # TODO sometimes gives two results for same boundary
        result |= sub_parts
    return result

FN = 360
angles = np.linspace(0, np.pi*2, FN, False)
sincos = np.stack( [np.cos(angles), np.sin(angles)]).T

def polycircle(x, y, r):
    return np.round([x,y] + r * sincos).astype(np.int32)

def xcoord(): return random.uniform(0, 1600)
def ycoord(): return random.uniform(0, 1000)
def rad(): return random.uniform(400, 800)

paths_in = [polycircle(xcoord(), ycoord(), rad()) for _ in range(6)]
#p1 = polycircle(0, 0, 800)
#p2 = polycircle(-500, -300, 900)
#p3 = polycircle(-500, 300, 800)

paths = splitall(paths_in)
print("#", len(paths))
for path in paths:
    print(f"polygon{path}")

#paths = [p1, p2, p3]
colors = mpl.colormaps['viridis'](np.linspace(0, 1, len(paths)))
#plt.figure(figsize=(16, 10))
#plt.axis('equal')
for i, p in enumerate(paths):

    boundary = [ [ p[i], p[(i+1) % len(p)] ] for i in range(0, len(p), 1) ]
    pat = pattern(random.uniform(.1, .8), 30)
    res = pat.hatch(boundary, dashing.WindingRule.EvenOdd)
    print(len(res))
    if not res: continue
    for p1, p2 in res:
        plt.plot([p1[0], p2[0]], [p1[1], p2[1]], color=colors[i])
    #continue
    x = [pi[0] for pi in p] + [p[0][0]]
    y = [pi[1] for pi in p] + [p[0][1]]
    plt.plot(x, y, color=colors[i])
#, facecolor=colors[i]

plt.axis('off')
plt.axis("image")
plt.show()
