[![Build status](https://ci.appveyor.com/api/projects/status/9ippie8i83vqt62n?svg=true)](https://ci.appveyor.com/project/tniessen/clockscreensaver)

# Building

The VS 2015 project provides *Debug* and *Release* configurations for both *Win32* and *x64*. *x64*
support is experimental and will not compile without warnings.

The result is a single file `ClockScreenSaver.scr` in the directory `$(Platform)\$(Configuration)`,
e.g. `Win32\Release`.
