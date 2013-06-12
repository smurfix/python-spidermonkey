/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"

PyObject* 
Compiled_Wrap(Context* cx, JSObject* obj)
{
    Compiled* self = NULL;
    PyObject* tpl = NULL;
    PyObject* ret = NULL;

    JS_BeginRequest(cx->cx);

    // Build our new python object.
    tpl = Py_BuildValue("(O)", cx);
    if(tpl == NULL) goto error;
    
    self = (Compiled*) PyObject_CallObject((PyObject*) CompiledType, tpl);
    if(self == NULL) goto error;
    
    // Attach the compiled blob
    self->cblob = obj;
    self->cbval = OBJECT_TO_JSVAL(obj);

    if(!JS_AddNamedValueRoot(cx->cx, &(self->cbval), "Compiled_Wrap"))
    {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set GC root.");
        goto error;
    }

    ret = (PyObject*) self;
    goto success;

error:
    Py_XDECREF(self);
    ret = NULL; // In case it was AddRoot
success:
    Py_XDECREF(tpl);
    JS_EndRequest(cx->cx);
    return (PyObject*) ret;
}

PyObject*
Compiled_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    Context* cx = NULL;
    Compiled* self = NULL;

    if(!PyArg_ParseTuple(args, "O!", ContextType, &cx)) goto error;

    self = (Compiled*) type->tp_alloc(type, 0);
    if(self == NULL) goto error;
    
    Py_INCREF(cx);
    self->cx = cx;
    self->cblob = NULL;
    self->cbval = JSVAL_VOID;

    goto success;

error:
    ERROR("spidermonkey.Compiled.new()");
success:
    return (PyObject*) self;
}

int Compiled_init(Compiled* self, PyObject* args, PyObject* kwargs)
{
    return 0;
}

void Compiled_dealloc(Compiled* self)
{
    if(self->cblob != NULL)
    {
        JS_BeginRequest(self->cx->cx);
        JS_RemoveValueRoot(self->cx->cx, &(self->cbval));
        JS_EndRequest(self->cx->cx);
    }
   
    Py_XDECREF(self->cx);
}

/* Note that the execution context does not have to be the same as the original
   compiling context, though the original compiling context is held as a location
   to reference root objects. */

static PyObject* Compiled_execute(Compiled* self, PyObject *args, PyObject* kwargs)
{
    PyObject* ret = NULL;
    Context* exctx = NULL;
    JSContext *jcx;
    jsval rval;

    if (!PyArg_ParseTuple(args, "|O!", ContextType, &exctx))
	return NULL;

    if (exctx == NULL)
	exctx = self->cx;

    Py_INCREF(exctx);

    jcx = exctx->cx;

    JS_BeginRequest(jcx);

    if (!JS_ExecuteScript(jcx, exctx->root, self->cblob, &rval))
    {
        if(!PyErr_Occurred())
        {
            PyErr_SetString(PyExc_RuntimeError, "Script execution failed and no exception was set");
        }
        goto done;
    }

    if (!PyErr_Occurred()) {
	ret = js2py(exctx, rval);
	JS_MaybeGC(exctx->cx);
    }

done:
    Py_XDECREF(exctx);
    JS_EndRequest(jcx);
    return ret;
}

static PyMemberDef Compiled_members[] = {
    {NULL}
};

static PyMethodDef Compiled_methods[] = {
    {"execute", (PyCFunction) Compiled_execute, METH_KEYWORDS | METH_VARARGS,
     "Execute the compiled Javascript code"},
    {NULL}
};

PyTypeObject _CompiledType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /*ob_size*/
    "spidermonkey.Compiled",                    /*tp_name*/
    sizeof(Compiled),                           /*tp_basicsize*/
    0,                                          /*tp_itemsize*/
    (destructor)Compiled_dealloc,               /*tp_dealloc*/
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
    "JavaScript Compiled Object",		/*tp_doc*/
    0,						/*tp_traverse*/
    0,						/*tp_clear*/
    0,                                          /*tp_richcompare*/
    0,						/*tp_weaklistoffset*/
    0,						/*tp_iter*/
    0,						/*tp_iternext*/
    Compiled_methods,                           /*tp_methods*/
    Compiled_members,                           /*tp_members*/
    0,                                          /*tp_getset*/
    0,                                          /*tp_base*/
    0,                                          /*tp_dict*/
    0,                                          /*tp_descr_get*/
    0,                                          /*tp_descr_set*/
    0,                                          /*tp_dictoffset*/
    (initproc)Compiled_init,                    /*tp_init*/
    0,                                          /*tp_alloc*/
    Compiled_new,                               /*tp_new*/
};
