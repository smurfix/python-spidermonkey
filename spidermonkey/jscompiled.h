/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#ifndef PYSM_JSCOMPILED_H
#define PYSM_JSCOMPILED_H

/*
    This is a representation of a JavaScript
    Compiled in Python land.
*/

#include <Python.h>
#include "structmember.h"

#include "spidermonkey.h"

typedef struct {
    PyObject_HEAD
    Context* cx;
    JSScript* sobj;
} Compiled;

extern PyTypeObject _CompiledType;

PyObject* Compiled_Wrap(Context* cx, JSScript* obj);

#endif
