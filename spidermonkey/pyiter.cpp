/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"

#define SLOT_PYOBJ    0
#define SLOT_ITER     1
#define SLOT_ITERFLAG 2

#define SLOT_COUNT    (SLOT_ITERFLAG+1)

PyObject*
get_js_slot(JSObject* obj, int slot)
{
    jsval priv = JS_GetReservedSlot(obj, slot);
    return (PyObject*) JSVAL_TO_PRIVATE(priv);
}

void finalize(JSFreeOp* jsfop, JSObject* jsobj)
{
    PyObject* pyobj;
    PyObject* pyiter;

    pyobj = get_js_slot(jsobj, SLOT_PYOBJ);
    Py_XDECREF(pyobj);

    pyiter = get_js_slot(jsobj, SLOT_ITER);
    Py_XDECREF(pyiter);
}

JSBool call(JSContext* jscx, unsigned argc, jsval* vp)
{
    jsval *argv = JS_ARGV(jscx, vp);
    jsval objval = JS_CALLEE(jscx, vp);

    JSObject* obj = JSVAL_TO_OBJECT(objval);

    if (argc >= 1 && JSVAL_IS_BOOLEAN(argv[0]) && !JSVAL_TO_BOOLEAN(argv[0]))
	JS_SetReservedSlot(obj, SLOT_ITERFLAG, JSVAL_TRUE);

    JS_SET_RVAL(jscx, vp, objval);

    return JS_TRUE;
}

JSBool is_for_of(JSContext* cx, JSObject* obj, JSBool* rval)
{
    jsval slot = JS_GetReservedSlot(obj, SLOT_ITERFLAG);

    if (!JSVAL_IS_BOOLEAN(slot)) return JS_FALSE;
    *rval = JSVAL_TO_BOOLEAN(slot);
    return JS_TRUE;
}

JSBool def_next(JSContext* jscx, unsigned argc, jsval* vp)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* iter = NULL;
    JSBool for_of = JS_FALSE;
    jsval rval;
    JSObject *jsthis = JSVAL_TO_OBJECT(JS_THIS(jscx, vp));

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if (pycx == NULL) {
        JS_ReportError(jscx, "Failed to get JS Context.");
	return JS_FALSE;
    }

    iter = get_js_slot(jsthis, SLOT_ITER);
    if (!PyIter_Check(iter)) {
        JS_ReportError(jscx, "Object is not an iterator.");
	return JS_FALSE;
    }

    pyobj = get_js_slot(jsthis, SLOT_PYOBJ);
    if (pyobj == NULL) {
        JS_ReportError(jscx, "Failed to find iterated object.");
	return JS_FALSE;
    }

    CPyAutoObject next(PyIter_Next(iter));
    if (next.isNull() && PyErr_Occurred()) {
	return JS_FALSE;
    } else if (next.isNull()) {
	JS_ThrowStopIteration(jscx);
	return JS_FALSE;
    }

    if (!is_for_of(jscx, jsthis, &for_of)) {
        JS_ReportError(jscx, "Failed to get iterator flag.");
	return JS_FALSE;
    }

    if (PyMapping_Check(pyobj) && for_of) {
        CPyAutoObject value(PyObject_GetItem(pyobj, next));
        if (value.isNull()) {
            JS_ReportError(jscx, "Failed to get value in 'for of'");
	    return JS_FALSE;
        }
        rval = py2js(pycx, value);
    } else {
        rval = py2js(pycx, next);
    }

    JS_SET_RVAL(jscx, vp, rval);

    return JS_TRUE;
}

JSBool seq_next(JSContext* jscx, unsigned argc, jsval* vp)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* iter = NULL;
    JSBool for_of = JS_FALSE;
    jsval rval;
    long maxval = -1;
    long currval = -1;
    jsval valthis = JS_THIS(jscx, vp);
    JSObject *jsthis = JSVAL_TO_OBJECT(valthis);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if (pycx == NULL) {
        JS_ReportError(jscx, "Failed to get JS Context.");
	return JS_FALSE;
    }

    pyobj = get_js_slot(jsthis, SLOT_PYOBJ);
    if (!PySequence_Check(pyobj)) {
        JS_ReportError(jscx, "Object is not a sequence.");
	return JS_FALSE;
    }

    maxval = PyObject_Length(pyobj);
    if (maxval < 0) 
	return JS_FALSE;

    iter = get_js_slot(jsthis, SLOT_ITER);
    if (!PyInt_Check(iter)) {
        JS_ReportError(jscx, "Object is not an integer.");
	return JS_FALSE;
    }

    currval = PyInt_AsLong(iter);
    if (currval == -1 && PyErr_Occurred()) {
	return JS_FALSE;
    }
    
    if (currval + 1 > maxval) {
	JS_ThrowStopIteration(jscx);
	return JS_FALSE;
    }

    CPyAutoObject next(PyInt_FromLong(currval + 1));
    if (next.isNull())
	return JS_FALSE;

    JS_SetReservedSlot(jsthis, SLOT_ITER, PRIVATE_TO_JSVAL(next));

    if (!is_for_of(jscx, jsthis, &for_of))
    {
        JS_ReportError(jscx, "Failed to get iterator flag.");
	return JS_FALSE;
    }

    if (for_of) {
        CPyAutoObject value(PyObject_GetItem(pyobj, iter));
        if (value.isNull()) {
            JS_ReportError(jscx, "Failed to get array element in 'for each'");
	    return JS_FALSE;
        }
        rval = py2js(pycx, value);
    } else {
        rval = py2js(pycx, iter);
    }

    next = iter;

    JS_SET_RVAL(jscx, vp, rval);

    return JS_TRUE;
}

static JSClass
js_iter_class = {
    "PyJSIteratorClass",
    JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL, // check access
    call
};

static JSFunctionSpec js_def_iter_functions[] = {
    {"next", JSOP_WRAPPER(def_next), 0, 0},
    {0, JSOP_WRAPPER(NULL), 0, 0}
};

static JSFunctionSpec js_seq_iter_functions[] = {
    {"next", JSOP_WRAPPER(seq_next), 0, 0},
    {0, JSOP_WRAPPER(NULL), 0, 0}
};

JSBool new_py_def_iter(Context* cx, PyObject* obj, JS::MutableHandleValue rval, int for_of)
{
    JSObject* jsiter = NULL;
    jsval jsv = JSVAL_VOID;

    // Initialize the return value
    rval.setUndefined();

    CPyAutoObject pyiter(PyObject_GetIter(obj));
    if (pyiter.isNull()) {
        if(PyErr_GivenExceptionMatches(PyErr_Occurred(), PyExc_TypeError)) {
            PyErr_Clear();
	    return JS_TRUE;
        } else {
	    return JS_FALSE;
	}
    }

    jsiter = JS_NewObject(cx->cx, &js_iter_class, NULL, NULL);
    if (jsiter == NULL)
	return JS_FALSE;

    if (!JS_DefineFunctions(cx->cx, jsiter, js_def_iter_functions)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to define iter funcions.");
	return JS_FALSE;
    }

    Py_INCREF(obj);
    jsv = PRIVATE_TO_JSVAL(obj);
    JS_SetReservedSlot(jsiter, SLOT_PYOBJ, jsv);

    jsv = PRIVATE_TO_JSVAL(pyiter.asNew());
    JS_SetReservedSlot(jsiter, SLOT_ITER, jsv);

    JS_SetReservedSlot(jsiter, SLOT_ITERFLAG, for_of? JSVAL_TRUE : JSVAL_FALSE);

    rval.setObject(*jsiter);

    return JS_TRUE;
}

JSBool new_py_seq_iter(Context* cx, PyObject* obj, JS::MutableHandleValue rval, int for_of)
{
    JSObject* jsiter = NULL;
    jsval jsv = JSVAL_VOID;

    // Initialize the return value
    rval.setUndefined();

    // Our counting state
    CPyAutoObject pyiter(PyInt_FromLong(0));
    if (pyiter.isNull())
	return JS_FALSE;

    jsiter = JS_NewObject(cx->cx, &js_iter_class, NULL, NULL);
    if (jsiter == NULL) 
	return JS_FALSE;

    if (!JS_DefineFunctions(cx->cx, jsiter, js_seq_iter_functions)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to define iter funcions.");
	return JS_FALSE;
    }

    Py_INCREF(obj);
    jsv = PRIVATE_TO_JSVAL(obj);
    JS_SetReservedSlot(jsiter, SLOT_PYOBJ, jsv);

    jsv = PRIVATE_TO_JSVAL(pyiter.asNew());
    JS_SetReservedSlot(jsiter, SLOT_ITER, jsv);

    JS_SetReservedSlot(jsiter, SLOT_ITERFLAG, for_of? JSVAL_TRUE : JSVAL_FALSE);

    rval.setObject(*jsiter);

    return JS_TRUE;
}

JSBool new_py_iter(Context* cx, PyObject* obj, JS::MutableHandleValue rval, int for_of)
{
    if (PySequence_Check(obj))
        return new_py_seq_iter(cx, obj, rval, for_of);

    return new_py_def_iter(cx, obj, rval, for_of);
}
