#
# Copyright 2009 Paul J. Davis <paul.joseph.davis@gmail.com>
# Copyright 2014 Gary J. Wisniewski <garyw@blueseastech.com>
#
# This file is part of the python-spidermonkey package released
# under the MIT license.
#

"""\
Python/JavaScript bridge module, making use of Mozilla's spidermonkey
JavaScript implementation.  Allows implementation of JavaScript classes,
objects and functions in Python, and evaluation and calling of JavaScript
scripts and functions respectively.  Borrows heavily from Claes Jacobssen's
Javascript Perl module, in turn based on Mozilla's 'PerlConnect' Perl binding.
""",

# This is a workaround for a Python bug aggravated by nose
# Issue report: http://bugs.python.org/issue15881
try:
    import multiprocessing
except ImportError:
    pass

# I haven't the sligthest, but this appears to fix
# all those EINTR errors. Pulled and adapted for OS X
# from twisted bug #733
# 
# Definitely forgot to comment this out before distribution.
#
# import ctypes
# import signal
# libc = ctypes.CDLL("libc.dylib")
# libc.siginterrupt(signal.SIGCHLD, 0)

import glob
import os
import subprocess as sp
import sys
from distutils.dist import Distribution
from distutils.sysconfig import get_config_vars
from setuptools import setup, Extension

MOZJS_SWITCH = "--mozjs-source"
MOZJS_PATH = None
USE_SYSTEM_LIB = True

DEBUG = "--debug" in sys.argv

if MOZJS_SWITCH in sys.argv:
    MOZJS_PATH = os.path.abspath(os.path.join(sys.argv[sys.argv.index(MOZJS_SWITCH) + 1], 'js/src/dist'))
    USE_SYSTEM_LIB = False
    if not os.path.exists(MOZJS_PATH):
        print "Error: Trying to use mozjs source build, but cannot find path: " + MOZJS_PATH
        exit(1)

# Remove strict-prototypes from any options, this allows CPP files to be compiled using gcc.  This is really
# a bug in distutils.
# Code is from: http://stackoverflow.com/questions/8106258/cc1plus-warning-command-line-option-wstrict-prototypes-is-valid-for-ada-c-o/9740721#9740721
(opt,) = get_config_vars('OPT')
os.environ['OPT'] = " ".join(flag for flag in opt.split() if flag != '-Wstrict-prototypes')

def find_sources(extensions=(".c", ".cpp")):
    return [
        fname
        for ext in extensions
        for fname in glob.glob("spidermonkey/*" + ext)
        ]

def pkg_config(pkg_name, config=None):
    pipe = sp.Popen("pkg-config --cflags --libs %s" % pkg_name,
                        shell=True, stdout=sp.PIPE, stderr=sp.PIPE)
    (stdout, stderr) = pipe.communicate()
    if pipe.wait() != 0:
        raise RuntimeError("No package configuration found for: %s" % pkg_name)
    if config is None:
        config = {
            "include_dirs": [],
            "library_dirs": [],
            "libraries": [],
            "extra_compile_args": [],
            "extra_link_args": []
        }
    prefixes = {
        "-I": ("include_dirs", 2),
        "-L": ("library_dirs", 2),
        "-l": ("libraries", 2),
        "-D": ("extra_compile_args", 0),
        "-Wl": ("extra_link_args", 0)
    }
    for flag in stdout.split():
        for prefix in prefixes:
            if not flag.startswith(prefix):
                continue
            # Hack for xulrunner
            if flag.endswith("/stable"):
                flag = flag[:-6] + "unstable"
            name, trim = prefixes[prefix]
            config[name].append(flag[trim:])
    return config

def nspr_config(config=None):
    return pkg_config("nspr", config)

def js_config(config=None):
    config = pkg_config("mozjs185", config)
    return config

def mozjs_config(config=None):
    config = pkg_config("mozilla-js", config)
    return config

def platform_config():
    sysname = os.uname()[0]
    machine = os.uname()[-1]

    # Build our configuration
    config = {
        # no-write-strings turns of coercion warnings about string literals.  THIS SHOULD BE REMOVED
        # as soon as Python header files start using const correctly.
        "extra_compile_args": ["-Wno-write-strings"],
        "include_dirs": [],
        "library_dirs": [],
        "libraries": [],
        "extra_link_args": []
    }

    # If we're linking against a system library it should give
    # us all the information we need.
    if USE_SYSTEM_LIB:
        if DEBUG:
            config['extra_compile_args'] = ['-g', '-DDEBUG', '-O0']
            return mozjs_config(config=config)
        else:
            return mozjs_config()
    
    # Debug builds are useful for finding errors in
    # the request counting semantics for Spidermonkey
    if DEBUG:
        config["extra_compile_args"].extend([
            "-UNDEBG",
            "-DDEBUG",
            "-DJS_PARANOID_REQUEST",
            "-O0",
        ])

    config["include_dirs"] = [
            "spidermonkey/%s-%s" % (sysname, machine)
            ]

    if MOZJS_PATH:
        config["include_dirs"] += [os.path.join(MOZJS_PATH, "include")]
        config["library_dirs"] += [os.path.join(MOZJS_PATH, "lib")]
        config["runtime_library_dirs"] = [os.path.join(MOZJS_PATH, "lib")]
        config["libraries"] += ['mozjs-24']
        config["extra_compile_args"] += ["-Wl,-version-script,symverscript"]
        #config["extra_compile_args"] += ["-include RequiredDefines.h"]
    else:
        config["include_dirs"] += ["spidermonkey/libjs"]

    if sysname in ["Linux", "FreeBSD"]:
        config["extra_compile_args"].extend([
            "-DHAVE_VA_COPY",
            "-DVA_COPY=va_copy"
        ])

    # Currently no suppot for Win32, patches welcome.
    if sysname in ["Darwin", "Linux", "FreeBSD"]:
        config["extra_compile_args"].append("-DXP_UNIX")
    else:
        raise RuntimeError("Unknown system name: %s" % sysname)

    return nspr_config(config=config)

Distribution.global_options.append(("debug", None,
                    "Build a DEBUG version of spidermonkey."))
Distribution.global_options.append((MOZJS_SWITCH[2:] + "=", None,
                    "Link against prebuilt library at specific location."))

setup(
    name = "python-spidermonkey",
    version = "0.2.01",
    license = "MIT",
    author = "Gary J. Wisniewski",
    author_email = "garyw@blueseastech.com",
    description = "JavaScript / Python bridge.",
    long_description = __doc__,
    url = "http://github.com/garywiz/python-spidermonkey",
    download_url = "http://github.com/garywiz/python-spidermonkey.git",
    zip_safe = False,
    
    classifiers = [
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Natural Language :: English',
        'Operating System :: OS Independent',
        'Programming Language :: C',
        'Programming Language :: JavaScript',
        'Programming Language :: Other',
        'Programming Language :: Python',
        'Topic :: Internet :: WWW/HTTP :: Browsers',
        'Topic :: Internet :: WWW/HTTP :: Dynamic Content',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ],
    
    setup_requires = [
        'setuptools>=3.4',
        'nose>=1.3.0',
    ],

    ext_modules =  [
        Extension(
            "spidermonkey",
            sources=find_sources(),
            **platform_config()
        )
    ],

    test_suite = 'nose.collector',

)
