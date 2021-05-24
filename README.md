CRENDER
=======

Version
-------

This is version 0.1.

General
-------

This is a small tool intended to do linting, pre-rendering, and analyzis of conda recipes.
It supports some jinja2 features (be aware it doesn't support full python-language feature)
It outputs files in yaml format.

Building / Installing
---------------------

Right now build scripts are just available for unix-like operating systems.  Nevertheless the
adjustments required for Windows are pretty straight-forward, and just a few adjustments might
be necessary for it.

First, build Jinja2Cpp by executing the 'bldJinja2Cpp.sh' script, or run its cmake manually.
Note: Test test-suite is not adjusted, so don't wonder if it fails right now.

Second, build yaml-cpp.  You can use for this also the provided 'bldYaml-cpp.sh' script.

Third step, build crender itself by using 'bld.sh'.

After building you will find 2 new directories in crender's git root directory.  One is 'pref',
which contains all binaries created by Jinja2Cpp and yaml-cpp. The second is the 'bin' folder
containing created crender binary.
The crender binary assumes that as sibling directory to its installed bin directory there is the data/ folder.
So if you move crender somewhere else, don't miss to move "data/" and its content accordingly.

Authors
-------

For Jinja2Cpp and yaml-cpp see its README.
The crender was written by Kai Tietz in 2021/05/21

Licenses used
-------------

Jinja2Cpp is under Mozilla Public License Version 2.0.  For further information see Jinja2Cpp's sources.
This sources were majorly extended by Kai Tietz by using the same license for its source.

yaml-cpp is released under MIT-like license by Jesse Beder.

crender is released under MIT-like license. See LICENSE for more details.


