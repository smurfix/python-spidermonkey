/*
 * Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
 *
 * This file is part of the python-spidermonkey package released
 * under the MIT license.
 *
 */

#ifndef SPIDERMONKEY_H
#define SPIDERMONKEY_H

#include <js/RequiredDefines.h>

#include <Python.h>
#include "structmember.h"

#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#pragma GCC diagnostic ignored "-Wunused-variable"
#include <jsapi.h>
#pragma GCC diagnostic warning "-Winvalid-offsetof"
#pragma GCC diagnostic warning "-Wunused-variable"

#include "pyplus.h"

#include "runtime.h"
#include "context.h"

#include "string.h"
#include "integer.h"
#include "double.h"

#include "pyobject.h"
#include "pyiter.h"

#include "jsobject.h"
#include "jsarray.h"
#include "jscompiled.h"
#include "jsfunction.h"
#include "jsiterator.h"

#include "convert.h"
#include "error.h"

#include "hashcobj.h"

extern PyObject* SpidermonkeyModule;
extern PyTypeObject* RuntimeType;
extern PyTypeObject* ContextType;
extern PyTypeObject* ClassType;
extern PyTypeObject* ObjectType;
extern PyTypeObject* ArrayType;
extern PyTypeObject* CompiledType;
extern PyTypeObject* FunctionType;
extern PyTypeObject* IteratorType;
extern PyTypeObject* HashCObjType;
extern PyObject* JSError;

#endif

#define TRUE (-1)
#define FALSE (0)
