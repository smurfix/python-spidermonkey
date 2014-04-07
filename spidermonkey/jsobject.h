/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#ifndef PYSM_JSOBJECT_H
#define PYSM_JSOBJECT_H

/*
    This is a representation of a JavaScript
    object in Python land.
*/

typedef struct {
    PyObject_HEAD
    Context* cx;
    jsval val;
    JSObject* obj;
} PJObject;

extern PyTypeObject _PJObjectType;

PyObject* make_object(PyTypeObject* type, Context* cx, jsval val);
PyObject* js2py_object(Context* cx, jsval val);

typedef CPyAuto<PJObject> CPyAutoPJObject;

#endif
