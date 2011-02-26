#!/usr/bin/env python

import pprint

import exactcover

pentominos = {
    'f': [(1, 0), (2, 0), (0, 1), (1, 1), (1, 2)],
    'i': [(0, 0), (0, 1), (0, 2), (0, 3), (0, 4)],
    'l': [(0, 0), (0, 1), (0, 2), (0, 3), (1, 3)],
    'n': [(1, 0), (1, 1), (0, 2), (1, 2), (0, 3)],
    'p': [(0, 0), (1, 0), (0, 1), (1, 1), (0, 2)],
    't': [(0, 0), (1, 0), (2, 0), (1, 1), (1, 2)],
    'u': [(0, 0), (2, 0), (0, 1), (1, 1), (2, 1)],
    'v': [(0, 0), (0, 1), (0, 2), (1, 2), (2, 2)],
    'w': [(0, 0), (0, 1), (1, 1), (1, 2), (2, 2)],
    'x': [(1, 0), (0, 1), (1, 1), (2, 1), (1, 2)],
    'y': [(1, 0), (0, 1), (1, 1), (1, 2), (1, 3)],
    'z': [(0, 0), (1, 0), (1, 1), (1, 2), (2, 2)],
}

def rotations(shape):
    def xflip(shape):
        return [(4 - x, y) for x, y in shape]
    def yflip(shape):
        return [(x, 4 - y) for x, y in shape]
    def transpose(shape):
        return [(y, x) for x, y in shape]
    def align(shape):
        mx = min(x for x, y in shape)
        my = min(y for x, y in shape)
        return sorted((x - mx, y - my) for x, y in shape)

    out = []
    def add(shape):
        rotation = align(shape)
        if rotation not in out:
            out.append(rotation)

    add(shape)
    add(transpose(xflip(shape)))
    add(xflip(yflip(shape)))
    add(transpose(yflip(shape)))

    add(xflip(shape))
    add(yflip(shape))
    add(transpose(shape))
    add(transpose(xflip(yflip(shape))))

    return out


def positions(shape, width, height, world):
    for y in xrange(height):
        for x in xrange(width):
            new_shape = [(x + xx, y + yy) for xx, yy in shape]
            if set(new_shape).issubset(world):
                yield new_shape


def board():
    b = set()
    for x in xrange(8):
        for y in xrange(8):
            if 3 <= x < 5 and 3 <= y < 5:
                pass
            else:
                b.add((x, y))
    return b


def matrix():
    b = board()

    covers = []
    for name, shape in pentominos.iteritems():
        for rotation in rotations(shape):
            for position in positions(rotation, 8, 8, b):
                covers.append([name] + sorted(position))
    return covers


def show(solution):
    b = board()
    grid = [[' ' for i in xrange(8)] for j in xrange(8)]
    for x, y in b:
        grid[y][x] = '.'
    for row in solution:
        c = row[0]
        for x, y in row[1:]:
            grid[y][x] = c

    print "\n".join(''.join(row) for row in grid)


def main():
    m = matrix()

    print "Example covering:"
    solution = exactcover.coverings(m).next()
    pprint.pprint(solution)
    show(solution)

    print "There are {0} unique coverings.".format(
        sum(1 for x in exactcover.coverings(m)))


if __name__ == '__main__':
    main()
