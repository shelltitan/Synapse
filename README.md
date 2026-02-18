# Synapse
Synapse is a lightweight framework for building multiplayer games.

## CMake settings
- `STACK_LIFO_CHECK`, If this macro is defined, the stack allocator will track allocations and verify that they are deallocated in Last-In-First-Out (LIFO) order.

# Installing dependecies
```
cmake -DCMAKE_PREFIX_PATH=$(pwd)/install -DCMAKE_INSTALL_PREFIX=install -B Dependecies/build -S Dependencies
cmake --build Dependecies/build
```

**Note:** For production builds on Windows, **Clang** is recommended over **MSVC** due to better optimisation and code generation performance.
