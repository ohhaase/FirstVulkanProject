# C++ Project Template

Template for me to use to make personal C++ projects. Focused on my weird development style, because I work both on Mac and Windows machines. So, these projects are typically worked on in VSCode, using CMake to build. These are both OS agnostic (hopefully lol). For simpler projects that don't include external libraries and the sort, the included Makefile should be able to compile everything as well.

To build, we first need to get CMake initialized. Run

```
cmake -G "Unix Makefiles" -B build -DCMAKE_CXX_COMPILER='g++'
```

This makes a build directory, forces cmake to make a Unix Makefile, and sets the compiler to GNU++. 

Note: Maybe should relax the GNU++ specification in the future.

Then, to generate the executable, use

```
cmake --build build
```

This generates an executable in ./build/.

To run the executable, use

```
./build/<executablename> <sim parameters>
```

This will generate the resulting data relative to the main directory.
