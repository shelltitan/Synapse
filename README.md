# Synapse
Synapse is a lightweight framework for building multiplayer games.

## CMake settings
- `STACK_LIFO_CHECK`, If this macro is defined, the stack allocator will track allocations and verify that they are deallocated in Last-In-First-Out (LIFO) order.

**Note:** For production builds on Windows, **Clang** is recommended over **MSVC** due to better optimization and code generation performance.
