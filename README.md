# Global-Corruptor
This is a program that auto corrupts programs in such a way to yield glitchy results.

Currently, it runs a random address changer, where it takes a section of ram, picks two random bytes, one is a target which defines the value to be changed, and the value, which is what the target will be changed into when found.
This blanket approch causes a lot of courruption of ram, but is relitivly stable. Much more stable then the pecking technuique I used previously.

To build, you will need [CMake](http://www.cmake.org) and, if you are on a non-Windows system, [Boost](http://www.boost.org).
Build tests have been done with Visual Studio 2015 and GCC 6.1. Clang may work but it is not guaranteed.

## Building on Windows
### Visual Studio 2015
Access the Developer Command Prompt from the MSVC 2015 folder in the Start menu, navigate this folder and run `cmake -G "Visual Studio 14 2015"`, or if you prefer 64-bit, `cmake -G "Visual Studio 14 2015 Win64"`

## Building on Linux
In your terminal application (Konsole, xterm, etc), navigate to this folder and run `cmake`
