/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#include "spidermonkey.h"
#include "frameobject.h" // Python
#include "traceback.h" // Python

static int
add_to_dict(PyDictObject* dict, const char *key, PyObject *newval)
{
    int ret = 0;

    if (newval != NULL) {
	ret = !PyDict_SetItemString((PyObject*)dict, key, newval);
	Py_XDECREF(newval);	/* passed in object was addrefed (created from scratch) */
    }

    return ret;
}

static void
user_error_reporter_wrapper(JSContext *jscx, const char *message, JSErrorReport *report)
{
    PyDictObject* infodict = NULL;
    PyObject* tpl = NULL;
    PyObject* tmp = NULL;
    Context* pycx;

    //if (report->flags & JSREPORT_EXCEPTION)
    //	return;			/* these are best handled by JS */

    pycx = (Context*) JS_GetContextPrivate(jscx);

    if (pycx == NULL || pycx->err_reporter == NULL)
	return;			/* not much we can do */

    infodict = (PyDictObject*) PyDict_New();
    if (infodict == NULL)
	return;

    if (!add_to_dict(infodict, "flags", PyInt_FromLong(report->flags)) ||
	!add_to_dict(infodict, "errorNumber", PyInt_FromLong(report->errorNumber)) ||
	!add_to_dict(infodict, "lineno", PyInt_FromLong(report->lineno)))
	goto error;

    if (message != NULL && !add_to_dict(infodict, "message", PyString_FromString(message)))
	goto error;

    if (report->filename != NULL)
	if (!add_to_dict(infodict, "filename", PyString_FromString(report->filename)))
	    goto error;

    if (report->linebuf != NULL) {
	if (!add_to_dict(infodict, "linebuf", PyString_FromString(report->linebuf)))
	    goto error;
	if (report->tokenptr != NULL &&
	    !add_to_dict(infodict, "tokenpos", PyLong_FromLong(report->tokenptr - report->linebuf)))
	    goto error;
    }

    /* Now call the error reporting function */

    tpl = Py_BuildValue("(O)", infodict);
    if (tpl != NULL) {
	tmp = PyObject_Call(pycx->err_reporter, tpl, NULL);
	if (tmp != NULL)
	    Py_XDECREF(tmp);
    }

 error:
    Py_XDECREF(infodict);
    Py_XDECREF(tpl);
}

void
add_frame(const char* srcfile, const char* funcname, int linenum)
{
    PyObject* src = NULL;
    PyObject* func = NULL;
    PyObject* glbl = NULL;
    PyObject* tpl = NULL;
    PyObject* str = NULL;
    PyCodeObject* code = NULL;
    PyFrameObject* frame = NULL;

    src = PyString_FromString(srcfile);
    if(src == NULL) goto error;

    func = PyString_FromString(funcname);
    if(func == NULL) goto error;
    
    glbl = PyModule_GetDict(SpidermonkeyModule);
    if(glbl == NULL) goto error;

    tpl = PyTuple_New(0);
    if(tpl == NULL) goto error;

    str = PyString_FromString("");
    if(str == NULL) goto error;

    code = PyCode_New(
        0,                      /*co_argcount*/
        0,                      /*co_nlocals*/
        0,                      /*co_stacksize*/
        0,                      /*co_flags*/
        str,                    /*co_code*/
        tpl,                    /*co_consts*/
        tpl,                    /*co_names*/
        tpl,                    /*co_varnames*/
        tpl,                    /*co_freevars*/
        tpl,                    /*co_cellvars*/
        src,                    /*co_filename*/
        func,                   /*co_name*/
        linenum,                /*co_firstlineno*/
        str                     /*co_lnotab*/
    );
    if(code == NULL) goto error;
   
    frame = PyFrame_New(PyThreadState_Get(), code, glbl, NULL);
    if(frame == NULL) goto error;
    
    frame->f_lineno = linenum;
    PyTraceBack_Here(frame);

    goto success;
    
error:
success:
    Py_XDECREF(func);
    Py_XDECREF(src);
    Py_XDECREF(tpl);
    Py_XDECREF(str);
    Py_XDECREF(code);
    Py_XDECREF(frame);
}

void
report_error_cb(JSContext* cx, const char* message, JSErrorReport* report)
{
    /* Subtle note about JSREPORT_EXCEPTION: it triggers whenever exceptions
     * are raised, even if they're caught and the Mozilla docs say you can
     * ignore it.
     */

    /* If there is a user callback, execute it first */

    user_error_reporter_wrapper(cx, message, report);

    /* Continue and include anything else except WARNINGS as a standard Python
     * exception.
     */

    if (report->flags & JSREPORT_WARNING)
        return;

    const char* srcfile = report->filename;
    const char* mesg = message;

    if(srcfile == NULL) srcfile = "<JavaScript>";
    if(mesg == NULL) mesg = "Unknown JavaScript execution error";

    if(!PyErr_Occurred())
    {
        PyErr_SetString(JSError, message);
	PyErr_SyntaxLocation(srcfile, report->lineno);
    }

    add_frame(srcfile, "JavaScript code", report->lineno);
}
