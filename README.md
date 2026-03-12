# First Vulkan Project

My first vulkan project. Using it to learn how Vulkan works so I can use it for GPU stuff in the future.

To build, we first need to get CMake initialized. Run

```
cmake -G "Ninja" -B build -DCMAKE_CXX_COMPILER='g++'
```

This makes a build directory and forces cmake to make a Ninja makefile. This is necessary for Vulkan, which doesn't support Unix Makefiles.

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
