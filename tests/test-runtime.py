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
    rt = t.spidermonkey.Runtime(1000000)
    cx = rt.new_context()
    script = "var b = []; for (var i = 0; i < 1000000; i++) b.push('string: ' + i);"
    t.raises(t.JSError, cx.execute, script)

