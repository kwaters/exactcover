#include "Python.h"

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

#if 0
void check_row(Element *row)
{
    Element *e = row;
    do {
        assert(e->right->left == e);
        assert(e->left->right == e);
        e = e->right;
    } while (e != row);
}

void check_column(Header *column)
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

void check(Header *corner)
{
    Element *e;

    assert(corner->e.up == &corner->e);
    assert(corner->e.down == &corner->e);

    for (e = corner->e.right; e != &corner->e; e = e->right)
    {
        check_column((Header*)e);
    }
}
#endif

/* ------------------------------------------------------------------------ *
 * Sparse Matrix Representation                                             *
 * ------------------------------------------------------------------------ */

/* Remove a column and all rows with a '1' in that column from the matrix. */
static void unlink_column(Header *column)
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
static void unlink_row(Element *row)
{
    Element *e = row;
    do {
        unlink_column(e->column);
        e = e->right;
    } while (e != row);
}

/* Put a column back into the matrix.  Must be called in the exact reverse
 * order of unlink_column(). */
static void link_column(Header *column)
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
static void link_row(Element *row)
{
    Element *e = row->left;
    do {
        link_column(e->column);
        e = e->left;
    } while (e != row->left);
}

/* Return the header for the column with the fewest '1's.  Returns NULL if
 * there are no columns in the matrix */
static Header *smallest_column(Header *corner)
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
static int column_count(Header *corner)
{
    int count = 0;
    Element *e = corner->e.right;
    for (; e != &corner->e; e = e->right) {
        count += 1;
    }
    return count;
}

/* Finds or inserts a column, returns NULL on comparison failure. */
static Header *find_column(Header *corner, PyObject *object)
{
    Header *i;

    /* Linear scan of the universe for this object */
    for (i = (Header *)corner->e.right; i != corner; i = (Header *)i->e.right) {
        int cmp = PyObject_RichCompareBool(i->object, object, Py_EQ);
        if (cmp == -1) {
            return NULL;
        } else if (cmp == 1) {
            return i;
        }
    }

    /* New header element. */
    i = PyMem_New(Header, 1);
    /* TODO: allocation failure */
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

/* Free a matrix */
static void free_matrix(Header *corner)
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
 * coveringsiter class                                                      *
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
} coveringsiterobject;

/* .tp_dealloc */
static void coveringsiter_dtor(coveringsiterobject *self)
{
    /* restore matrix */
    while (--self->solutionSize >= 0)
        link_row(self->solution[self->solutionSize]);

    free_matrix(self->corner);
    PyMem_Del(self->solution);
    PyObject_Del(self);
}

/* Make one solving step, returns an Action.
 *
 * CONTINUE = Some of the universe is uncovered, call this function again.
 * BACKUP = The remaining universe cannot be covered, backtrack then call this
 *          function again.
 * SOLUTION = solution contains a covering set. */
static int coveringsiter_step(coveringsiterobject *self)
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
    // check(self->corner);
    self->solution[self->solutionSize] = row;
    self->solutionSize++;

    return CONTINUE;
}

#if 0
#include <stdio.h>
void show(Header *corner)
{
    Header *column = (Header *)corner->e.right;
    for (; column != corner; column = (Header *)column->e.right) {
        printf("%s(%d) ", PyString_AS_STRING(PyObject_Repr(column->object)),
               column->count);
    }
    printf("\n");

    column = (Header *)corner->e.right;
    for (; column != corner; column = (Header *)column->e.right) {
        Element *row;
        printf("Column %s:\n", PyString_AS_STRING(PyObject_Repr(column->object)));
        for (row = column->e.down; row != &column->e; row = row->down) {
            show_row(row);
        }
    }
}

void show_column(Header *column)
{
    printf("%s\n", PyString_AS_STRING(PyObject_Repr(column->object)));
}

void show_row(Element *row)
{
    Element *e;

    printf("row");
    e = row;
    do {
        printf(" %s", PyString_AS_STRING(PyObject_Repr(e->column->object)));
        e = e->right;
    } while (e != row);
    printf("\n");
}
#endif

/* backtrack.  Returns -1 if there are no more solutions */
static int coveringsiter_backup(coveringsiterobject *self)
{
    while (self->solutionSize > 0) {
        Element *row = self->solution[self->solutionSize - 1];
        link_row(row);
        // check(self->corner);
        row = row->down;
        if (row == &row->column->e) {
            self->solutionSize--;
        } else {
            unlink_row(row);
            // check(self->corner);
            self->solution[self->solutionSize - 1] = row;
            return 0;
        }
    }
    return -1;
}

/* Create a tuple of the current solution stack. */
static PyObject *coveringsiter_solution(coveringsiterobject *self)
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

/* .tp_iternext */
static PyObject *coveringsiter_next(coveringsiterobject *self)
{
    // check(self->corner);

    /* We need to backup from the last solution on every new iteration. */
    if (self->first) {
        self->first = 0;
    } else if (coveringsiter_backup(self) < 0) {
        return NULL;
    }

    for (;;) {
        int action = coveringsiter_step(self);
        switch (action) {
        case CONTINUE:
            break;

        case BACKUP:
            if (coveringsiter_backup(self) < 0)
                return NULL;
            break;

        case SOLUTION:
            return coveringsiter_solution(self);
        }
    }

    /* not reached. */
    return NULL;
}

static PyTypeObject coveringsiter_Type = {
    PyObject_HEAD_INIT(NULL)
    .tp_name = "converingsiter",
    .tp_basicsize = sizeof(coveringsiterobject),
    .tp_dealloc = (destructor)coveringsiter_dtor,
    .tp_hash = PyObject_HashNotImplemented,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "doc",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc)coveringsiter_next
};


/* ------------------------------------------------------------------------ *
 * coverings function                                                       *
 * ------------------------------------------------------------------------ */
static PyObject *coverings(PyObject *module, PyObject *covers)
{
    PyObject *cover = NULL;
    PyObject *coverIt = NULL;
    PyObject *elem = NULL;
    PyObject *it = NULL;

    coveringsiterobject *coveringsiter = NULL;

    Header *corner = PyMem_New(Header, 1);
    if (!corner) {
        PyErr_NoMemory();
        goto error;
    }
    corner->e.up = &corner->e;
    corner->e.down = &corner->e;
    corner->e.left = &corner->e;
    corner->e.right = &corner->e;
    corner->e.column = corner;
    corner->e.object = NULL;
    corner->count = 0;
    corner->object = NULL;

    if (!(coverIt = PyObject_GetIter(covers)))
        goto error;
    while ((cover = PyIter_Next(coverIt))) {
        Element *row = NULL;

        if (!(it = PyObject_GetIter(cover)))
            goto error;
        while ((elem = PyIter_Next(it))) {
            Element *e = NULL;
            Header *column = find_column(corner, elem);
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

    coveringsiter = PyObject_New(coveringsiterobject, &coveringsiter_Type);
    if (!coveringsiter)
        goto error;
    coveringsiter->corner = corner;
    coveringsiter->first = 1;
    coveringsiter->solution = PyMem_New(Element *, column_count(corner));
    coveringsiter->solutionSize = 0;

    if (!coveringsiter->solution) {
        /* The dtor will free corner. */
        corner = NULL;
        PyErr_NoMemory();
        goto error;
    }

    return (PyObject *)coveringsiter;

error:
    free_matrix(corner);
    Py_XDECREF(coveringsiter);
    Py_XDECREF(cover);
    Py_XDECREF(coverIt);
    Py_XDECREF(elem);
    Py_XDECREF(it);
    return NULL;
}


/* ------------------------------------------------------------------------ *
 * Module init                                                              *
 * ------------------------------------------------------------------------ */
static PyMethodDef exactcovermethods[] = {
    {
        .ml_name = "coverings",
        .ml_meth = (PyCFunction)coverings,
        .ml_flags = METH_O
    },
    { NULL }
};

PyMODINIT_FUNC initexactcover(void)
{
    PyObject *module = Py_InitModule("exactcover", exactcovermethods);
    if (!module)
        return;

    if (PyType_Ready(&coveringsiter_Type) < 0)
        return;
}
