# Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
#
# This file is part of the python-spidermonkey package released
# under the MIT license.
import t

@t.rt()
def test_creating_runtime(rt):
    t.ne(rt, None)

def test_create_no_memory():
    rt = t.spidermonkey.Runtime(1)
    t.raises(RuntimeError, rt.new_context)

def test_exceed_memory():
    # Test to see if OOM exception os raised.
    rt = t.spidermonkey.Runtime(50000)
    cx = rt.new_context()
    script = "var b = []; for(var f in 100000) b.push(2.456);"
    cx.execute(script)

