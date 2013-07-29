/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"

#include <time.h> // After spidermonkey.h so after Python.h

//#include <jsobj.h>
//#include <jscntxt.h>

// Forward decl for add_prop
JSBool set_prop(JSContext* jscx, JSObject* jsobj, jsid keyid, JSBool strict, jsval* rval);

PyObject* get_cxglobal(Context* self)
{
    PyObject* ret = self->strongglobal;

    if (ret == NULL && (self->weakglobal == NULL || (ret = PyWeakref_GetObject(self->weakglobal)) == Py_None))
	return NULL;

    // Must be certain to decref the global, since anything may cause it to disappear.
    Py_INCREF(ret);
    return ret;
}

JSBool add_prop(JSContext* jscx, JSObject* jsobj, jsid keyid, jsval* rval)
{
    JSObject* obj = NULL;

    if(JSVAL_IS_NULL(*rval) || !JSVAL_IS_OBJECT(*rval)) return JS_TRUE;

    obj = JSVAL_TO_OBJECT(*rval);
    if(!JS_ObjectIsFunction(jscx, obj))
	return JS_TRUE;
    
    return set_prop(jscx, jsobj, keyid, JS_TRUE, rval);
}

JSBool
del_prop(JSContext* jscx, JSObject* jsobj, jsid keyid, jsval* rval)
{
    Context* pycx = NULL;
    PyObject* pykey = NULL;
    PyObject* pyval = NULL;
    PyObject* global = NULL;
    JSBool ret = JS_FALSE;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to get Python context.");
        goto done;
    }

    // Bail if there's no available global handler.
    if ((global = get_cxglobal(pycx)) == NULL)
    {
        ret = JS_TRUE;
        goto done;
    }
    
    // Check access to python land.
    if(Context_has_access(pycx, jscx, global, pykey) <= 0) goto done;

    // Bail if the global doesn't have a __delitem__
    if(!PyObject_HasAttrString(global, "__delitem__"))
    {
        ret = JS_TRUE;
        goto done;
    }

    pykey = js2py(pycx, key);
    if(pykey == NULL) goto done;

    if(PyObject_DelItem(global, pykey) < 0) goto done;

    ret = JS_TRUE;

done:
    Py_XDECREF(global);
    Py_XDECREF(pykey);
    Py_XDECREF(pyval);
    return ret;
}

JSBool
get_prop(JSContext* jscx, JSObject* jsobj, jsid keyid, jsval* rval)
{
    Context* pycx = NULL;
    PyObject* pykey = NULL;
    PyObject* pyval = NULL;
    PyObject* global = NULL;
    JSBool ret = JS_FALSE;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to get Python context.");
        goto done;
    }

    // Bail if there's no available global handler.
    if ((global = get_cxglobal(pycx)) == NULL)
    {
        ret = JS_TRUE;
        goto done;
    }

    pykey = js2py(pycx, key);
    if(pykey == NULL) goto done;

    if(Context_has_access(pycx, jscx, global, pykey) <= 0) goto done;

    pyval = PyObject_GetItem(global, pykey);
    if(pyval == NULL)
    {
        if(PyErr_GivenExceptionMatches(PyErr_Occurred(), PyExc_KeyError))
        {
            PyErr_Clear();
            ret = JS_TRUE;
        }
        goto done;
    }

    *rval = py2js(pycx, pyval);
    if(JSVAL_IS_VOID(*rval)) goto done;
    ret = JS_TRUE;

done:
    Py_XDECREF(global);
    Py_XDECREF(pykey);
    Py_XDECREF(pyval);
    return ret;
}

JSBool
set_prop(JSContext* jscx, JSObject* jsobj, jsid keyid, JSBool strict, jsval* rval)
{
    Context* pycx = NULL;
    PyObject* pykey = NULL;
    PyObject* pyval = NULL;
    PyObject* global = NULL;
    JSBool ret = JS_FALSE;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to get Python context.");
        goto done;
    }

    // Bail if there's no available global handler.
    if ((global = get_cxglobal(pycx)) == NULL)
    {
        ret = JS_TRUE;
        goto done;
    }

    pykey = js2py(pycx, key);
    if(pykey == NULL) goto done;

    if(Context_has_access(pycx, jscx, global, pykey) <= 0) goto done;

    pyval = js2py(pycx, *rval);
    if(pyval == NULL) goto done;

    if(PyObject_SetItem(global, pykey, pyval) < 0) goto done;

    ret = JS_TRUE;

done:
    Py_XDECREF(global);
    Py_XDECREF(pykey);
    Py_XDECREF(pyval);
    return ret;
}

JSBool
resolve(JSContext* jscx, JSObject* jsobj, jsid keyid)
{
    Context* pycx = NULL;
    PyObject* pykey = NULL;
    PyObject* global = NULL;
    jsid pid;
    JSBool ret = JS_FALSE;
    jsval key;

    JS_IdToValue(jscx, keyid, &key);

    pycx = (Context*) JS_GetContextPrivate(jscx);
    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to get Python context.");
        goto done;
    }

    // Bail if there's no available global handler.
    if ((global = get_cxglobal(pycx)) == NULL)
    {
        ret = JS_TRUE;
        goto done;
    }

    pykey = js2py(pycx, key);
    if(pykey == NULL) goto done;
    
    if(Context_has_access(pycx, jscx, global, pykey) <= 0) goto done;

    if(!PyMapping_HasKey(global, pykey))
    {
        ret = JS_TRUE;
        goto done;
    }

    if(!JS_ValueToId(jscx, key, &pid))
    {
        JS_ReportError(jscx, "Failed to convert property id.");
        goto done;
    }

    if(!JS_DefinePropertyById(jscx, pycx->root, pid, JSVAL_VOID, NULL, NULL,
                            JSPROP_SHARED))
    {
        JS_ReportError(jscx, "Failed to define property.");
        goto done;
    }

    ret = JS_TRUE;

done:
    Py_XDECREF(global);
    Py_XDECREF(pykey);
    return ret;
}

static JSClass
js_global_class = {
    "JSGlobalObjectClass",
    JSCLASS_GLOBAL_FLAGS,
    add_prop,
    del_prop,
    get_prop,
    set_prop,
    JS_EnumerateStub,
    resolve,
    JS_ConvertStub,
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#define MAX(a, b) ((a) > (b) ? (a) : (b))
JSBool
branch_cb(JSContext* jscx)
{
    Context* pycx = (Context*) JS_GetContextPrivate(jscx);
    time_t now = time(NULL);

    if(pycx == NULL)
    {
        JS_ReportError(jscx, "Failed to find Python context.");
        return JS_FALSE;
    }

    // Get out quick if we don't have any quotas.
    if(pycx->max_time == 0 && pycx->max_heap == 0)
    {
        return JS_TRUE;
    }

    // Only check occasionally for resource usage.
    pycx->branch_count++;
    if((pycx->branch_count > 0x3FFF) != 1)
    {
        return JS_TRUE;
    }

    pycx->branch_count = 0;

    if(pycx->max_heap > 0)
    {
	int gcbytes = JS_GetGCParameter(pycx->rt->rt, JSGC_BYTES);
	if (gcbytes > pycx->max_heap)
	{
	    // First see if garbage collection gets under the threshold.
	    JS_GC(jscx);
	    gcbytes = JS_GetGCParameter(pycx->rt->rt, JSGC_BYTES);
	    if(gcbytes > pycx->max_heap)
	    {
		PyErr_NoMemory();
		return JS_FALSE;
	    }
	}
    }

    if(
        pycx->max_time > 0
        && pycx->start_time > 0
        && pycx->max_time < now - pycx->start_time
    )
    {
        PyErr_SetNone(PyExc_SystemError);
        return JS_FALSE;
    }

    return JS_TRUE;
}

PyObject*
Context_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    Context* self = NULL;
    Runtime* runtime = NULL;
    PyObject* global = NULL;
    PyObject* weakglobal = NULL;
    PyObject* strongglobal = NULL;
    PyObject* access = NULL;
    int strict = 0;
    uint32_t jsopts;

    char* keywords[] = {"runtime", "glbl", "access", "strict", NULL};

    if(!PyArg_ParseTupleAndKeywords(
        args, kwargs,
        "O!|OOI",
        keywords,
        RuntimeType, &runtime,
        &global,
        &access,
        &strict
    )) goto error;

    if(global == Py_None) global = NULL;
    if(access == Py_None) access = NULL;
    strict &= 1;  /* clamp at 1 */

    if(global != NULL)
    {
	if (!PyMapping_Check(global))
	  {
	      PyErr_SetString(PyExc_TypeError,
			      "Global handler must provide item access.");
	      goto error;
	  }

	/* If for any reason we can't create a weak reference, then make it a strong one. */

	if ((weakglobal = PyWeakref_NewRef(global, NULL)) == NULL) {
	    PyErr_Clear();
	    strongglobal = global;
	}
    }

    if(access != NULL && !PyCallable_Check(access))
    {
        PyErr_SetString(PyExc_TypeError,
                            "Access handler must be callable.");
        goto error;
    }

    self = (Context*) type->tp_alloc(type, 0);
    if(self == NULL) goto error;

    // Tracking what classes we've installed in
    // the context.
    self->classes = (PyDictObject*) PyDict_New();
    if(self->classes == NULL) goto error;

    self->objects = (PySetObject*) PySet_New(NULL);
    if(self->objects == NULL) goto error;

    self->cx = JS_NewContext(runtime->rt, 8192);
    if(self->cx == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create JSContext.");
        goto error;
    }

    JS_BeginRequest(self->cx);

    /*
     *  Notice that we don't add a ref to the Python context for
     *  the copy stored on the JSContext*. I'm pretty sure this
     *  would cause a cyclic dependancy that would prevent
     *  garbage collection from happening on either side of the
     *  bridge.
     *
     */
    JS_SetContextPrivate(self->cx, self);

    // Setup the root of the property lookup doodad.
    self->root = JS_NewCompartmentAndGlobalObject(self->cx, &js_global_class, NULL);
    if(self->root == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Error creating root object.");
        goto error;
    }

    if(!JS_InitStandardClasses(self->cx, self->root))
    {
        PyErr_SetString(PyExc_RuntimeError, "Error initializing JS VM.");
        goto error;
    }

    // Don't setup the global handler until after the standard classes
    // have been initialized.
    self->weakglobal = weakglobal;

    if (strongglobal != NULL) Py_INCREF(strongglobal);
    self->strongglobal = strongglobal;

    if(access != NULL) Py_INCREF(access);
    self->access = access;

    // Setup counters for resource limits
    self->branch_count = 0;
    self->max_time = 0;
    self->start_time = 0;
    self->max_heap = 0;

    JS_SetOperationCallback(self->cx, branch_cb);
    JS_SetErrorReporter(self->cx, report_error_cb);

    jsopts = JS_GetOptions(self->cx);
    jsopts |= JSOPTION_VAROBJFIX;
    if (strict) {
        jsopts |= JSOPTION_STRICT;
    } else {
        jsopts &= ~JSOPTION_STRICT;
    }
    JS_SetOptions(self->cx, jsopts);
    
    Py_INCREF(runtime);
    self->rt = runtime;

    goto success;

error:
    if(self != NULL && self->cx != NULL) JS_EndRequest(self->cx);
    Py_XDECREF(self);
    Py_XDECREF(weakglobal);
    Py_XDECREF(strongglobal);
    self = NULL;

success:
    if(self != NULL && self->cx != NULL) JS_EndRequest(self->cx);
    return (PyObject*) self;
}

int
Context_init(Context* self, PyObject* args, PyObject* kwargs)
{
    return 0;
}

void
Context_dealloc(Context* self)
{
    if (self->cx != NULL)
    {
        JS_DestroyContext(self->cx);
    }


    Py_CLEAR(self->err_reporter);
    Py_CLEAR(self->objects);
    Py_CLEAR(self->weakglobal);
    Py_CLEAR(self->strongglobal);
    Py_CLEAR(self->access);
    Py_CLEAR(self->classes);

    Py_XDECREF(self->rt);
}

PyObject*
Context_add_global(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* pykey = NULL;
    PyObject* pyval = NULL;
    jsval jsk;
    jsid kid;
    jsval jsv;

    JS_BeginRequest(self->cx);

    if(!PyArg_ParseTuple(args, "OO", &pykey, &pyval)) goto error;

    jsk = py2js(self, pykey);
    if(JSVAL_IS_VOID(jsk)) goto error;

    if(!JS_ValueToId(self->cx, jsk, &kid))
    {
        PyErr_SetString(PyExc_AttributeError, "Failed to create key id.");
        goto error;
    }

    jsv = py2js(self, pyval);
    if(JSVAL_IS_VOID(jsv)) goto error;

    if(!JS_SetPropertyById(self->cx, self->root, kid, &jsv))
    {
        PyErr_SetString(PyExc_AttributeError, "Failed to set global property.");
        goto error;
    }

    goto success;

error:
success:
    JS_EndRequest(self->cx);
    Py_RETURN_NONE;
}

PyObject*
Context_rem_global(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* pykey = NULL;
    PyObject* ret = NULL;
    jsval jsk;
    jsid kid;
    jsval jsv;

    JS_BeginRequest(self->cx);

    if(!PyArg_ParseTuple(args, "O", &pykey)) goto error;

    jsk = py2js(self, pykey);
    if(JSVAL_IS_VOID(jsk)) goto error;

    if(!JS_ValueToId(self->cx, jsk, &kid))
    {
        PyErr_SetString(JSError, "Failed to create key id.");
    }

    if(!JS_GetPropertyById(self->cx, self->root, kid, &jsv))
    {
        PyErr_SetString(JSError, "Failed to get global property.");
        goto error;
    }
    
    ret = js2py(self, jsv);
    if(ret == NULL) goto error;
    
    if(!JS_DeletePropertyById(self->cx, self->root, kid))
    {
        PyErr_SetString(JSError, "Failed to remove global property.");
        goto error;
    }

    JS_MaybeGC(self->cx);

    goto success;

error:
success:
    JS_EndRequest(self->cx);
    return ret;
}

PyObject*
Context_set_access(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* ret = NULL;
    PyObject* newval = NULL;

    if(!PyArg_ParseTuple(args, "|O", &newval)) goto done;
    if(newval != NULL && newval != Py_None)
    {
        if(!PyCallable_Check(newval))
        {
            PyErr_SetString(PyExc_TypeError,
                                    "Access handler must be callable.");
            ret = NULL;
            goto done;
        }
    }

    ret = self->access;

    if(newval != NULL && newval != Py_None)
    {
        Py_INCREF(newval);
        self->access = newval;
    }

    if(ret == NULL)
    {
        ret = Py_None;
        Py_INCREF(ret);
    }

done:
    return ret;
}

PyObject*
Context_execute(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* obj = NULL;
    PyObject* ret = NULL;
    JSContext* cx = NULL;
    JSObject* root = NULL;
    JSString* script = NULL;
    const jschar* schars = NULL;
    JSBool started_counter = JS_FALSE;
    char *fname = "<anonymous JavaScript>";
    unsigned int lineno = 1;
    size_t slen;
    jsval rval;

    char *keywords[] = {"code", "filename", "lineno", NULL};

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O|sI", keywords,
                                    &obj, &fname, &lineno))
	return NULL;

    JS_BeginRequest(self->cx);
    
    script = py2js_string_obj(self, obj);
    if(script == NULL) goto error;

    schars = JS_GetStringCharsZ(self->cx, script);
    slen = JS_GetStringLength(script);
    
    cx = self->cx;
    root = self->root;

    // Mark us for time consumption
    if(self->start_time == 0)
    {
        started_counter = JS_TRUE;
        self->start_time = time(NULL);
    }

    if(!JS_EvaluateUCScript(cx, root, schars, slen, fname, lineno, &rval))
    {
        if(!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, "Script execution failed and no exception was set");
        }
        goto error;
    }

    if(PyErr_Occurred()) goto error;

    ret = js2py(self, rval);

    JS_EndRequest(self->cx);
    JS_MaybeGC(self->cx);
    goto success;

error:
    JS_EndRequest(self->cx);
success:

    if(started_counter)
    {
        self->start_time = 0;
    }

    return ret;
}

PyObject*
Context_set_error_reporter(Context* self, PyObject* errfunc)
{
    if (errfunc == NULL || errfunc == Py_None) {
	Py_CLEAR(self->err_reporter);
	Py_RETURN_NONE;
    }

    if (!PyCallable_Check(errfunc)) {
	PyErr_SetString(PyExc_TypeError, "Error reporter must be a callable object.");
	return NULL;
    }

    Py_CLEAR(self->err_reporter);

    Py_INCREF(errfunc);
    self->err_reporter = errfunc;

    Py_RETURN_NONE;
}

PyObject*
Context_compile(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* obj = NULL;
    PyObject* ret = NULL;
    JSContext* jcx = NULL;
    JSObject* root = NULL;
    JSString* script = NULL;
    const jschar* schars = NULL;
    char *fname = "<anonymous compiled JavaScript>";
    unsigned int lineno = 1;
    size_t slen;
    JSObject *rvalobj;

    char *keywords[] = {"code", "filename", "lineno", NULL};

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O|sI", keywords,
                                    &obj, &fname, &lineno))
	return NULL;

    JS_BeginRequest(self->cx);
    
    script = py2js_string_obj(self, obj);
    if(script == NULL) goto error;

    schars = JS_GetStringCharsZ(self->cx, script);
    slen = JS_GetStringLength(script);
    
    jcx = self->cx;
    root = self->root;

    if(!(rvalobj = JS_CompileUCScript(jcx, root, schars, slen, fname, lineno)))
    {
        if(!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, "Script could not be compiled");
        }
        goto error;
    }

    if(PyErr_Occurred()) goto error;

    ret = Compiled_Wrap(self, rvalobj);

    JS_EndRequest(jcx);
    JS_MaybeGC(jcx);
    goto success;

error:
    JS_EndRequest(jcx);
success:

    return ret;
}

PyObject*
Context_gc(Context* self, PyObject* args, PyObject* kwargs)
{
    Py_DECREF(self->objects);
    self->objects = (PySetObject*) PySet_New(NULL);

    JS_GC(self->cx);

    Py_INCREF(self);
    return (PyObject*) self;
}

PyObject*
Context_max_memory(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* ret = NULL;
    long curr_max = -1;
    long new_max = -1;

    if(!PyArg_ParseTuple(args, "|l", &new_max)) goto done;

    curr_max = self->max_heap;
    if(new_max >= 0) self->max_heap = new_max;

    ret = PyLong_FromLong(curr_max);

done:
    return ret;
}

PyObject*
Context_max_time(Context* self, PyObject* args, PyObject* kwargs)
{
    PyObject* ret = NULL;
    int curr_max = -1;
    int new_max = -1;

    if(!PyArg_ParseTuple(args, "|i", &new_max)) goto done;

    curr_max = self->max_time;
    if(new_max > 0) self->max_time = (time_t) new_max;

    ret = PyLong_FromLong((long) curr_max);

done:
    return ret;
}

static PyMemberDef Context_members[] = {
    {NULL}
};

static PyMethodDef Context_methods[] = {
    {
        "add_global",
        (PyCFunction)Context_add_global,
        METH_VARARGS,
        "Install a global object in the JS VM."
    },
    {
        "rem_global",
        (PyCFunction)Context_rem_global,
        METH_VARARGS,
        "Remove a global object in the JS VM."
    },
    {
        "set_access",
        (PyCFunction)Context_set_access,
        METH_VARARGS,
        "Set the access handler for wrapped python objects."
    },
    {
        "execute",
        (PyCFunction)Context_execute,
        METH_VARARGS | METH_KEYWORDS,
        "Execute JavaScript source code."
    },
    {
        "compile",
        (PyCFunction)Context_compile,
        METH_VARARGS | METH_KEYWORDS,
        "Compile JavaScript source code."
    },
    {
        "set_error_reporter",
        (PyCFunction)Context_set_error_reporter,
        METH_O,
        "Specify a callable error reporter."
    },
    {
        "gc",
        (PyCFunction)Context_gc,
        METH_VARARGS,
        "Force garbage collection of the JS context."
    },
    {
        "max_memory",
        (PyCFunction)Context_max_memory,
        METH_VARARGS,
        "Get/Set the maximum memory allocation allowed for a context."
    },
    {
        "max_time",
        (PyCFunction)Context_max_time,
        METH_VARARGS,
        "Get/Set the maximum time a context can execute for."
    },
    {NULL}
};

PyTypeObject _ContextType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /*ob_size*/
    "spidermonkey.Context",                     /*tp_name*/
    sizeof(Context),                            /*tp_basicsize*/
    0,                                          /*tp_itemsize*/
    (destructor)Context_dealloc,                /*tp_dealloc*/
    0,                                          /*tp_print*/
    0,                                          /*tp_getattr*/
    0,                                          /*tp_setattr*/
    0,                                          /*tp_compare*/
    0,                                          /*tp_repr*/
    0,                                          /*tp_as_number*/
    0,                                          /*tp_as_sequence*/
    0,                                          /*tp_as_mapping*/
    0,                                          /*tp_hash*/
    0,                                          /*tp_call*/
    0,                                          /*tp_str*/
    0,                                          /*tp_getattro*/
    0,                                          /*tp_setattro*/
    0,                                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /*tp_flags*/
    "JavaScript Context",                       /*tp_doc*/
    0,		                                    /*tp_traverse*/
    0,		                                    /*tp_clear*/
    0,		                                    /*tp_richcompare*/
    0,		                                    /*tp_weaklistoffset*/
    0,		                                    /*tp_iter*/
    0,		                                    /*tp_iternext*/
    Context_methods,                            /*tp_methods*/
    Context_members,                            /*tp_members*/
    0,                                          /*tp_getset*/
    0,                                          /*tp_base*/
    0,                                          /*tp_dict*/
    0,                                          /*tp_descr_get*/
    0,                                          /*tp_descr_set*/
    0,                                          /*tp_dictoffset*/
    (initproc)Context_init,                     /*tp_init*/
    0,                                          /*tp_alloc*/
    Context_new,                                /*tp_new*/
};

int
Context_has_access(Context* pycx, JSContext* jscx, PyObject* obj, PyObject* key)
{
    PyObject* tpl = NULL;
    PyObject* tmp = NULL;
    int res = -1;

    if(pycx->access == NULL)
    {
        res = 1;
        goto done;
    }

    tpl = Py_BuildValue("(OO)", obj, key);
    if(tpl == NULL) goto done;

    tmp = PyObject_Call(pycx->access, tpl, NULL);

    // Any exception raised is interpreted as an access check failure.  This allows
    // more information to be passed from the access check routine, or traps
    // unexpected conditions.

    if (tmp == NULL) {
	Py_XDECREF(tpl);
	return NULL;
    }

    res = PyObject_IsTrue(tmp);

done:
    Py_XDECREF(tpl);
    Py_XDECREF(tmp);

    if(res < 0)
    {
        PyErr_Clear();
        JS_ReportError(jscx, "Failed to check python access.");
    }
    else if(res == 0)
    {
        JS_ReportError(jscx, "Python access prohibited.");
    }

    return res;
}

PyObject*
Context_get_class(Context* cx, const char* key)
{
    return PyDict_GetItemString((PyObject*) cx->classes, key);
}

int
Context_add_class(Context* cx, const char* key, PyObject* val)
{
    return PyDict_SetItemString((PyObject*) cx->classes, key, val);
}

void 
addobject_decref(void *ptr)
{
    PyObject* pyo = (PyObject*) ptr;
    Py_DECREF(pyo);
}

int
Context_add_object(Context* cx, PyObject* val)
{
    PyObject* wrapped = HashCObj_FromVoidPtr(val, addobject_decref);
    return PySet_Add((PyObject*) cx->objects, wrapped);
}
