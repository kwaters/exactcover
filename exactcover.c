/* Copyright (C) 2011 by Kenneth Waters
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE. */

#include "Python.h"

static char exactcover__doc__[] =
"Exact cover solver.\n"
"\n"
"Solves the exact cover problem using Knuth's DLX, using the shortest\n"
"column first heuristic.\n"
"\n"
"Given a universe, U, and a collection of subsets of U, S.  Every\n"
"subcollection of S which is a partition of U is an exact cover.  Finding\n"
"an exact cover is NP-Complete.\n"
"\n"
"Many constraint satisfaction problems can be expressed in terms of exact\n"
"cover, making it useful in a variety of situations.\n";

/* #undef CHECK_INVARIANTS */

typedef struct ElementRec Element;
typedef struct HeaderRec Header;

/* states */
enum Action
{
    CONTINUE,
    BACKUP,
    SOLUTION
};

/* An element in the sparse matrix. */
struct ElementRec
{
    Element *up;
    Element *down;
    Element *left;
    Element *right;

    Header *column;

    /* A reference to this rows object.  If rows are <= 5 elements it's a win
     * to keep a pointer in every element.  It also reduces complexity. */
    PyObject *object;
};

/* A header for a column in the sparse matrix.  Keeps track of a count of
 * rows which have a '1' in this column. */
struct HeaderRec
{
    Element e;

    int count;
    PyObject *object;
};

/* ------------------------------------------------------------------------ *
 * Sparse Matrix Representation                                             *
 * ------------------------------------------------------------------------ */

#if defined(CHECK_INVARIANTS) && !defined(NDEBUG)
static void
check_row(Element *row)
{
    Element *e = row;
    do {
        assert(e->right->left == e);
        assert(e->left->right == e);
        e = e->right;
    } while (e != row);
}

static void
check_column(Header *column)
{
    int count = 0;
    Element *e = &column->e;
    do {
        assert(e->up->down = e);
        assert(e->down->up = e);
        assert(e->column = column);
        check_row(e);
        e = e->down;
        count++;
    } while (e != &column->e);
    assert(column->count == count - 1);
}

static void
check(Header *corner)
{
    Element *e;

    assert(corner->e.up == &corner->e);
    assert(corner->e.down == &corner->e);

    for (e = corner->e.right; e != &corner->e; e = e->right)
    {
        check_column((Header*)e);
    }
}
#define CHECK(x) check(x)
#else
#define CHECK(x)
#endif

/* Remove a column and all rows with a '1' in that column from the matrix. */
static void
unlink_column(Header *column)
{
    Element *row;
    Element *e;

    /* remove Header element */
    column->e.left->right = column->e.right;
    column->e.right->left = column->e.left;

    /* remove rows */
    for (row = column->e.up; row != &column->e; row = row->up) {
        for (e = row->left; e != row; e = e->left) {
            e->column->count--;
            e->up->down = e->down;
            e->down->up = e->up;
        }
    }
}

/* Remove a row from the matrix. */
static void
unlink_row(Element *row)
{
    Element *e = row;
    do {
        unlink_column(e->column);
        e = e->right;
    } while (e != row);
}

/* Put a column back into the matrix.  Must be called in the exact reverse
 * order of unlink_column(). */
static void
link_column(Header *column)
{
    Element *row;
    Element *e;

    /* link Header element */
    column->e.left->right = &column->e;
    column->e.right->left = &column->e;

    /* Add rows */
    for (row = column->e.down; row != &column->e; row = row->down) {
        for (e = row->right; e != row; e = e->right) {
            e->column->count++;
            e->up->down = e;
            e->down->up = e;
        }
    }
}

/* Put a row back into the matrix.  Must be called in the exact reverse order
 * of link_row(). */
static void
link_row(Element *row)
{
    Element *e = row->left;
    do {
        link_column(e->column);
        e = e->left;
    } while (e != row->left);
}

/* Return the header for the column with the fewest '1's.  Returns NULL if
 * there are no columns in the matrix */
static Header *
smallest_column(Header *corner)
{
    Header *smallest = NULL;

    Header *column = (Header *)corner->e.right;
    for (; column != corner; column = (Header *)column->e.right) {
        if (!smallest || smallest->count > column->count) {
            smallest = column;
        }
    }

    return smallest;
}

/* Return the number of columns.  Aka, the number of elements in the
 * universe. */
static int
column_count(Header *corner)
{
    int count = 0;
    Element *e = corner->e.right;
    for (; e != &corner->e; e = e->right) {
        count += 1;
    }
    return count;
}

/* Finds or inserts a column, returns NULL on failure. */
static Header *
find_column(Header *corner, PyObject *object)
{
    Header *i;

    /* Linear scan of the universe for this object */
    for (i = (Header *)corner->e.right; i != corner;
         i = (Header *)i->e.right) {
        int cmp = PyObject_RichCompareBool(i->object, object, Py_EQ);
        if (cmp == -1) {
            return NULL;
        } else if (cmp == 1) {
            return i;
        }
    }

    /* New header element. */
    i = PyMem_New(Header, 1);
    if (!i)
        return NULL;
    i->e.up = &i->e;
    i->e.down = &i->e;
    i->e.column = i;
    i->e.object = NULL;
    Py_INCREF(object);
    i->object = object;
    i->count = 0;

    /* Link into the header chain. */
    i->e.right = &corner->e;
    i->e.left = corner->e.left;
    corner->e.left->right = &i->e;
    corner->e.left = &i->e;

    return i;
}

/* Alloc a matrix */
static Header*
alloc_matrix()
{
    Header *corner = PyMem_New(Header, 1);
    if (!corner) {
        PyErr_NoMemory();
        return NULL;
    }
    corner->e.up = &corner->e;
    corner->e.down = &corner->e;
    corner->e.left = &corner->e;
    corner->e.right = &corner->e;
    corner->e.column = corner;
    corner->e.object = NULL;
    corner->count = 0;
    corner->object = NULL;

    return corner;
}

/* Free a matrix */
static void
free_matrix(Header *corner)
{
    Header *column;
    Header *next_column;
    Element *e;
    Element *next_e;

    /* Safe to delete NULL */
    if (corner == NULL)
        return;

    for (column = (Header *)corner->e.right;
         column != corner;
         column = next_column) {
        /* Free the elements of this column */
        for (e = column->e.down; e != &column->e; e = next_e) {
            next_e = e->down;
            Py_DECREF(e->object);
            PyMem_Del(e);
        }

        assert(column->e.object == NULL);

        next_column = (Header *)column->e.right;
        Py_DECREF(column->object);
        PyMem_Del(column);
    }

    PyMem_Del(corner);
}


/* ------------------------------------------------------------------------ *
 * coverings class                                                      *
 * ------------------------------------------------------------------------ */
typedef struct {
    PyObject_HEAD

    /* Sparse matrix representing the problem. */
    Header *corner;

    /* Non-zero if next() has never been called. */
    int first;

    /* A stack representing the current solution.  This has at most
     * len(universe) elements. */
    Element **solution;
    int solutionSize;
} coverings;

static char coverings__doc__[] =
"coverings(iterable) -> coverings object\n"
"\n"
"Compute exact covers.\n"
"\n"
"Return an coverings object whose .next() method returns a tuple of\n"
"elements from iterable which cover the union of all the elements of\n"
"iterable.  iterable must yield sequences.\n"
"\n"
"While the elements may be mutable, mutating them will have no effect on\n"
"the results produced.  It is recommended that they remain unchanged\n"
"during the iteration.\n";

/* Make one solving step, returns an Action.
 *
 * CONTINUE = Some of the universe is uncovered, call this function again.
 * BACKUP = The remaining universe cannot be covered, backtrack then call this
 *          function again.
 * SOLUTION = solution contains a covering set. */
static int
coverings_step(coverings *self)
{
    Header *column = NULL;
    Element *row = NULL;

    /* New column. */
    column = smallest_column(self->corner);
    if (column == NULL ) {
        return SOLUTION;
    } else if (column->count == 0) {
        return BACKUP;
    }
    row = column->e.down;

    /* Add new row. */
    unlink_row(row);
    CHECK(self->corner);
    self->solution[self->solutionSize] = row;
    self->solutionSize++;

    return CONTINUE;
}

/* backtrack.  Returns -1 if there are no more solutions */
static int
coverings_backup(coverings *self)
{
    while (self->solutionSize > 0) {
        Element *row = self->solution[self->solutionSize - 1];
        link_row(row);
        CHECK(self->corner);
        row = row->down;
        if (row == &row->column->e) {
            self->solutionSize--;
        } else {
            unlink_row(row);
            CHECK(self->corner);
            self->solution[self->solutionSize - 1] = row;
            return 0;
        }
    }
    return -1;
}

/* Create a tuple of the current solution stack. */
static PyObject *
coverings_solution(coverings *self)
{
    PyObject *tuple;
    int i;

    tuple = PyTuple_New(self->solutionSize);
    if (!tuple)
        return NULL;

    for (i = 0; i < self->solutionSize; i++) {
        PyObject *object = self->solution[i]->object;
        Py_INCREF(object);
        PyTuple_SET_ITEM(tuple, i, object);
    }
    return tuple;
}

static void
coverings_cleanup(coverings *self)
{
    if (self->corner) {
        /* restore matrix */
        while (--self->solutionSize >= 0)
            link_row(self->solution[self->solutionSize]);
    }

    free_matrix(self->corner);
    PyMem_Del(self->solution);
}

/* .next() */
static PyObject *
coverings_next(coverings *self)
{
    /* We need to backup from the last solution on every new iteration. */
    if (self->first) {
        self->first = 0;
    } else if (coverings_backup(self) < 0) {
        return NULL;
    }

    for (;;) {
        int action = coverings_step(self);
        switch (action) {
        case CONTINUE:
            break;

        case BACKUP:
            if (coverings_backup(self) < 0)
                return NULL;
            break;

        case SOLUTION:
            return coverings_solution(self);
        }
    }

    /* not reached. */
    return NULL;
}

/* .__init__() */
static int
coverings_init(coverings *self, PyObject *args, PyObject *kwds)
{
    PyObject *covers = NULL;
    PyObject *cover = NULL;
    PyObject *coverIt = NULL;
    PyObject *elem = NULL;
    PyObject *it = NULL;

    if (kwds && PyDict_Size(kwds) != 0) {
        PyErr_Format(PyExc_TypeError,
                     "coverings does not take keyword arguments");
        goto error;
    }
    if (!PyArg_ParseTuple(args, "O:coverings", &covers))
        goto error;

    coverings_cleanup(self);

    self->corner = alloc_matrix();
    if (!self->corner)
        goto error;

    if (!(coverIt = PyObject_GetIter(covers)))
        goto error;
    while ((cover = PyIter_Next(coverIt))) {
        Element *row = NULL;

        if (!(it = PyObject_GetIter(cover)))
            goto error;
        while ((elem = PyIter_Next(it))) {
            Element *e = NULL;
            Header *column = find_column(self->corner, elem);
            if (!column)
                goto error;

            /* Create element */
            e = PyMem_New(Element, 1);
            if (!e) {
                PyErr_NoMemory();
                goto error;
            }
            e->column = column;
            Py_INCREF(cover);
            e->object = cover;
            column->count++;

            /* Link into column */
            e->up = column->e.up;
            e->down = &column->e;
            column->e.up->down = e;
            column->e.up = e;

            /* Link into row */
            if (row == NULL) {
                row = e;
                e->left = e;
                e->right = e;
            } else {
                e->left = row->left;
                e->right = row;
                row->left->right = e;
                row->left = e;
            }

            Py_CLEAR(elem);
        }
        Py_CLEAR(it);
        Py_CLEAR(cover);
    }
    Py_CLEAR(coverIt);

    CHECK(self->corner);

    self->first = 1;
    self->solution = PyMem_New(Element *, column_count(self->corner));
    self->solutionSize = 0;
    if (!self->solution) {
        PyErr_NoMemory();
        goto error;
    }

    return 0;

error:
    Py_XDECREF(cover);
    Py_XDECREF(coverIt);
    Py_XDECREF(elem);
    Py_XDECREF(it);
    return -1;
}

/* .tp_dealloc */
static void
coverings_dealloc(coverings *self)
{
    coverings_cleanup(self);
    Py_TYPE((PyObject *)self)->tp_free((PyObject *)self);
}

static PyTypeObject coverings_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "exactcover.coverings",         /* tp_name */
    sizeof(coverings),              /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)coverings_dealloc,  /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    PyObject_HashNotImplemented,    /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    coverings__doc__,               /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,              /* tp_iter */
    (iternextfunc)coverings_next,   /* tp_iternext */
    0,                              /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    (initproc)coverings_init,       /* tp_init */
    PyType_GenericAlloc,            /* tp_alloc */
    PyType_GenericNew,              /* tp_new */
    0,                              /* tp_free */
    0,                              /* tp_is_gc */
    0,                              /* tp_bases */
    0,                              /* tp_mro */
    0,                              /* tp_cache */
    0,                              /* tp_subclasses */
    0,                              /* tp_weaklist */
    0                               /* tp_del */
};

/* ------------------------------------------------------------------------ *
 * Module init                                                              *
 * ------------------------------------------------------------------------ */
static PyMethodDef exactcovermethods[] = {
    { NULL }
};

PyMODINIT_FUNC initexactcover(void)
{
    PyObject *module = Py_InitModule3("exactcover", exactcovermethods,
                                      exactcover__doc__);
    if (!module)
        return;

    if (PyType_Ready(&coverings_Type) < 0)
        return;

    Py_INCREF(&coverings_Type);
    if (PyModule_AddObject(module,
                           "coverings", (PyObject *)&coverings_Type) < 0)
        return;
}
