#!/usr/bin/env python
# Copyright (C) 2011 by Kenneth Waters
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Example pentominos solver.

Dana Scott studied covering a chessboard without the four center squares with
one of each of the twelve pentominos.  Scott used a novel backtracking approach
tuned to his exact problem.  Here we will use exact cover.

The universe consists of 72 elements.
  - One for each of the 12 pentominos.
  - One for each of the 60 non-center squares on the chess board.

Each subset represents one way to position one pentomino.  There are 1568 such
subsets.

There are 520 tilings (65 if we eliminate reflections and rotations).

"""
from __future__ import print_function

import pprint

import exactcover

# The twelve pentominos, along with their names.
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
    """List the rotations and reflections of a pentomino."""
    # The 8 orientations are combinations of 3 primitive operations, x-flip,
    # y-flip, and transpose.
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

    # Helper to add unique shapes to a list
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
    """List the positions a pentomino can be in.

    'world' is a set containing all of the legal squares on the board.
    'width' and 'height' are the maximum extents of world.

    """
    for y in range(height):
        for x in range(width):
            new_shape = [(x + xx, y + yy) for xx, yy in shape]
            if set(new_shape).issubset(world):
                yield new_shape


def board():
    """List the legal squares on the board.

    The board is a standard 8x8 chess board with the center 4 squares removed.

    """
    b = set((x, y) for x in range(8) for y in range(8)
            if not (3 <= x < 5 and 3 <= y < 5))
    return b


def matrix():
    """Compute the set of covers representing the problem."""
    b = board()

    covers = []
    for name, shape in pentominos.iteritems():
        for rotation in rotations(shape):
            for position in positions(rotation, 8, 8, b):
                covers.append([name] + sorted(position))
    return covers


def solution_str(solution):
    """Turn a covering into a string picture representation."""
    grid = [[' ' for i in range(8)] for j in range(8)]

    # Mark unoccupied squares.
    for x, y in board():
        grid[y][x] = '.'

    # Mark each pentomino by its name.
    for row in solution:
        c = row[0]
        for x, y in row[1:]:
            grid[y][x] = c

    return "\n".join(''.join(row) for row in grid)


def main():
    m = matrix()

    print("Example covering:")
    # Take the first result from the iterator.
    solution = next(exactcover.Coverings(m))
    pprint.pprint(solution)
    print()
    print(solution_str(solution))
    print()

    # Count the number of results returned by the iterator.
    print("There are {0} unique tilings.".format(
        sum(1 for x in exactcover.Coverings(m))))


if __name__ == '__main__':
    main()
