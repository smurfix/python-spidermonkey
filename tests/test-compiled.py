# Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
#
# This file is part of the python-spidermonkey package released
# under the MIT license.
import t
import time

@t.rt()
def test_no_provided_runtime(rt):
    t.raises(TypeError, t.spidermonkey.Context)

@t.rt()
def test_invalid_runtime(rt):
    t.raises(TypeError, t.spidermonkey.Context, 0)

@t.cx()
def test_compiled_execution(cx):
    expr1 = cx.compile("var x = 4; x * x;")
    print type(expr1)
    t.eq(expr1.execute(), 16)
    expr2 = cx.compile("22/7;")
    t.lt(expr2.execute() - 3.14285714286, 0.00000001)

@t.rt()
def test_compiled_contexts(rt):
    "Check to be sure multiple contexts can be used for compiled execution."
    ctx1 = rt.new_context({'a': 111})
    ctx2 = rt.new_context({'a': 222})
    expr1 = ctx1.compile("a * 3;")
    t.eq(expr1.execute(), 333)
    t.eq(expr1.execute(ctx2), 666)
