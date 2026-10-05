#!/usr/bin/env python3
"""Génère les modèles 3D (Wavefront OBJ + MTL) du projet.

Usage :  python generate_models.py
Sortie :  axe, giant_spider, mummy, ladder, plank, planks_barricade, lava_golem

Conventions (identiques pour tous les modèles) :
  - 1 unité = 1 mètre, axe Y vers le haut, le modèle regarde vers +Z
  - l'origine est au sol (ou au bas du manche pour la hache)
  - normales fournies, UV fournies, faces triangulées, sens anti-horaire
  - matériaux .mtl : Kd (diffus), Ks/Ns (spéculaire), Ke (émissif : lave, yeux)
"""
import math
import os
import random
from contextlib import contextmanager

# --------------------------------------------------------------------------
#  Petite algèbre vectorielle
# --------------------------------------------------------------------------
I3 = ((1, 0, 0), (0, 1, 0), (0, 0, 1))


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def dist(a, b): return length(sub(a, b))
def lerp(a, b, t): return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))
def flerp(a, b, t): return a + (b - a) * t


def norm(a):
    l = length(a)
    return (0.0, 0.0, 0.0) if l < 1e-12 else mul(a, 1.0 / l)


def mv(R, v): return tuple(R[i][0] * v[0] + R[i][1] * v[1] + R[i][2] * v[2] for i in range(3))
def mm(A, B): return tuple(tuple(sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)) for i in range(3))


def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return ((1, 0, 0), (0, c, -s), (0, s, c))


def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return ((c, 0, s), (0, 1, 0), (-s, 0, c))


def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return ((c, -s, 0), (s, c, 0), (0, 0, 1))


def basis(x, y, z):
    """Matrice dont les colonnes sont x, y, z."""
    return tuple((x[i], y[i], z[i]) for i in range(3))


def basis_y(d):
    """Repère orthonormé direct dont l'axe Y est la direction d."""
    y = norm(d)
    h = (1, 0, 0) if abs(y[0]) < 0.9 else (0, 0, 1)
    x = norm(cross(y, h))
    z = cross(x, y)
    return basis(x, y, z)


def newell(pts):
    n = [0.0, 0.0, 0.0]
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        n[0] += (a[1] - b[1]) * (a[2] + b[2])
        n[1] += (a[2] - b[2]) * (a[0] + b[0])
        n[2] += (a[0] - b[0]) * (a[1] + b[1])
    return tuple(n)


# --------------------------------------------------------------------------
#  Maillage
# --------------------------------------------------------------------------
def mat(kd, ks=(0.05, 0.05, 0.05), ns=10.0, ke=None):
    return {"Kd": kd, "Ks": ks, "Ns": ns, "Ke": ke}


class Mesh:
    def __init__(self, name, materials):
        self.name = name
        self.mats = materials
        self.polys = []
        self.stack = [(I3, (0.0, 0.0, 0.0))]
        self.grp = "body"
        self.uvswap = False          # texture dont le grain suit l'axe Y (montants verticaux)
        self.pivots = {}

    @contextmanager
    def group(self, name, pivot=None):
        """Groupe animable (`g` dans l'OBJ). `pivot` : point d'articulation (écrit en commentaire `# pivot`)."""
        old, self.grp = self.grp, name
        if pivot is not None:
            self.pivots[name] = pivot
        try:
            yield
        finally:
            self.grp = old

    @contextmanager
    def at(self, R=I3, t=(0.0, 0.0, 0.0)):
        """Place les primitives suivantes dans un repère (rotation R, translation t)."""
        Rt, tt = self.stack[-1]
        self.stack.append((mm(Rt, R), add(mv(Rt, t), tt)))
        try:
            yield
        finally:
            self.stack.pop()

    # ---- polygone convexe : le sens des faces est corrigé grâce à `hint` (direction « vers l'extérieur »)
    def poly(self, m, pts, normals=None, uvs=None, hint=None):
        R, t = self.stack[-1]
        items = []
        for i, p in enumerate(pts):
            P = add(mv(R, p), t)
            if items and dist(items[-1][0], P) < 1e-9:
                continue
            items.append([P, norm(mv(R, normals[i])) if normals else None, uvs[i] if uvs else None])
        if len(items) > 1 and dist(items[0][0], items[-1][0]) < 1e-9:
            items.pop()
        if len(items) < 3:
            return
        nw = newell([it[0] for it in items])
        if length(nw) < 1e-14:
            return
        h = mv(R, hint) if hint is not None else None
        if h is None and normals:
            h = (0.0, 0.0, 0.0)
            for it in items:
                h = add(h, it[1])
        if h is not None and dot(nw, h) < 0:
            items.reverse()
            nw = mul(nw, -1)
        fn = norm(nw)
        ax = max(range(3), key=lambda k: abs(fn[k]))
        a, b = [k for k in range(3) if k != ax]
        for it in items:
            if it[1] is None:
                it[1] = fn
            if it[2] is None:
                it[2] = (it[0][b], it[0][a]) if self.uvswap else (it[0][a], it[0][b])
        self.polys.append((m, items, self.grp))

    # ---- primitives -----------------------------------------------------
    def box(self, m, c, size):
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
                 ((0, 1, 0), (0, 0, 1), (1, 0, 0)), ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
                 ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (0, 1, 0), (1, 0, 0))]
        for n, u, v in faces:
            e = (hx, hy, hz)
            cn = add(c, (n[0] * e[0], n[1] * e[1], n[2] * e[2]))
            eu = (u[0] * e[0], u[1] * e[1], u[2] * e[2])
            ev = (v[0] * e[0], v[1] * e[1], v[2] * e[2])
            pts = [sub(sub(cn, eu), ev), sub(add(cn, eu), ev), add(add(cn, eu), ev), add(sub(cn, eu), ev)]
            self.poly(m, pts, uvs=[(0, 0), (1, 0), (1, 1), (0, 1)], hint=n)

    def ellipsoid(self, m, c, r, nu=24, nv=14):
        def pt(i, j):
            th = 2 * math.pi * i / nu
            ph = -math.pi / 2 + math.pi * j / nv
            d = (math.cos(ph) * math.cos(th), math.sin(ph), math.cos(ph) * math.sin(th))
            p = (c[0] + r[0] * d[0], c[1] + r[1] * d[1], c[2] + r[2] * d[2])
            n = (d[0] / r[0], d[1] / r[1], d[2] / r[2])
            return p, n, (i / nu, j / nv)
        for j in range(nv):
            for i in range(nu):
                q = [pt(i, j), pt(i + 1, j), pt(i + 1, j + 1), pt(i, j + 1)]
                self.poly(m, [x[0] for x in q], normals=[x[1] for x in q], uvs=[x[2] for x in q])

    def sphere(self, m, c, r, nu=16, nv=10):
        self.ellipsoid(m, c, (r, r, r), nu, nv)

    def tube(self, m, p0, p1, r0, r1, seg=16, caps=True):
        """Cylindre / cône tronqué de p0 à p1."""
        d = sub(p1, p0)
        L = length(d)
        if L < 1e-9:
            return
        B = basis_y(d)
        ex, ez = (B[0][0], B[1][0], B[2][0]), (B[0][2], B[1][2], B[2][2])
        ey = norm(d)
        slope = (r0 - r1) / L

        def ring(i, p, r):
            th = 2 * math.pi * i / seg
            rad = add(mul(ex, math.cos(th)), mul(ez, math.sin(th)))
            return add(p, mul(rad, r)), norm(add(rad, mul(ey, slope)))
        for i in range(seg):
            a0, n0 = ring(i, p0, r0)
            a1, n1 = ring(i + 1, p0, r0)
            b0, n2 = ring(i, p1, r1)
            b1, n3 = ring(i + 1, p1, r1)
            self.poly(m, [a0, a1, b1, b0], normals=[n0, n1, n3, n2],
                      uvs=[(i / seg, 0), ((i + 1) / seg, 0), ((i + 1) / seg, 1), (i / seg, 1)])
        if caps:
            for p, r, h in ((p0, r0, mul(ey, -1)), (p1, r1, ey)):
                if r > 1e-6:
                    pts = [ring(i, p, r)[0] for i in range(seg)]
                    self.poly(m, pts, hint=h, normals=[h] * seg,
                              uvs=[(0.5 + 0.5 * math.cos(2 * math.pi * i / seg),
                                    0.5 + 0.5 * math.sin(2 * math.pi * i / seg)) for i in range(seg)])

    def lathe(self, m, stations, seg=22, caps=(True, True)):
        """Corps de révolution elliptique : stations = [(y, rx, rz, cx, cz), ...] (cx, cz optionnels)."""
        st = [(s[0], s[1], s[2], s[3] if len(s) > 3 else 0.0, s[4] if len(s) > 4 else 0.0) for s in stations]
        n = len(st)

        def slope(k):
            a, b = st[max(k - 1, 0)], st[min(k + 1, n - 1)]
            return -((b[1] + b[2]) - (a[1] + a[2])) / 2 / max(b[0] - a[0], 1e-9)

        def pt(i, k):
            y, rx, rz, cx, cz = st[k]
            th = 2 * math.pi * i / seg
            c, s = math.cos(th), math.sin(th)
            return ((cx + rx * c, y, cz + rz * s),
                    norm((c / max(rx, 1e-6), slope(k) / max((rx + rz) / 2, 1e-6), s / max(rz, 1e-6))),
                    (i / seg, k / (n - 1)))
        for k in range(n - 1):
            for i in range(seg):
                q = [pt(i, k), pt(i + 1, k), pt(i + 1, k + 1), pt(i, k + 1)]
                self.poly(m, [x[0] for x in q], normals=[x[1] for x in q], uvs=[x[2] for x in q])
        for k, h, ok in ((0, (0, -1, 0), caps[0]), (n - 1, (0, 1, 0), caps[1])):
            if ok:
                self.poly(m, [pt(i, k)[0] for i in range(seg)], hint=h, normals=[h] * seg)

    def band(self, m, c, axis, rx, rz, w, seg=20):
        """Bande de tissu : anneau elliptique ouvert, centré en c, d'axe `axis`."""
        B = basis_y(axis)
        ex, ey, ez = (B[0][0], B[1][0], B[2][0]), (B[0][1], B[1][1], B[2][1]), (B[0][2], B[1][2], B[2][2])

        def pt(i, s):
            th = 2 * math.pi * i / seg
            c0, s0 = math.cos(th), math.sin(th)
            p = add(add(c, mul(ey, s * w / 2)), add(mul(ex, rx * c0), mul(ez, rz * s0)))
            n = norm(add(mul(ex, c0 / rx), mul(ez, s0 / rz)))
            return p, n, (i / seg, 0.5 + s / 2)
        for i in range(seg):
            q = [pt(i, -1), pt(i + 1, -1), pt(i + 1, 1), pt(i, 1)]
            self.poly(m, [x[0] for x in q], normals=[x[1] for x in q], uvs=[x[2] for x in q])

    def loft(self, m, rings, smooth=False, caps=True, metric=False):
        """Surface entre des anneaux de points (même nombre de points, anneaux convexes)."""
        n = len(rings[0])
        cen = [tuple(sum(p[k] for p in r) / len(r) for k in range(3)) for r in rings]
        for i in range(len(rings) - 1):
            mid = lerp(cen[i], cen[i + 1], 0.5)
            for j in range(n):
                j2 = (j + 1) % n
                q = [rings[i][j], rings[i][j2], rings[i + 1][j2], rings[i + 1][j]]
                hint = sub(tuple(sum(p[k] for p in q) / 4 for k in range(3)), mid)
                uv = [(j / n, i / (len(rings) - 1)), ((j + 1) / n, i / (len(rings) - 1)),
                      ((j + 1) / n, (i + 1) / (len(rings) - 1)), (j / n, (i + 1) / (len(rings) - 1))]
                if metric:      # UV en mètres : u le long de la planche, v autour de la section (grain du bois dans le bon sens)
                    u0, u1 = dist(cen[0], cen[i]), dist(cen[0], cen[i + 1])
                    v0 = sum(dist(rings[i][k], rings[i][(k + 1) % n]) for k in range(j))
                    v1 = v0 + dist(rings[i][j], rings[i][j2])
                    uv = [(u0, v0), (u0, v1), (u1, v1), (u1, v0)]
                if smooth:
                    ns = [sub(rings[i][j], cen[i]), sub(rings[i][j2], cen[i]),
                          sub(rings[i + 1][j2], cen[i + 1]), sub(rings[i + 1][j], cen[i + 1])]
                    self.poly(m, q, normals=ns, uvs=uv, hint=hint)
                else:
                    self.poly(m, q, uvs=uv, hint=hint)
        if caps:
            self.poly(m, rings[0], hint=sub(cen[0], cen[1]))
            self.poly(m, rings[-1], hint=sub(cen[-1], cen[-2]))

    def rock(self, m, c, r, seed, rough=0.12, level=2):
        """Rocher facetté : icosphère déformée par un bruit sinusoïdal (ombrage plat)."""
        rng = random.Random(seed)
        waves = [(norm((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))), rng.uniform(2.0, 5.5),
                  rng.uniform(0, 6.283), rng.uniform(0.4, 1.0)) for _ in range(5)]
        tot = sum(w[3] for w in waves)
        V, F = icosphere(level)

        def disp(d):
            k = 1 + rough * sum(a * math.sin(f * dot(d, dr) + ph) for dr, f, ph, a in waves) / tot * 2
            return (c[0] + r[0] * d[0] * k, c[1] + r[1] * d[1] * k, c[2] + r[2] * d[2] * k)
        P = [disp(v) for v in V]
        for a, b, d in F:
            tri = [P[a], P[b], P[d]]
            cen = tuple(sum(p[k] for p in tri) / 3 for k in range(3))
            self.poly(m, tri, hint=sub(cen, c))

    # ---- export ---------------------------------------------------------
    def write(self, outdir):
        used, groups = [], []
        for m, _, g in self.polys:
            if m not in used:
                used.append(m)
            if g not in groups:
                groups.append(g)
        vs, vts, vns = {}, {}, {}
        vl, vtl, vnl = [], [], []

        def idx(d, lst, key, val):
            if key not in d:
                d[key] = len(lst) + 1
                lst.append(val)
            return d[key]
        lines = ["# %s - généré par generate_models.py" % self.name, "mtllib %s.mtl" % self.name, "o %s" % self.name]
        for g, pv in self.pivots.items():
            lines.append("# pivot %s %.4f %.4f %.4f" % ((g,) + tuple(pv)))
        body = []
        for gname, mname in [(g, m) for g in groups for m in used]:
            first = True
            for m, items, g in self.polys:
                if m != mname or g != gname:
                    continue
                if first:
                    body += ["g %s" % gname, "usemtl %s" % mname]
                    first = False
                ids = []
                for p, n, uv in items:
                    a = idx(vs, vl, tuple(round(x, 5) for x in p), p)
                    b = idx(vts, vtl, tuple(round(x, 4) for x in uv), uv)
                    c = idx(vns, vnl, tuple(round(x, 4) for x in n), n)
                    ids.append("%d/%d/%d" % (a, b, c))
                for k in range(1, len(ids) - 1):
                    body.append("f %s %s %s" % (ids[0], ids[k], ids[k + 1]))
        lines += ["v %.5f %.5f %.5f" % p for p in vl]
        lines += ["vt %.4f %.4f" % uv for uv in vtl]
        lines += ["vn %.4f %.4f %.4f" % n for n in vnl]
        lines += body
        with open(os.path.join(outdir, self.name + ".obj"), "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        out = ["# matériaux de %s" % self.name]
        for mname in used:
            d = self.mats[mname]
            out += ["", "newmtl %s" % mname, "Ka 0.1 0.1 0.1", "Kd %.3f %.3f %.3f" % d["Kd"],
                    "Ks %.3f %.3f %.3f" % d["Ks"], "Ns %.1f" % d["Ns"]]
            if d["Ke"]:
                out.append("Ke %.3f %.3f %.3f" % d["Ke"])
            out += ["d 1.0", "illum 2"]
        with open(os.path.join(outdir, self.name + ".mtl"), "w", newline="\n") as f:
            f.write("\n".join(out) + "\n")
        ntri = sum(len(it) - 2 for _, it, _ in self.polys)
        print("%-18s %6d sommets  %6d triangles  %d matériaux" % (self.name, len(vl), ntri, len(used)))


_ico_cache = {}


def icosphere(level):
    if level in _ico_cache:
        return _ico_cache[level]
    t = (1 + 5 ** 0.5) / 2
    V = [norm(v) for v in [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t),
                           (0, -1, -t), (0, 1, -t), (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]]
    F = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
         (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
         (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range(level):
        cache, NF = {}, []

        def mid(a, b):
            k = (min(a, b), max(a, b))
            if k not in cache:
                V.append(norm(lerp(V[a], V[b], 0.5)))
                cache[k] = len(V) - 1
            return cache[k]
        for a, b, c in F:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            NF += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        F = NF
    _ico_cache[level] = (V, F)
    return V, F


# --------------------------------------------------------------------------
#  Outils de composition
# --------------------------------------------------------------------------
def tilted(axis, rng, amount):
    """Axe légèrement incliné dans une direction aléatoire (pour les bandelettes)."""
    axis = norm(axis)
    perp = norm(cross(axis, (rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))))
    a = rng.uniform(-amount, amount)
    return norm(add(mul(axis, math.cos(a)), mul(perp, math.sin(a))))


def wrap_limb(m, p0, p1, r0, r1, n, rng, tilt=0.4, swell=1.07):
    """Bandelettes enroulées autour d'un membre p0 -> p1."""
    axis = norm(sub(p1, p0))
    spacing = dist(p0, p1) / n
    for k in range(n):
        t = (k + 0.5) / n
        r = flerp(r0, r1, t) * swell
        mt = "bandage_dark" if rng.random() < 0.3 else "bandage"
        m.band(mt, lerp(p0, p1, t), tilted(axis, rng, tilt), r, r, spacing * 1.55)


# --------------------------------------------------------------------------
#  1. HACHE
# --------------------------------------------------------------------------
AXE_MATS = {
    "wood": mat((0.46, 0.30, 0.16), (0.05, 0.04, 0.03), 12),
    "leather": mat((0.20, 0.11, 0.07), (0.04, 0.03, 0.03), 8),
    "steel": mat((0.62, 0.64, 0.68), (0.9, 0.9, 0.95), 120),
    "steel_dark": mat((0.30, 0.31, 0.34), (0.5, 0.5, 0.55), 60),
}


def build_axe():
    m = Mesh("axe", AXE_MATS)
    # manche (légèrement renflé en bas), pommeau, bouchon
    m.ellipsoid("wood", (0, 0.015, 0), (0.036, 0.03, 0.036))
    m.tube("wood", (0, 0.015, 0), (0, 0.34, 0), 0.030, 0.0245)
    m.tube("wood", (0, 0.34, 0), (0, 0.97, 0), 0.0245, 0.020)
    m.ellipsoid("wood", (0, 0.97, 0), (0.020, 0.012, 0.020), 16, 8)
    # poignée en cuir
    for i in range(7):
        y = 0.07 + i * 0.038
        m.band("leather", (0, y, 0), (0.05 * (i % 2) - 0.025, 1, 0), 0.0305 - i * 0.0006, 0.0305 - i * 0.0006, 0.05)
    # douille d'acier (collier) et rivets
    m.tube("steel_dark", (0, 0.61, 0), (0, 0.89, 0), 0.043, 0.043, 20)
    for y in (0.61, 0.88):
        m.tube("steel", (0, y - 0.012, 0), (0, y + 0.012, 0), 0.049, 0.049, 20)
    for y in (0.70, 0.80):
        for z in (-1, 1):
            m.sphere("steel", (0, y, z * 0.044), 0.009, 10, 6)
    # lame : anneaux hexagonaux dont l'épaisseur diminue vers le fil
    rings, N = [], 13
    for i in range(N):
        t = i / (N - 1)
        y = 0.54 + 0.43 * t
        s = 1 - (2 * t - 1) ** 2
        xe = 0.15 + 0.12 * s ** 0.55            # fil convexe, cornes haute et basse
        h = 0.012 + 0.020 * s ** 0.8
        xm = 0.03 + 0.42 * (xe - 0.03)
        yc = 0.755
        yb = yc + (y - yc) * 0.55                  # côté manche : lame étroite, qui s'évase vers le fil
        ym = yc + (y - yc) * 0.78
        rings.append([(0.03, yb, h), (xm, ym, 0.8 * h), (xe, y, 0.002), (xe, y, -0.002), (xm, ym, -0.8 * h), (0.03, yb, -h)])
    m.loft("steel", rings)
    # talon (contre-poids) en acier sombre + pointe
    m.box("steel_dark", (-0.052, 0.76, 0), (0.074, 0.085, 0.058))
    m.tube("steel_dark", (-0.089, 0.76, 0), (-0.17, 0.76, 0), 0.026, 0.004, 12)
    return m


# --------------------------------------------------------------------------
#  2. ARAIGNÉE GÉANTE
# --------------------------------------------------------------------------
SPIDER_MATS = {
    "chitin": mat((0.14, 0.09, 0.07), (0.45, 0.40, 0.35), 70),
    "chitin_dark": mat((0.035, 0.028, 0.03), (0.30, 0.28, 0.30), 50),
    "marking": mat((0.62, 0.06, 0.03), (0.3, 0.2, 0.2), 40),
    "eye": mat((0.9, 0.1, 0.08), (0.8, 0.8, 0.8), 120, ke=(0.8, 0.05, 0.04)),
    "fang": mat((0.82, 0.76, 0.58), (0.4, 0.4, 0.35), 40),
}


def build_spider():
    m = Mesh("giant_spider", SPIDER_MATS)
    rng = random.Random(7)
    # abdomen incliné, pédicule, céphalothorax
    with m.at(rot_x(0.2), (0, 0.95, -0.55)):
        m.ellipsoid("chitin", (0, 0, 0), (0.48, 0.42, 0.62), 28, 18)
        m.ellipsoid("marking", (0, 0.395, 0.13), (0.16, 0.05, 0.12), 16, 8)     # sablier
        m.ellipsoid("marking", (0, 0.385, -0.17), (0.12, 0.05, 0.10), 16, 8)
        for i in range(5):                                                       # pointes dorsales
            z = 0.30 - i * 0.17
            y = 0.42 * math.sqrt(max(1 - (z / 0.62) ** 2, 0)) * 0.96
            for x in (-0.22, 0.22):
                m.tube("chitin_dark", (x, y - 0.03, z), (x * 1.3, y + 0.10, z - 0.04), 0.022, 0.0, 8)
        m.tube("chitin_dark", (0, -0.06, -0.60), (0, -0.10, -0.80), 0.07, 0.02, 10)   # filières
    m.ellipsoid("chitin_dark", (0, 0.89, -0.12), (0.12, 0.10, 0.12), 14, 8)
    m.ellipsoid("chitin", (0, 0.85, 0.25), (0.32, 0.22, 0.38), 28, 16)
    m.ellipsoid("chitin_dark", (0, 1.0, 0.20), (0.15, 0.06, 0.22), 14, 6)      # plaque dorsale
    # 8 yeux
    for x, y, z, r in ((0.09, 0.97, 0.56, 0.05), (0.17, 1.0, 0.44, 0.035), (0.22, 0.93, 0.47, 0.03), (0.10, 1.04, 0.40, 0.03)):
        for sx in (-1, 1):
            m.sphere("eye", (sx * x, y, z), r, 12, 8)
    # chélicères, crocs, pédipalpes
    for sx in (-1, 1):
        m.ellipsoid("chitin_dark", (sx * 0.10, 0.78, 0.60), (0.085, 0.11, 0.09), 14, 8)
        m.tube("fang", (sx * 0.10, 0.74, 0.64), (sx * 0.07, 0.62, 0.80), 0.05, 0.034, 10)
        m.tube("fang", (sx * 0.07, 0.62, 0.80), (sx * 0.06, 0.46, 0.76), 0.034, 0.005, 10)
        m.tube("chitin", (sx * 0.22, 0.78, 0.52), (sx * 0.26, 0.56, 0.84), 0.04, 0.026, 10)
        m.sphere("chitin_dark", (sx * 0.26, 0.55, 0.85), 0.032, 10, 6)
    # 8 pattes à 3 segments
    angles = [52, 18, -18, -52]
    lens = [1.15, 1.0, 1.0, 1.2]
    zs = [0.20, 0.08, -0.06, -0.18]
    for sx in (-1, 1):
        for k in range(4):
            th = math.radians(angles[k])
            d = (sx * math.cos(th), 0, math.sin(th))
            L = lens[k]
            S = (sx * 0.24, 0.85, 0.25 + zs[k])
            K = add(add(S, mul(d, 0.6 * L)), (0, 0.55, 0))
            A = add(add(S, mul(d, 1.3 * L)), (0, -0.45, 0))
            P = add(add(S, mul(d, 1.65 * L)), (0, -0.85, 0))
            with m.group("leg_%s%d" % ("p" if sx > 0 else "n", k), S):
                m.sphere("chitin_dark", S, 0.075, 12, 8)
                m.tube("chitin", S, K, 0.06, 0.052, 12)
                m.sphere("chitin_dark", K, 0.058, 12, 8)
                m.tube("chitin", K, A, 0.05, 0.034, 12)
                m.sphere("chitin_dark", A, 0.038, 10, 6)
                m.tube("chitin_dark", A, P, 0.034, 0.008, 10)
                out = norm(add(mul(d, 0.5), (0, 0.8, 0)))
                for t in (0.25, 0.5, 0.75):                 # poils raides sur le tibia
                    b = lerp(K, A, t)
                    m.tube("chitin_dark", b, add(b, mul(out, 0.09)), 0.009, 0.0, 6)
    return m


# --------------------------------------------------------------------------
#  3. MOMIE
# --------------------------------------------------------------------------
MUMMY_MATS = {
    "bandage": mat((0.78, 0.70, 0.52), (0.03, 0.03, 0.03), 6),
    "bandage_dark": mat((0.52, 0.43, 0.29), (0.03, 0.03, 0.03), 6),
    "skin_dark": mat((0.16, 0.10, 0.07), (0.05, 0.05, 0.05), 10),
    "eye_glow": mat((0.3, 1.0, 0.5), (0.5, 0.5, 0.5), 50, ke=(0.25, 0.9, 0.4)),
}

TORSO = [(0.86, 0.19, 0.12), (0.96, 0.21, 0.135), (1.10, 0.17, 0.12), (1.26, 0.20, 0.13),
         (1.42, 0.24, 0.15), (1.52, 0.22, 0.14), (1.58, 0.10, 0.09), (1.64, 0.058, 0.058), (1.69, 0.055, 0.055)]


def torso_radii(y):
    for a, b in zip(TORSO, TORSO[1:]):
        if a[0] <= y <= b[0]:
            t = (y - a[0]) / (b[0] - a[0])
            return flerp(a[1], b[1], t), flerp(a[2], b[2], t)
    return TORSO[-1][1], TORSO[-1][2]


def build_mummy():
    m = Mesh("mummy", MUMMY_MATS)
    rng = random.Random(21)
    m.lathe("bandage", TORSO, 24)
    # bandelettes du tronc
    for k in range(17):
        y = 0.90 + k * 0.040
        rx, rz = torso_radii(y)
        ax = norm((rng.uniform(-0.08, 0.08), 1, rng.uniform(-0.08, 0.08)))
        m.band("bandage_dark" if rng.random() < 0.3 else "bandage", (0, y, 0), ax, rx * 1.04, rz * 1.04, 0.075, 24)
    # poitrine éventrée : côtes visibles
    m.ellipsoid("skin_dark", (0.09, 1.40, 0.14), (0.065, 0.075, 0.02), 14, 8)
    for i in range(3):
        m.box("bandage_dark", (0.09, 1.37 + i * 0.03, 0.158), (0.10, 0.008, 0.008))
    # épaules
    for sx in (-1, 1):
        with m.group("arm_p" if sx > 0 else "arm_n", (sx * 0.27, 1.47, 0)):
            m.sphere("bandage", (sx * 0.27, 1.47, 0), 0.085, 14, 10)
    # tête : crâne bandé, bande de visage sombre, orbites lumineuses
    hc, hr = (0, 1.76, 0.01), (0.105, 0.13, 0.12)
    m.ellipsoid("bandage", hc, hr, 24, 16)
    for k, y in enumerate((1.655, 1.69, 1.725, 1.83, 1.862, 1.89)):
        f = math.sqrt(max(1 - ((y - hc[1]) / hr[1]) ** 2, 0.01))
        ax = norm((rng.uniform(-0.3, 0.3), 1, rng.uniform(-0.3, 0.3)))
        m.band("bandage_dark" if k % 3 == 1 else "bandage", (0, y, hc[2]), ax, hr[0] * f * 1.08, hr[2] * f * 1.08, 0.05, 20)
    m.band("skin_dark", (0, 1.775, hc[2]), (0, 1, 0), hr[0] * 1.01, hr[2] * 1.01, 0.08, 24)
    for sx in (-1, 1):
        m.ellipsoid("eye_glow", (sx * 0.045, 1.777, 0.118), (0.022, 0.014, 0.012), 12, 8)
    m.ellipsoid("bandage", (0, 1.635, 0.04), (0.07, 0.04, 0.07), 14, 8)        # menton
    # jambes
    for sx in (-1, 1):
        with m.group("leg_p" if sx > 0 else "leg_n", (sx * 0.11, 0.95, 0)):
            hip, knee, ank = (sx * 0.11, 0.95, 0), (sx * 0.10, 0.50, 0.01), (sx * 0.10, 0.10, 0)
            m.tube("bandage", hip, knee, 0.088, 0.068, 16)
            m.tube("bandage", knee, ank, 0.068, 0.052, 16)
            m.sphere("bandage", knee, 0.07, 12, 8)
            m.ellipsoid("bandage", (sx * 0.10, 0.05, 0.07), (0.065, 0.05, 0.14), 16, 10)
            wrap_limb(m, hip, knee, 0.088, 0.068, 9, rng)
            wrap_limb(m, knee, ank, 0.068, 0.052, 8, rng)
            wrap_limb(m, ank, (sx * 0.10, 0.04, 0.0), 0.054, 0.058, 2, rng)
    # bras tendus vers l'avant
    for sx in (-1, 1):
        with m.group("arm_p" if sx > 0 else "arm_n", (sx * 0.27, 1.47, 0)):
            sh, el, wr = (sx * 0.27, 1.47, 0), (sx * 0.31, 1.28, 0.17), (sx * 0.28, 1.27, 0.50)
            m.tube("bandage", sh, el, 0.058, 0.046, 14)
            m.sphere("bandage", el, 0.048, 12, 8)
            m.tube("bandage", el, wr, 0.046, 0.036, 14)
            wrap_limb(m, sh, el, 0.058, 0.046, 6, rng)
            wrap_limb(m, el, wr, 0.046, 0.036, 8, rng)
            m.ellipsoid("skin_dark", (sx * 0.28, 1.262, 0.57), (0.042, 0.02, 0.075), 12, 8)      # main
            for f in range(4):                                                                  # doigts
                fx = sx * (0.28 + (f - 1.5) * 0.02)
                m.tube("skin_dark", (fx, 1.262, 0.62), (fx, 1.25, 0.70), 0.009, 0.005, 6)
            # lambeaux de bandes qui pendent
            for j in range(3):
                t = 0.3 + 0.3 * j
                p = lerp(el, wr, t)
                ln = rng.uniform(0.18, 0.32)
                with m.at(mm(rot_z(rng.uniform(-0.12, 0.12)), rot_x(rng.uniform(-0.15, 0.15))), (p[0], p[1] - 0.04, p[2])):
                    m.box("bandage_dark", (0, -ln / 2, 0), (0.035, ln, 0.004))
    for sx in (-0.1, 0.0, 0.1):                                                            # lambeaux à la taille
        with m.at(rot_x(rng.uniform(-0.1, 0.1)), (sx * 1.2, 1.02, -0.13)):
            m.box("bandage_dark", (0, -0.15, 0), (0.04, 0.3, 0.004))
    return m


# --------------------------------------------------------------------------
#  4. ÉCHELLE
# --------------------------------------------------------------------------
LADDER_MATS = {
    "wood": mat((0.50, 0.34, 0.19), (0.05, 0.04, 0.03), 10),
    "wood_dark": mat((0.34, 0.22, 0.12), (0.04, 0.03, 0.02), 8),
    "nail": mat((0.25, 0.25, 0.27), (0.6, 0.6, 0.6), 60),
}


def build_ladder():
    m = Mesh("ladder", LADDER_MATS)
    H, W = 3.0, 0.52
    for sx in (-1, 1):
        x = sx * W / 2
        m.box("wood", (x, H / 2, 0), (0.06, H, 0.045))
        m.ellipsoid("wood", (x, H, 0), (0.03, 0.012, 0.0225), 12, 6)
        m.box("wood_dark", (x, 0.02, 0), (0.07, 0.04, 0.05))                 # patin
    for i in range(9):
        y = 0.35 + i * 0.3
        m.tube("wood", (-W / 2 - 0.012, y, 0), (W / 2 + 0.012, y, 0), 0.022, 0.022, 14)
        for sx in (-1, 1):
            m.tube("nail", (sx * (W / 2 + 0.03), y, 0.0), (sx * (W / 2 + 0.036), y, 0.0), 0.011, 0.009, 8)
    return m


# --------------------------------------------------------------------------
#  5. PLANCHES
# --------------------------------------------------------------------------
def plank(m, seed, nails=(-0.7, 0.7), length=1.6, width=0.22, thick=0.035):
    """Planche brute : longueur sur X, épaisseur sur Y (face clouée = +Y). Arêtes chanfreinées."""
    rng = random.Random(seed)
    a, b, c = thick / 2, width / 2, 0.005
    stations = 7
    rings = []
    for i in range(stations):
        x = -length / 2 + length * i / (stations - 1)
        bw = b * (1 + rng.uniform(-0.012, 0.012))
        sag = 0.002 * math.sin(math.pi * i / (stations - 1)) * (1 if seed % 2 else -1)
        cut = 0.025 * (1 if seed % 3 else -1)
        shape = [(a, -bw + c), (a, bw - c), (a - c, bw), (-a + c, bw), (-a, bw - c), (-a, -bw + c), (-a + c, -bw), (a - c, -bw)]
        ring = []
        for y, z in shape:
            xx = x + (cut * z / b if i in (0, stations - 1) else 0)   # coupe d'extrémité légèrement oblique
            ring.append((xx, y + sag, z))
        rings.append(ring)
    m.loft("wood", rings, smooth=False, metric=True)
    top = a + 0.0004
    for k in range(4):                                               # veines du bois
        z = rng.uniform(-b + 0.02, b - 0.02)
        ln = rng.uniform(0.5, 1.3)
        x0 = rng.uniform(-length / 2 + ln / 2 + 0.03, length / 2 - ln / 2 - 0.03)
        m.box("wood_dark", (x0, top, z), (ln, 0.0012, rng.uniform(0.002, 0.004)))
    for k in range(2):                                               # nœuds
        m.ellipsoid("wood_dark", (rng.uniform(-0.55, 0.55), a - 0.0005, rng.uniform(-0.06, 0.06)),
                    (rng.uniform(0.02, 0.035), 0.0045, rng.uniform(0.012, 0.02)), 14, 6)
    for nx in nails:                                                 # clous
        nz = rng.uniform(-0.03, 0.03)
        m.tube("nail", (nx, a, nz), (nx, a + 0.007, nz), 0.0085, 0.0095, 10)
        m.tube("nail", (nx, a + 0.007, nz), (nx, a + 0.0095, nz), 0.0095, 0.0075, 10)


def build_plank():
    m = Mesh("plank", LADDER_MATS)
    with m.at(I3, (0, 0.0175, 0)):                                   # posée sur le sol
        plank(m, 3)
    return m


def build_barricade():
    m = Mesh("planks_barricade", LADDER_MATS)
    # deux montants verticaux derrière, quatre planches clouées devant
    for i, sx in enumerate((-0.5, 0.5)):
        with m.at(mm(rot_z(math.pi / 2), rot_x(math.pi / 2)), (sx, 1.0, 0.0)):
            m.uvswap = True
            plank(m, 10 + i, nails=(), length=2.0, width=0.2)
            m.uvswap = False
    for i, y in enumerate((0.3, 0.77, 1.24, 1.71)):
        tilt = (-0.025, 0.02, -0.015, 0.03)[i]
        with m.at(mm(rot_z(tilt), rot_x(math.pi / 2)), (0.02 * (i % 2), y, 0.0525)):
            plank(m, 20 + i, nails=(-0.5, 0.5), length=1.5 + 0.1 * (i % 2), width=0.2)
    return m


# --------------------------------------------------------------------------
#  6. GOLEM DE LAVE
# --------------------------------------------------------------------------
GOLEM_MATS = {
    "basalt": mat((0.13, 0.11, 0.10), (0.08, 0.08, 0.08), 12),
    "basalt_light": mat((0.25, 0.20, 0.17), (0.08, 0.08, 0.08), 12),
    "obsidian": mat((0.03, 0.03, 0.045), (0.9, 0.9, 1.0), 150),
    "lava": mat((1.0, 0.36, 0.05), (0.2, 0.1, 0.05), 20, ke=(1.5, 0.3, 0.04)),
    "lava_bright": mat((1.0, 0.8, 0.25), (0.3, 0.3, 0.2), 20, ke=(1.9, 0.8, 0.15)),
}


def build_golem():
    m = Mesh("lava_golem", GOLEM_MATS)
    rng = random.Random(99)
    seed = [100]

    def rock(c, r, mt="basalt", rough=0.11):
        seed[0] += 1
        m.rock(mt, c, r, seed[0], rough)

    def crack(c, r, d, size=0.25, mt="lava"):
        """Veine de lave posée à la surface d'un rocher (c, r) dans la direction d."""
        d = norm(d)
        k = 1 / math.sqrt((d[0] / r[0]) ** 2 + (d[1] / r[1]) ** 2 + (d[2] / r[2]) ** 2)
        p = add(c, mul(d, k * 1.03))
        n = norm((d[0] / r[0], d[1] / r[1], d[2] / r[2]))
        t = norm(sub(rng.choice([(0, 1, 0), (1, 0, 0), (0.6, 0.8, 0)]), mul(n, dot(n, (0, 1, 0)))))
        if length(t) < 0.1:
            t = norm(cross(n, (1, 0, 0)))
        R = basis(cross(t, n), t, n)
        with m.at(R, p):
            m.ellipsoid(mt, (0, 0, 0), (0.035, size, 0.022), 8, 6)

    # --- jambes ---
    for sx in (-1, 1):
        with m.group("leg_p" if sx > 0 else "leg_n", (sx * 0.5, 1.95, 0)):
            rock((sx * 0.50, 1.50, 0.0), (0.40, 0.52, 0.40), "basalt")                 # cuisse
            m.sphere("lava", (sx * 0.50, 1.00, 0.08), 0.27, 14, 10)                     # genou
            rock((sx * 0.52, 0.58, 0.04), (0.38, 0.56, 0.38), "basalt_light")           # tibia
            rock((sx * 0.54, 0.17, 0.20), (0.44, 0.19, 0.55), "basalt")                 # pied
            for a in (-0.2, 0.0, 0.2):                                                  # orteils
                rock((sx * 0.54 + a * 1.2, 0.12, 0.62), (0.13, 0.11, 0.17), "basalt_light", 0.08)
            crack((sx * 0.50, 1.50, 0.0), (0.40, 0.52, 0.40), (sx * 0.4, 0.2, 1.0), 0.24)
            crack((sx * 0.52, 0.58, 0.04), (0.38, 0.56, 0.38), (sx * 0.3, 0.3, 1.0), 0.28)
    # --- bassin et taille ---
    rock((0, 2.0, 0), (0.70, 0.38, 0.48), "basalt")
    m.ellipsoid("lava", (0, 2.34, 0), (0.60, 0.22, 0.52), 20, 10)
    m.ellipsoid("lava_bright", (0, 2.34, 0.0), (0.50, 0.20, 0.44), 16, 8)
    # --- torse ---
    rock((0, 2.95, -0.05), (1.00, 0.72, 0.72), "basalt")
    rock((0, 3.3, -0.55), (0.80, 0.55, 0.50), "basalt_light")                       # bosse dorsale
    m.ellipsoid("lava_bright", (0, 2.85, 0.50), (0.44, 0.40, 0.28), 18, 12)        # cœur en fusion
    for c, r in (((-0.45, 2.85, 0.60), (0.36, 0.44, 0.24)), ((0.45, 2.85, 0.60), (0.36, 0.44, 0.24)),
                 ((0, 3.25, 0.58), (0.50, 0.20, 0.24)), ((0, 2.50, 0.56), (0.42, 0.18, 0.22))):
        rock(c, r, "basalt_light", 0.08)                                           # plaques autour du cœur
    for i in range(7):
        a = rng.uniform(-1.1, 1.1)
        crack((0, 2.95, -0.05), (1.00, 0.72, 0.72), (a, rng.uniform(-0.3, 0.8), 0.9), rng.uniform(0.15, 0.3))
    # --- épaules, bras ---
    for sx in (-1, 1):
        with m.group("arm_p" if sx > 0 else "arm_n", (sx * 1.1, 3.05, 0)):
            m.sphere("lava", (sx * 1.05, 3.05, 0.0), 0.34, 14, 10)                      # articulation d'épaule
            rock((sx * 1.28, 3.20, 0.0), (0.52, 0.44, 0.52), "basalt_light")           # épaulière
            for j in range(3):                                                         # pointes d'obsidienne
                ox = sx * (1.18 + j * 0.16)
                m.tube("obsidian", (ox, 3.5 - j * 0.04, 0.0), (ox + sx * 0.05, 4.0 - j * 0.12, -0.05 + j * 0.05), 0.11, 0.0, 8)
            rock((sx * 1.50, 2.35, 0.05), (0.38, 0.68, 0.38), "basalt")                # biceps
            m.sphere("lava", (sx * 1.52, 1.68, 0.12), 0.26, 14, 10)                    # coude
            rock((sx * 1.55, 1.15, 0.18), (0.44, 0.60, 0.44), "basalt_light")          # avant-bras
            rock((sx * 1.58, 0.55, 0.25), (0.58, 0.50, 0.58), "basalt")                # poing
            for j in range(3):
                m.sphere("basalt_light", (sx * (1.58 + (j - 1) * 0.28), 0.32, 0.55), 0.14, 10, 6)   # phalanges
            crack((sx * 1.50, 2.35, 0.05), (0.38, 0.68, 0.38), (sx * 0.5, 0.0, 1.0), 0.3)
            crack((sx * 1.55, 1.15, 0.18), (0.44, 0.60, 0.44), (sx * 0.6, 0.2, 1.0), 0.28)
    # --- tête ---
    m.sphere("lava", (0, 3.45, 0.18), 0.25, 12, 8)                                   # cou incandescent
    rock((0, 3.72, 0.32), (0.44, 0.40, 0.42), "basalt")
    rock((0, 3.93, 0.52), (0.42, 0.10, 0.16), "basalt_light", 0.07)                 # arcade
    rock((0, 3.52, 0.52), (0.30, 0.14, 0.22), "basalt", 0.07)                       # mâchoire
    for sx in (-1, 1):
        with m.at(rot_z(sx * 0.35), (sx * 0.19, 3.78, 0.76)):
            m.ellipsoid("lava_bright", (0, 0, 0), (0.11, 0.045, 0.05), 12, 8)       # yeux
        m.tube("obsidian", (sx * 0.30, 3.96, 0.30), (sx * 0.62, 4.45, 0.12), 0.095, 0.0, 10)   # cornes
    m.box("lava_bright", (0, 3.56, 0.72), (0.36, 0.05, 0.06))                       # bouche
    for i in range(5):
        x = (i - 2) * 0.075
        m.tube("obsidian", (x, 3.585, 0.73), (x, 3.52, 0.745), 0.018, 0.0, 6)
    # --- pointes dorsales ---
    for i, (x, y) in enumerate(((-0.55, 3.55), (-0.25, 3.8), (0.0, 3.85), (0.25, 3.8), (0.55, 3.55), (-0.3, 3.1), (0.3, 3.1))):
        m.tube("obsidian", (x, y - 0.1, -0.7), (x * 1.3, y + 0.35, -1.25), 0.14, 0.0, 8)
    # --- gouttes de lave ---
    for x, y, z in ((0.1, 1.88, 0.5), (-0.6, 2.10, 0.4), (0.8, 2.25, 0.3)):
        m.ellipsoid("lava", (x, y, z), (0.03, 0.06, 0.03), 8, 6)
    return m


# --------------------------------------------------------------------------
def main():
    out = os.path.dirname(os.path.abspath(__file__))
    for build in (build_axe, build_spider, build_mummy, build_ladder, build_plank, build_barricade, build_golem):
        build().write(out)


if __name__ == "__main__":
    main()
