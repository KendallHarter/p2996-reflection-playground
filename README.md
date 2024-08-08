# p2996-reflection-playground
Playing around with proposed C++ reflection

There's no guarantee that these will continue to build as the compiler
continues to advance but by running prepare.sh with the environment
variable GUARANTEE_BUILD set to non-zero value, a known safe commit will
be used when building Clang.
Otherwise the latest commit on the branch p2996 will be used.

## Building

Prerequisite programs for building are Ninja and CMake.

Run `./prepare` to build the p2996 Clang branch and run CMake for the project.

Build the project with `cmake --build build`.

To run an executable use `./run_executable <executable_name>`, e.g.,
`./run_executable commands`.
