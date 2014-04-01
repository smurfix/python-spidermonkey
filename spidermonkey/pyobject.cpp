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

JSBool
js_del_prop(JSContext* jscx, JS::HandleObject jsobj, JS::HandleId keyid, JSBool *succeeded)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* pykey = NULL;
    JSBool ret = JS_FALSE;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get JS Context.");
        goto error;
    }
    
    pyobj = get_py_obj(jsobj);
    if(pyobj == NULL) goto error;
    
    pykey = js2py(pycx, key);
    if(pykey == NULL) goto error;

    if(Context_has_access(pycx, jscx, pyobj, pykey) <= 0) goto error;

    if(PyObject_DelItem(pyobj, pykey) < 0)
    {
        PyErr_Clear();
        if(PyObject_DelAttr(pyobj, pykey) < 0)
        {
            PyErr_Clear();
            *succeeded = FALSE;
        }
    }
   
    ret = JS_TRUE;
    goto success;
    
error:
success:
    Py_XDECREF(pykey);
    return ret;
}

JSBool js_get_prop(JSContext* jscx, JS::HandleObject jsobj, JS::HandleId keyid, JS::MutableHandleValue rval)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* pykey = NULL;
    PyObject* utf8 = NULL;
    PyObject* pyval = NULL;
    JSBool ret = JS_FALSE;
    const char* data;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to get JS Context.");
        goto done;
    }
    
    pyobj = get_py_obj(jsobj);
    if(pyobj == NULL) goto done;
    
    pykey = js2py(pycx, key);
    if(pykey == NULL) goto done;

    if(Context_has_access(pycx, jscx, pyobj, pykey) <= 0) goto done;
    
    // Yeah. It's ugly as sin.
    if(PyString_Check(pykey) || PyUnicode_Check(pykey))
    {
        utf8 = PyUnicode_AsUTF8String(pykey);
        if(utf8 == NULL) goto done;

        data = PyString_AsString(utf8);
        if(data == NULL) goto done;

        if(strcmp("__iterator__", data) == 0)
        {
            if(!new_py_iter(pycx, pyobj, rval)) goto done;
            if (!rval.isUndefined())
            {
                ret = JS_TRUE;
                goto done;
            }
        }
    }

    pyval = PyObject_GetItem(pyobj, pykey);
    if(pyval == NULL)
    {
        PyErr_Clear();
        pyval = PyObject_GetAttr(pyobj, pykey);
        if(pyval == NULL)
        {
            PyErr_Clear();
            ret = JS_TRUE;
            rval.setUndefined();
            goto done;
        }
    }

    rval.set(py2js(pycx, pyval));
    if(rval.isUndefined()) goto done;
    ret = JS_TRUE;

done:
    Py_XDECREF(pykey);
    Py_XDECREF(pyval);
    Py_XDECREF(utf8);

    return ret;
}

JSBool
js_set_prop(JSContext* jscx, JS::HandleObject jsobj, JS::HandleId keyid, JSBool strict, JS::MutableHandleValue rval)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* pykey = NULL;
    PyObject* pyval = NULL;
    JSBool ret = JS_FALSE;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);
    
    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to find a Python Context.");
        goto error;
    }
    
    pyobj = get_py_obj(jsobj);
    if(pyobj == NULL)
    {
        JS_ReportError(jscx, "Failed to find a Python object.");
        goto error;
    }
    
    pykey = js2py(pycx, key);
    if(pykey == NULL)
    {
        JS_ReportError(jscx, "Failed to convert key to Python.");
        goto error;
    }

    if(Context_has_access(pycx, jscx, pyobj, pykey) <= 0) goto error;

    pyval = js2py(pycx, rval);
    if(pyval == NULL)
    {
        JS_ReportError(jscx, "Failed to convert value to Python.");
        goto error;
    }

    if(PyObject_SetItem(pyobj, pykey, pyval) < 0)
    {
        PyErr_Clear();
        if(PyObject_SetAttr(pyobj, pykey, pyval) < 0) goto error;
    }

    ret = JS_TRUE;
    goto success;
    
error:
success:
    Py_XDECREF(pykey);
    Py_XDECREF(pyval);
    return ret;
}

void js_finalize(JSFreeOp*, JSObject* jsobj)
{
    PyObject* pyobj = NULL;

    pyobj = get_py_obj(jsobj);
    Py_DECREF(pyobj);
}

PyObject*
mk_args_tuple(Context* pycx, JSContext* jscx, unsigned argc, jsval* argv)
{
    PyObject* tpl = NULL;
    PyObject* tmp = NULL;
    unsigned idx;
    
    tpl = PyTuple_New(argc);
    if(tpl == NULL)
    {
        JS_ReportError(jscx, "Failed to build args value.");
        goto error;
    }
    
    for(idx = 0; idx < argc; idx++)
    {
        tmp = js2py(pycx, argv[idx]);
        if(tmp == NULL) goto error;
        PyTuple_SET_ITEM(tpl, idx, tmp);
    }

    goto success;

error:
    Py_XDECREF(tpl);
success:
    return tpl;
}

JSBool
js_call(JSContext* jscx, unsigned argc, jsval* vp)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* tpl = NULL;
    PyObject* ret = NULL;
    PyObject* attrcheck = NULL;
    JSBool jsret = JS_FALSE;
    jsval rval;
    jsval *argv = JS_ARGV(jscx, vp);
    jsval funcobj = JS_CALLEE(jscx, vp);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to get Python context.");
        goto error;
    }
    
    pyobj = get_py_obj(JSVAL_TO_OBJECT(funcobj));
    
    if(!PyCallable_Check(pyobj))
    {
        JS_ReportError(jscx, "Object not callable, unable to apply");
        goto error;
    }

    // Use '__call__' as a notice that we want to execute a function.
    attrcheck = PyString_FromString("__call__");
    if(attrcheck == NULL) goto error;

    if(Context_has_access(pycx, jscx, pyobj, attrcheck) <= 0) goto error;

    tpl = mk_args_tuple(pycx, jscx, argc, argv);
    if(tpl == NULL) goto error;
    
    ret = PyObject_Call(pyobj, tpl, NULL);
    if(ret == NULL)
    {
        if(!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, "Failed to call object.");
        }
        JS_ReportError(jscx, "Failed to call object.");
        goto error;
    }
    
    rval = py2js(pycx, ret);
    JS_SET_RVAL(jscx, vp, rval);

    if(JSVAL_IS_VOID(rval))
    {
        JS_ReportError(jscx, "Failed to convert Python return value.");
        goto error;
    }

    jsret = JS_TRUE;
    goto success;

error:
success:
    Py_XDECREF(tpl);
    Py_XDECREF(ret);
    Py_XDECREF(attrcheck);
    return jsret;
}

JSBool
js_ctor(JSContext* jscx, unsigned argc, jsval* vp)
{
    Context* pycx = NULL;
    PyObject* pyobj = NULL;
    PyObject* tpl = NULL;
    PyObject* ret = NULL;
    PyObject* attrcheck = NULL;
    JSBool jsret = JS_FALSE;
    jsval *argv = JS_ARGV(jscx, vp);
    jsval funcobj = JS_CALLEE(jscx, vp);
    jsval rval;

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to get Python context.");
        goto error;
    }
    
    pyobj = get_py_obj(JSVAL_TO_OBJECT(funcobj));
    
    if(!PyCallable_Check(pyobj))
    {
        JS_ReportError(jscx, "Object not callable, unable to construct");
        goto error;
    }

    if(!PyType_Check(pyobj))
    {
        PyErr_SetString(PyExc_TypeError, "Object is not a Type object.");
        goto error;
    }

    // Use '__init__' to signal use as a constructor.
    attrcheck = PyString_FromString("__init__");
    if(attrcheck == NULL) goto error;

    if(Context_has_access(pycx, jscx, pyobj, attrcheck) <= 0) goto error;

    tpl = mk_args_tuple(pycx, jscx, argc, argv);
    if(tpl == NULL) goto error;
    
    ret = PyObject_CallObject(pyobj, tpl);
    if(ret == NULL)
    {
        JS_ReportError(jscx, "Failed to construct object.");
        goto error;
    }
    
    rval = py2js(pycx, ret);
    if(JSVAL_IS_VOID(rval))
    {
        JS_ReportError(jscx, "Failed to convert Python return value.");
        goto error;
    }

    JS_SET_RVAL(jscx, vp, rval);

    jsret = JS_TRUE;
    goto success;

error:
success:
    Py_XDECREF(tpl);
    Py_XDECREF(ret);
    return jsret;
}

void jsclass_finalizer(void *data)
{
    JSClass *jsc = (JSClass *)data;
    free((void*)jsc->name);
    free(jsc);
}

JSClass*
create_class(Context* cx, PyObject* pyobj)
{
    PyObject* curr = NULL;
    JSClass* jsclass = NULL;
    JSClass* ret = NULL;
    char* classname = NULL;
    int flags = JSCLASS_HAS_RESERVED_SLOTS(1);

    curr = Context_get_class(cx, pyobj->ob_type->tp_name);
    if(curr != NULL) return (JSClass*) HashCObj_AsVoidPtr(curr);

    jsclass = (JSClass*) calloc(1, sizeof(JSClass));
    if(jsclass == NULL)
    {
        PyErr_NoMemory();
        goto error;
    }
   
    classname = (char*) malloc(strlen(pyobj->ob_type->tp_name)+sizeof(char));
    if(classname == NULL)
    {
        PyErr_NoMemory();
        goto error;
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
    if(curr == NULL) goto error;
    if(Context_add_class(cx, pyobj->ob_type->tp_name, curr) < 0) goto error;

    ret = jsclass;
    goto success;

error:
    if(jsclass != NULL) free(jsclass);
    if(classname != NULL) free(classname);
success:
    return ret;
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

jsval
py2js_object(Context* cx, PyObject* pyobj)
{
    JSClass* klass = NULL;
    JSObject* jsobj = NULL;
    jsval pyval;
    jsval ret = JSVAL_VOID;
   
    Py_INCREF(pyobj);	/* This INCREF gets released by js_finalize */

    klass = create_class(cx, pyobj);
    if(klass == NULL) goto error;

    jsobj = JS_NewObject(cx->cx, klass, NULL, NULL);
    if(jsobj == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create JS object.");
        goto error;
    }

    pyval = PRIVATE_TO_JSVAL(pyobj);
    JS_SetReservedSlot(jsobj, 0, pyval);

    if(Context_add_object(cx, pyobj) < 0)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to store reference.");
        goto error;
    }

    ret = OBJECT_TO_JSVAL(jsobj);
    goto success;

error:
    Py_XDECREF(pyobj);
success:
    return ret;
}


