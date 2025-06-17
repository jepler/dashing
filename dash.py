import dashing
from math import cos, sin, radians

pat = dashing.HatchPattern.fromString("""
*
30.,0.,0.,10.,10.,20.,-10.""", 1)

print(pat)

circle_x = [210 + cos(radians(i)) * 200 for i in range(360)] #[::90]
circle_y = [210 + sin(radians(i)) * 200 for i in range(360)] #[::90]
circle_x.append(circle_x[0])
circle_y.append(circle_y[0])
circle_pts = list(zip(circle_x, circle_y))
print(circle_pts)

res = pat.hatch(
    [ [ circle_pts[i], circle_pts[i+1] ] for i in range(0, len(circle_pts)-1, 1) ],
    dashing.WindingRule.EvenOdd)

import matplotlib.pyplot as plt

plt.figure(figsize=(8, 8))
plt.axis('equal')
plt.plot(circle_x, circle_y)
for p1, p2 in res:
    plt.plot([p1[0], p2[0]], [p1[1], p2[1]])
print(res)
plt.show()
