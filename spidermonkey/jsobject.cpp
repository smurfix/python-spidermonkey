/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"

PyObject* make_object(PyTypeObject* type, Context* cx, jsval val)
{
    JSObject* obj = JSVAL_TO_OBJECT(val);

    JSAutoRequest request(cx->cx);

    // Wrap JS value
    CPyAutoObject tpl(Py_BuildValue("(O)", cx));
    if (tpl == NULL)
	return NULL;
    
    CPyAutoPJObject wrapped((PJObject*)PyObject_CallObject((PyObject*) type, tpl));
    if (wrapped.isNull())
	return NULL;
    
    wrapped->val = val;
    wrapped->obj = obj;

    if (!JS_AddNamedValueRoot(cx->cx, &(wrapped->val), "make_object")) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to set GC root.");
	return NULL;
    }

    return (PyObject*)wrapped.asNew();
}

PyObject* js2py_object(Context* cx, jsval val)
{
    return make_object(PJObjectType, cx, val);
}

PyObject* PJObject_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    PJObject* self = NULL;
    Context* cx = NULL;

    if (!PyArg_ParseTuple(args, "O!", ContextType, &cx)) {
	ERROR("spidermonkey.Object.new");
	return NULL;
    }

    self = (PJObject*) type->tp_alloc(type, 0);
    if (self == NULL) {
	ERROR("spidermonkey.Object.new");
	return NULL;
    }

    Py_INCREF(cx);
    self->cx = cx;
    self->val = JSVAL_VOID;
    self->obj = NULL;

    return (PyObject*) self;
}

int PJObject_init(PJObject* self, PyObject* args, PyObject* kwargs)
{
    return 0;
}

void PJObject_dealloc(PJObject* self)
{
    if (!JSVAL_IS_VOID(self->val)) {
	JSAutoRequest request(self->cx->cx);
        JS_RemoveValueRoot(self->cx->cx, &(self->val));
    }
   
    Py_XDECREF(self->cx);
}

PyObject* PJObject_repr(PJObject* self)
{
    JSString* repr = NULL;
    const jschar* rchars = NULL;
    size_t rlen;
    
    JSAutoRequest request(self->cx->cx);

    repr = JS_ValueToString(self->cx->cx, self->val);
    if (repr == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert to a string.");
        return NULL;
    }

    rchars = JS_GetStringCharsZ(self->cx->cx, repr);
    rlen = JS_GetStringLength(repr);

    return PyUnicode_Decode((const char*) rchars, rlen*2, "utf-16", "strict");
}

Py_ssize_t PJObject_length(PJObject* self)
{
    JSAutoRequest request(self->cx->cx);

    JSIdArray *ida = JS_Enumerate(self->cx->cx, JSVAL_TO_OBJECT(self->val));

    if (ida)
	return JS_IdArrayLength(self->cx->cx, ida);

    return 0;
}

PyObject* PJObject_getitem(PJObject* self, PyObject* key)
{
    jsval pval;
    jsid pid;

    JSAutoRequest request(self->cx->cx);

    pval = py2js(self->cx, key);
    if (JSVAL_IS_VOID(pval)) 
	return NULL;
   
    if(!JS_ValueToId(self->cx->cx, pval, &pid)) {
        PyErr_SetString(PyExc_KeyError, "Failed to get property id.");
	return NULL;
    }
    
    if(!JS_GetPropertyById(self->cx->cx, self->obj, pid, &pval)) {
        PyErr_SetString(PyExc_AttributeError, "Failed to get property.");
	return NULL;
    }

    return js2py_with_parent(self->cx, pval, self->val);
}

int PJObject_setitem(PJObject* self, PyObject* key, PyObject* val)
{
    jsval pval;
    jsval vval;
    jsid pid;

    JSAutoRequest request(self->cx->cx);

    pval = py2js(self->cx, key);
    if (JSVAL_IS_VOID(pval))
	return -1;
   
    if (!JS_ValueToId(self->cx->cx, pval, &pid)) {
        PyErr_SetString(PyExc_KeyError, "Failed to get property id.");
	return -1;
    }
   
    if (val != NULL) {
        vval = py2js(self->cx, val);
        if (JSVAL_IS_VOID(vval))
	    return -1;

        if (!JS_SetPropertyById(self->cx->cx, self->obj, pid, &vval)) {
            PyErr_SetString(PyExc_AttributeError, "Failed to set property.");
	    return -1;
        }
    } else {
        if (!JS_DeletePropertyById(self->cx->cx, self->obj, pid)) {
            PyErr_SetString(PyExc_AttributeError, "Failed to delete property.");
	    return -1;
        }

        if (JSVAL_IS_VOID(vval)) {
            PyErr_SetString(PyExc_AttributeError, "Unable to delete property.");
	    return -1;
        }
    }

    return 0;
}

PyObject* PJObject_rich_cmp(PJObject* self, PyObject* other, int op)
{
    if (!PyMapping_Check(other) && !PySequence_Check(other)) {
        PyErr_SetString(PyExc_ValueError, "Invalid rhs operand.");
	return NULL;
    }

    if (op != Py_EQ && op != Py_NE)
	return Py_INCREF_RET(Py_NotImplemented);

    JSContext *jcx = self->cx->cx;
    JSAutoRequest request(jcx);

    JSIdArray *ida = JS_Enumerate(jcx, JSVAL_TO_OBJECT(self->val));

    int llen = -1;
    if (ida)
	llen = JS_IdArrayLength(jcx, ida);
    if (llen < 0)
	return NULL;

    int rlen = PyObject_Length(other);
    if (rlen < 0) 
	return NULL;

    if (llen != rlen) {
        if (op == Py_EQ)
	    return Py_INCREF_RET(Py_False);
	return Py_INCREF_RET(Py_True);
    }

    for (int idix = 0; idix < llen; idix++) {
	jsval pkey, pval;
	jsid pid = JS_IdArrayGet(jcx, ida, idix);

        if (!JS_IdToValue(jcx, pid, &pkey)) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to get key.");
	    return NULL;
        }

        if (!JS_GetPropertyById(jcx, self->obj, pid, &pval)) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to get property.");
	    return NULL;
        }

        CPyAutoObject key(js2py(self->cx, pkey));
        if (key.isNull())
	    return NULL;
	
        CPyAutoObject val(js2py(self->cx, pval));
        if (val.isNull())
	    return NULL;

        CPyAutoObject otherval(PyObject_GetItem(other, key));
        if (otherval.isNull()) {
            PyErr_Clear();
            if (op == Py_EQ)
		return Py_INCREF_RET(Py_False);
	    return Py_INCREF_RET(Py_True);
        }

        int cmp = PyObject_Compare(val, otherval);
        if (PyErr_Occurred())
	    return NULL;

        if (cmp != 0) {
            if (op == Py_EQ)
		return Py_INCREF_RET(Py_False);
	    return Py_INCREF_RET(Py_True);
        }
    }

    if (op == Py_EQ)
	return Py_INCREF_RET(Py_True);
    return Py_INCREF_RET(Py_False);
}

PyObject* PJObject_iterator(PJObject* self, PyObject* args, PyObject* kwargs)
{
    return Iterator_Wrap(self->cx, self->obj);
}

static PyMemberDef PJObject_members[] = {
    {NULL}
};

static PyMethodDef PJObject_methods[] = {
    {NULL}
};

PyMappingMethods PJObject_mapping = {
    (lenfunc)PJObject_length,                     /*mp_length*/
    (binaryfunc)PJObject_getitem,                 /*mp_subscript*/
    (objobjargproc)PJObject_setitem               /*mp_ass_subscript*/
};

PyTypeObject _PJObjectType = {
    PyObject_HEAD_INIT(NULL)
    0,                                          /*ob_size*/
    "spidermonkey.Object",                      /*tp_name*/
    sizeof(PJObject),                           /*tp_basicsize*/
    0,                                          /*tp_itemsize*/
    (destructor)PJObject_dealloc,               /*tp_dealloc*/
    0,                                          /*tp_print*/
    0,                                          /*tp_getattr*/
    0,                                          /*tp_setattr*/
    0,                                          /*tp_compare*/
    (reprfunc)PJObject_repr,                    /*tp_repr*/
    0,                                          /*tp_as_number*/
    0,                                          /*tp_as_sequence*/
    &PJObject_mapping,                          /*tp_as_mapping*/
    0,                                          /*tp_hash*/
    0,                                          /*tp_call*/
    0,                                          /*tp_str*/
    (getattrofunc)PJObject_getitem,             /*tp_getattro*/
    (setattrofunc)PJObject_setitem,             /*tp_setattro*/
    0,                                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,   /*tp_flags*/
    "JavaScript Object",                        /*tp_doc*/
    0,		                                /*tp_traverse*/
    0,		                                /*tp_clear*/
    (richcmpfunc)PJObject_rich_cmp,             /*tp_richcompare*/
    0,		                                /*tp_weaklistoffset*/
    (getiterfunc)PJObject_iterator,	        /*tp_iter*/
    0,		                                /*tp_iternext*/
    PJObject_methods,                           /*tp_methods*/
    PJObject_members,                           /*tp_members*/
    0,                                          /*tp_getset*/
    0,                                          /*tp_base*/
    0,                                          /*tp_dict*/
    0,                                          /*tp_descr_get*/
    0,                                          /*tp_descr_set*/
    0,                                          /*tp_dictoffset*/
    (initproc)PJObject_init,                    /*tp_init*/
    0,                                          /*tp_alloc*/
    PJObject_new,                               /*tp_new*/
};
