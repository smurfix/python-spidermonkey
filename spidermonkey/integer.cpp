/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"

jsval
py2js_integer(Context* cx, PyObject* obj)
{
    long pyval;
    
    if(PyInt_Check(obj))
    {
        pyval = PyInt_AsLong(obj);
        if(PyErr_Occurred()) return JSVAL_VOID;
    }
    else
    {
        pyval = PyLong_AsLong(obj);
        if(PyErr_Occurred()) return JSVAL_VOID;
    }

    return long2js_integer(cx, pyval);
}

jsval
long2js_integer(Context* cx, long pyval)
{
    jsval ret = JSVAL_VOID;

    ret = INT_TO_JSVAL(pyval);

    return ret;
}

PyObject*
js2py_integer(Context* cx, jsval val)
{
    int32_t rval;

    if(!JS_ValueToInt32(cx->cx, val, &rval))
    {
        PyErr_SetString(PyExc_TypeError, "Invalid JS integer value.");
        return NULL;
    }

    return PyInt_FromLong(rval);
}
