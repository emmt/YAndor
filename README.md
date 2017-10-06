# YAndor is a Yorick plug-in for Andor cameras


## Documentation

There is a detailled documentation in [`andor.i`](./andor.i) file and the
"*Andor Software Guide*" is a good complement.


## Installation

This plug-in was tested with Andor SDK3.

To compile the plug-in, edit the `Makefile` (in particular the
`PKG_DEPLIBS` and `PKG_CFLAGS` macros needed to find the libraries and
headers files from the Andor SDK), then:

    yorick -batch make.i
    make
    make install

An alternative is to use the `configure` script.  This script can be run from
the source directory, say `$SRCDIR`, or from another build directory.  For
instance:

    mkdir -p build
    cd build
    $SRCDIR/configure ...         # --help to see the options
    make

To install the plug-in:

    make install

To test (after compilation, installation not needed), launch `yorick` and
type:

    include, "test.i";


## Issues

* Occasionally `YAndor` goes to 100% CPU usage while nothing is running.
