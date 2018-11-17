#!/usr/bin/env python

import exactcover


def print_solution(n, solution):
    a = [
        [ "." for col in range(n) ]
        for row in range(n)
    ]
    for entry in solution:
        row = int(entry[0][1:])
        col = int(entry[1][1:])
        a[row][col] = "*"

    for row in a:
        print "".join(row)

def n_queens(n):
    matrix = []
    for row in range(n):
        for col in range(n):
            matrix.append((
                "r%d" % row,
                "c%d" % col,
                "d%d" % (row + col),
                "a%d" % (row + n-1 - col)
            ))

    # The diagonal and antidiagonal constraints are secondary,
    # since not every diagonal will have a queen on it.
    secondary = set(( "d%d" % i for i in range(2*n-1) ))
    secondary.update(set(( "a%d" % i for i in range(2*n-1) )))

    solution = exactcover.Coverings(matrix, secondary).next()
    print_solution(n, solution)

def main():
    n_queens(8)


if __name__ == '__main__':
    main()
