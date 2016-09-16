
# Building

The VS 2015 project provides *Debug* and *Release* configurations for both *Win32* and *x64*. *x64*
support is experimental and will not compile without warnings.

The result is a single file `ClockScreenSaver.scr` in the directory `$(Platform)\$(Configuration)`,
e.g. `Win32\Release`.
