# Report the version description of a source tree to a caller outside CMake.
#
# The documentation build needs the same answer the library build uses.
# Running `git describe` in shell would be a second encoding of the selection
# rule -- which tags count as releases -- and the two would drift. This is the
# one implementation; callers read its output.
#
# Expects SOURCE_DIR and OUTPUT on the command line. The description is
# written to OUTPUT because message() goes to stderr, which a caller cannot
# capture without also capturing anything else CMake says.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectVersion.cmake")

dross_version_detect(version describe "${SOURCE_DIR}")
file(WRITE "${OUTPUT}" "${describe}")
