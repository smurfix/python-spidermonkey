/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#ifndef PYSM_CONTEXT_H
#define PYSM_CONTEXT_H

#include <Python.h>
#include "structmember.h"

#include "spidermonkey.h"

typedef struct {
    PyObject_HEAD
    Runtime* rt;

    // Whether a weak or strong global object is passed in depends upon whether
    // it is possible to take weak references of the passed object.  If the
    // global cannot be referenced weakly AND if wrapped JS objects appear inside it,
    // memory leaks generally occur because the python wrappers require the JS context,
    // and vice-versa.  The right way to do this is to make the Context garbage-collectable
    // and have the global object as a contained item.  Future note.
    PyObject* weakglobal;
    PyObject* strongglobal;

    PyObject* access;
    PyObject* err_reporter;
    JSContext* cx;
    JSObject* root;
    PyDictObject* classes;
    PySetObject* objects;
    PySetObject* root_objects;
    uint32 branch_count;
    long max_heap;
    time_t max_time;
    time_t start_time;
} Context;

PyObject* Context_get_class(Context* cx, const char* key);
int Context_add_class(Context* cx, const char* key, PyObject* val);

int Context_has_access(Context*, JSContext*, PyObject*, PyObject*);

int Context_add_object(Context* cx, PyObject* val);

extern PyTypeObject _ContextType;

#endif
