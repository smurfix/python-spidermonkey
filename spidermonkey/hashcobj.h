/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#ifndef PYSM_HASHCOBJ_H
#define PYSM_HASHCOBJ_H

/*
    A class to implement Python hashing of C pointers.
*/

typedef void (*HASHCOBJ_FINALIZER)(void *data);

typedef struct {
    PyObject_HEAD
    void* cobj;
    HASHCOBJ_FINALIZER finalizer;
} HashCObj;

extern PyTypeObject _HashCObjType;


/*
    BIG FUCKING NOTE: This constructor never
    Py_INCREF's the returned object.
*/
PyObject* HashCObj_FromVoidPtr(void *cobj, HASHCOBJ_FINALIZER finalizer);
void* HashCObj_AsVoidPtr(PyObject* self);

#endif
