/*
 * I've found Lennart Regebro's instructions very helpful:
 *     http://python3porting.com/cextensions.html
 *
 * This file declares all sorts of macros to work around the different
 * interfaces for CPython 2 and CPython 3 extensions. These definition
 * are copyied from Lennart's zope.proxy example.
 */

 #if PY_MAJOR_VERSION >= 3
   #define MOD_ERROR_VAL NULL
   #define MOD_SUCCESS_VAL(val) val
   #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
   #define MOD_DEF(ob, name, doc, methods) \
           static struct PyModuleDef moduledef = { \
             PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
           ob = PyModule_Create(&moduledef);
 #else
   #define MOD_ERROR_VAL
   #define MOD_SUCCESS_VAL(val)
   #define MOD_INIT(name) void init##name(void)
   #define MOD_DEF(ob, name, doc, methods) \
           ob = Py_InitModule3(name, methods, doc);
 #endif
