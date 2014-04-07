/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"

/*
    This is a fairly unsafe operation in so much as
    I'm relying on JavaScript to never call one of
    our callbacks on an object we didn't create.

    Also, of note, we're not incref'ing the Python
    object.
*/
PyObject*
get_py_obj(JSObject* obj)
{
    jsval priv = JS_GetReservedSlot(obj, 0);
    return (PyObject*) JSVAL_TO_PRIVATE(priv);
}

JSBool js_del_prop(JSContext* jscx, JS::HandleObject jsobj, JS::HandleId keyid, JSBool *succeeded)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    PSM_GET_PRIVATE_CONTEXT(pycx, jscx, JS_FALSE);
    
    pyobj = get_py_obj(jsobj);
    if (pyobj == NULL) 
	return JS_FALSE;
    
    CPyAutoObject pykey(js2py(pycx, key));

    if (pykey.isNull())
	return JS_FALSE;

    if (Context_has_access(pycx, jscx, pyobj, pykey) <= 0)
	return JS_FALSE;

    if (PyObject_DelItem(pyobj, pykey) < 0) {
        PyErr_Clear();
        if (PyObject_DelAttr(pyobj, pykey) < 0) {
            PyErr_Clear();
            *succeeded = FALSE;
        }
    }
   
    return JS_TRUE;
}

JSBool js_get_prop(JSContext* jscx, JS::HandleObject jsobj, JS::HandleId keyid, JS::MutableHandleValue rval)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    const char* data;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    PSM_GET_PRIVATE_CONTEXT(pycx, jscx, JS_FALSE);
    
    pyobj = get_py_obj(jsobj);
    if (pyobj == NULL) 
	return JS_FALSE;
    
    CPyAutoObject pykey(js2py(pycx, key));
    if (pykey.isNull())
	return JS_FALSE;

    if (Context_has_access(pycx, jscx, pyobj, pykey) <= 0) 
	return JS_FALSE;
    
    // Yeah. It's ugly as sin. 
    // (garyw: This is an old comment from somewhere.  Investigate.)

    if (PyString_Check(pykey) || PyUnicode_Check(pykey)) {
        CPyAutoObject utf8(PyUnicode_AsUTF8String(pykey));
        if (utf8.isNull())
	    return JS_FALSE;

        data = PyString_AsString(utf8);
        if (data == NULL) 
	    return JS_FALSE;

        if (strcmp("iterator", data) == 0) {
            if (!new_py_iter(pycx, pyobj, rval, TRUE)) // use for-of style
		return JS_FALSE;
            if (!rval.isUndefined())
		return JS_TRUE;
	}

        if (strcmp("__iterator__", data) == 0) {
            if (!new_py_iter(pycx, pyobj, rval, FALSE)) // use for-in style
		return JS_FALSE;
            if (!rval.isUndefined())
		return JS_TRUE;
	}
    }

    CPyAutoObject pyval(PyObject_GetItem(pyobj, pykey));

    if (pyval.isNull()) {
        PyErr_Clear();
        pyval = PyObject_GetAttr(pyobj, pykey);
        if (pyval.isNull()) {
            PyErr_Clear();
            rval.setUndefined();
	    return JS_TRUE;
	}
    }

    rval.set(py2js(pycx, pyval));
    if(rval.isUndefined())
	return JS_FALSE;

    return JS_TRUE;
}

JSBool js_set_prop(JSContext* jscx, JS::HandleObject jsobj, JS::HandleId keyid, JSBool strict, 
		   JS::MutableHandleValue rval)
{
    Context* pycx = NULL;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);
    
    PSM_GET_PRIVATE_CONTEXT(pycx, jscx, JS_FALSE);
    
    PyObject *pyobj = get_py_obj(jsobj);
    if (pyobj == NULL) {
        JS_ReportError(jscx, "Failed to find a Python object.");
	return JS_FALSE;
    }
    
    CPyAutoObject pykey(js2py(pycx, key));
    if (pykey.isNull()) {
        JS_ReportError(jscx, "Failed to convert key to Python.");
	return JS_FALSE;
    }

    if (Context_has_access(pycx, jscx, pyobj, pykey) <= 0) 
	return JS_FALSE;

    CPyAutoObject pyval(js2py(pycx, rval));
    if (pyval == NULL) {
        JS_ReportError(jscx, "Failed to convert value to Python.");
	return JS_FALSE;
    }

    if (PyObject_SetItem(pyobj, pykey, pyval) < 0) {
        PyErr_Clear();
        if (PyObject_SetAttr(pyobj, pykey, pyval) < 0)
	    return JS_FALSE;
    }

    return JS_TRUE;
}

void js_finalize(JSFreeOp*, JSObject* jsobj)
{
    PyObject* pyobj = NULL;

    pyobj = get_py_obj(jsobj);
    Py_DECREF(pyobj);
}

PyObject* mk_args_tuple(Context* pycx, JSContext* jscx, unsigned argc, jsval* argv)
{
    PyObject* tmp = NULL;
    unsigned idx;
    
    CPyAutoObject tpl(PyTuple_New(argc));
    if (tpl.isNull()) {
        JS_ReportError(jscx, "Failed to build args value.");
	return NULL;
    }
    
    for (idx = 0; idx < argc; idx++) {
        tmp = js2py(pycx, argv[idx]);
        if (!tmp)
	    return NULL;
        PyTuple_SET_ITEM((PyObject*)tpl, idx, tmp);
    }

    return tpl.asNew();
}

JSBool js_call(JSContext* jscx, unsigned argc, jsval* vp)
{
    Context* pycx = NULL;
    jsval *argv = JS_ARGV(jscx, vp);
    jsval funcobj = JS_CALLEE(jscx, vp);

    PSM_GET_PRIVATE_CONTEXT(pycx, jscx, JS_FALSE);

    PyObject *pyobj = get_py_obj(JSVAL_TO_OBJECT(funcobj));
    
    if(!PyCallable_Check(pyobj)) {
        JS_ReportError(jscx, "Object not callable, unable to apply");
	return JS_FALSE;
    }

    // Use '__call__' as a notice that we want to execute a function.
    CPyAutoObject attrcheck(PyString_FromString("__call__"));
    if (attrcheck.isNull())
	return JS_FALSE;

    if (Context_has_access(pycx, jscx, pyobj, attrcheck) <= 0) 
	return JS_FALSE;

    CPyAutoObject tpl(mk_args_tuple(pycx, jscx, argc, argv));
    if (tpl.isNull())
	return JS_FALSE;
    
    CPyAutoObject ret(PyObject_Call(pyobj, tpl, NULL));
    if (ret.isNull()){
	if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to call object.");
        }
        JS_ReportError(jscx, "Failed to call object.");
	return JS_FALSE;
    }
    
    jsval rval = py2js(pycx, ret);
    JS_SET_RVAL(jscx, vp, rval);

    if (JSVAL_IS_VOID(rval)) {
        JS_ReportError(jscx, "Failed to convert Python return value.");
	return JS_FALSE;
    }

    return JS_TRUE;
}

JSBool js_ctor(JSContext* jscx, unsigned argc, jsval* vp)
{
    Context* pycx = NULL;
    jsval *argv = JS_ARGV(jscx, vp);
    jsval funcobj = JS_CALLEE(jscx, vp);
    jsval rval;

    PSM_GET_PRIVATE_CONTEXT(pycx, jscx, JS_FALSE);
    
    PyObject *pyobj = get_py_obj(JSVAL_TO_OBJECT(funcobj));
    if (!PyCallable_Check(pyobj)) {
        JS_ReportError(jscx, "Object not callable, unable to construct");
	return JS_FALSE;
    }

    if(!PyType_Check(pyobj)) {
        PyErr_SetString(PyExc_TypeError, "Object is not a Type object.");
	return JS_FALSE;
    }

    // Use '__init__' to signal use as a constructor.
    CPyAutoObject attrcheck(PyString_FromString("__init__"));
    if (attrcheck.isNull())
	return JS_FALSE;

    if (Context_has_access(pycx, jscx, pyobj, attrcheck) <= 0) 
	return JS_FALSE;

    CPyAutoObject tpl(mk_args_tuple(pycx, jscx, argc, argv));
    if (tpl.isNull())
	return JS_FALSE;
    
    CPyAutoObject ret(PyObject_CallObject(pyobj, tpl));
    if (ret.isNull()) {
        JS_ReportError(jscx, "Failed to construct object.");
	return JS_FALSE;
    }
    
    rval = py2js(pycx, ret);
    if (JSVAL_IS_VOID(rval)) {
        JS_ReportError(jscx, "Failed to convert Python return value.");
	return JS_FALSE;
    }

    JS_SET_RVAL(jscx, vp, rval);

    return JS_TRUE;
}

void jsclass_finalizer(void *data)
{
    JSClass *jsc = (JSClass *)data;
    free((void*)jsc->name);
    free(jsc);
}

JSClass* create_class(Context* cx, PyObject* pyobj)
{
    PyObject* curr = NULL;
    int flags = JSCLASS_HAS_RESERVED_SLOTS(1);

    curr = Context_get_class(cx, pyobj->ob_type->tp_name);
    if (curr != NULL) 
	return (JSClass*) HashCObj_AsVoidPtr(curr);

    CPyAutoFreeJSClassPtr jsclass((JSClass*) calloc(1, sizeof(JSClass)));
    if (jsclass.isNull()) {
        PyErr_NoMemory();
	return NULL;
    }
   
    CPyAutoFreeCharPtr classname((char*) malloc(strlen(pyobj->ob_type->tp_name)+sizeof(char)));
    if (classname.isNull()) {
        PyErr_NoMemory();
	return NULL;
    }
    
    strcpy((char*) classname, pyobj->ob_type->tp_name);
    jsclass->name = classname;
    
    jsclass->flags = flags;
    jsclass->addProperty = JS_PropertyStub;
    jsclass->delProperty = js_del_prop;
    jsclass->getProperty = js_get_prop;
    jsclass->setProperty = js_set_prop;
    jsclass->enumerate = JS_EnumerateStub;
    jsclass->resolve = JS_ResolveStub;
    jsclass->convert = JS_ConvertStub;
    jsclass->finalize = js_finalize;

    /* Optional members (rest are null due to calloc()). */

    jsclass->call = js_call;
    jsclass->construct = js_ctor;
    
    curr = HashCObj_FromVoidPtr(jsclass, jsclass_finalizer);
    if (curr == NULL) 
	return NULL;

    if (Context_add_class(cx, pyobj->ob_type->tp_name, curr) < 0) 
	return NULL;

    classname.steal();
    return jsclass.steal();
}

PyObject* unwrap_pyobject(jsval val)
{
    PyObject* ret = NULL;
    JSClass* klass = NULL;
    JSObject* obj = NULL;

    obj = JSVAL_TO_OBJECT(val);
    klass = JS_GetClass(obj);

    if (klass->finalize == js_finalize)
    {
	ret = get_py_obj(obj);
	Py_INCREF(ret);
    }
    return ret;
}

jsval py2js_object(Context* cx, PyObject* pyobj)
{
    JSClass* klass = NULL;
    JSObject* jsobj = NULL;
    jsval pyval;
   
    klass = create_class(cx, pyobj);
    if (klass == NULL) 
	return JSVAL_VOID;

    jsobj = JS_NewObject(cx->cx, klass, NULL, NULL);
    if (jsobj == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create JS object.");
	return JSVAL_VOID;
    }

    if (Context_add_object(cx, pyobj) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to store reference.");
	return JSVAL_VOID;
    }

    Py_INCREF(pyobj);	/* This INCREF gets released by js_finalize */
    
    pyval = PRIVATE_TO_JSVAL(pyobj);
    JS_SetReservedSlot(jsobj, 0, pyval);

    return OBJECT_TO_JSVAL(jsobj);
}


