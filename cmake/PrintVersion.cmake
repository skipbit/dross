# Report what the build will say about its version, to a caller outside CMake.
#
# The release workflow and the documentation build both need this, and
# deriving it in shell would be a second encoding of a rule that lives here --
# which tags count as releases -- so the two would drift.
#
# Expects SOURCE_DIR and OUTPUT on the command line. Writes two lines to
# OUTPUT: the git description, then the numeric version. They go to a file
# because message() writes to stderr, which a caller cannot capture without
# also capturing anything else CMake says.

cmake_minimum_required(VERSION 3.20)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectVersion.cmake")

dross_version_detect(version describe "${SOURCE_DIR}")

file(WRITE "${OUTPUT}" "${describe}\n${version}\n")
