def create_points(nx, ny, nz):
    points = []
    for k in range(nz + 1):
        for j in range(ny + 1):
            for i in range(nx + 1 + 1):
                p = (i, j, k)
                points.append(p)
    return points


def create_cells(nx, ny, nz, points):
    cells = []
    for k in range(nz):
        for j in range(ny):
            for i in range(nx + 1):
                if i == nx // 2:
                    continue
                p0 = points.index((i, j, k))
                p1 = points.index((i + 1, j, k))
                p2 = points.index((i + 1, j + 1, k))
                p3 = points.index((i, j + 1, k))
                p4 = points.index((i, j, k + 1))
                p5 = points.index((i + 1, j, k + 1))
                p6 = points.index((i + 1, j + 1, k + 1))
                p7 = points.index((i, j + 1, k + 1))
                c = (p0, p1, p2, p3, p4, p5, p6, p7)
                cells.append(c)
    return cells


def create_faces(nx, ny, nz, points):
    faces = []
    for j in range(ny):
        p0 = points.index((nx // 2, j, 0))
        p1 = points.index((nx // 2, j + 1, 0))
        p2 = points.index((nx // 2, j + 1, 1))
        p3 = points.index((nx // 2, j, 1))
        f = (p0, p1, p2, p3)
        faces.append(f)
    return faces


def coords_point(nx, ny, nz, p):
    tmp = p[0] - 1 if p[0] > (nx // 2) else p[0]
    return tmp, p[1], p[2]


def print_vtk(nx, ny, nz, points, faces, cells):
    print("# vtk DataFile Version 2.0")
    print("vtk output")
    print("ASCII")
    print("DATASET UNSTRUCTURED_GRID")
    print("POINTS " + str(len(points)) + " float")
    for p in points:
        pp = coords_point(nx, ny, nz, p)
        print(" ".join([str(xyz) for xyz in pp]))
    print("")
    n_cells = len(cells)
    n_faces = len(faces)
    print("CELLS " + str(n_cells + n_faces) + " " + str(n_cells * (8 + 1) + n_faces * (4 + 1)))
    for c in cells:
        print("8 " + " ".join([str(pi) for pi in c]))
    for f in faces:
        print("4 " + " ".join([str(pi) for pi in f]))
    print("")
    print("CELL_TYPES " + str(n_cells + n_faces))
    for c in cells:
        print("12")
    for f in faces:
        print("9")


def main():
    # nx, ny, nz = 5, 10, 1
    nx, ny, nz = 10, 10, 1
    assert nx % 2 == 0
    points = create_points(nx, ny, nz)
    assert (len(points) == 121 * 2 + 11 * 2)
    cells = create_cells(nx, ny, nz, points)
    assert (len(cells) == 100)
    faces = create_faces(nx, ny, nz, points)
    assert (len(faces) == ny)
    print_vtk(nx, ny, nz, points, faces, cells)


if __name__ == "__main__":
    main()
