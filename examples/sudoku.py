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

"""Example Sudoku solver.

Sudoku can be easily solved by straight forward reduction to exact cover.
There are 364 elements in the universe.
  - One for each of the 81 cells, which must be occupied exactly once.
  - Nine for each of the 9 rows, which must contain every number exactly once.
  - Nine for each of the 9 columns, which must contain every number once.
  - Nine for each of the 9 boxes, which must contain every number once.
Each subset contains 4 elements, and represents one way to fill in a cell.

"""
from __future__ import print_function

import pprint

import exactcover

# Public domain puzzle from Wikipedia, courtesy Lawrence Leonard Gilbert.
sample_puzzle = """
    53..7....
    6..195...
    .98....6.
    8...6...3
    4..8.3..1
    7...2...6
    .6....28.
    ...419..5
    ....8..79
"""

def sudoku_matrix(puzzle):
    """Generate the constraint matrix from a sodoku puzzle.

    The puzzle is supplied as a string, rows are seprated by whitespace, '.'
    represents an empty sqaure, '1' - '9' represent fixed cells.  All other
    characters are illegal.

    We're using strings and tupples to represent the columns of the matrix.
    The exact cover solver only compares columns for equality, so we can use
    whatever representation is best for us.  Strings are easy to generate and
    are easy to display.

    """
    rows = puzzle.strip().split()
    if len(rows) != 9 or any(len(row) != 9 for row in rows):
        raise ValueError("Puzzle must be 9x9.")

    # Helper to add constraints
    m = []
    def add_constraint(x, y, c):
        d = {
            'x': x + 1,
            'y': y + 1,
            'c': c,
            'b': 3 * (y / 3) + x / 3 + 1
        }
        m.append(((x, y),
            'r{y}{c}'.format(**d),
            'c{x}{c}'.format(**d),
            'b{b}{c}'.format(**d)))

    # Add constraints for every cell of the puzzle
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c == '.':
                for c in '123456789':
                    add_constraint(x, y, c)
            elif c in '123456789':
                add_constraint(x, y, c)
            else:
                raise ValueError("'{0}' unexpected in puzzle.".format(c))

    return m


def solution_str(solution):
    """Turn a puzzle solution into a 2d string representation."""
    grid = [['' for i in xrange(9)] for j in xrange(9)]
    for row in solution:
        (x, y), (_, _, c), _, _, = row
        grid[y][x] = c
    return '\n'.join(''.join(c for c in row) for row in grid)


def sudoku(puzzle):
    """Solve a sudoku puzzle."""
    print("Solving puzzle:")
    print('\n'.join(puzzle.strip().split()))
    print()

    m = sudoku_matrix(sample_puzzle)
    solution = next(exactcover.Coverings(m))

    print("Solution partition:")
    pprint.pprint(solution)
    print()
    print("Solved puzzle:")
    print(solution_str(solution))


def main():
    sudoku(sample_puzzle)


if __name__ == '__main__':
    main()
