#!/usr/bin/env bash
# Fails unless each flag reached the build: every compile command, and the
# link flags of executables and shared libraries.
#
# Usage: check-build-flags.sh <build-dir> <flags>
#
# A green build is no evidence the flags applied. CMake ignores CXXFLAGS and
# LDFLAGS once the cache sets the variables they initialise, and a sanitizer
# flag that never reaches the compiler still builds and passes the tests.
set -euo pipefail

build_dir=$1
read -r -a flags <<< "$2"

commands=$(jq -r '.[].command' "$build_dir/compile_commands.json")
total=$(grep -c . <<< "$commands" || true)
if [ "$total" -eq 0 ]; then
  echo "$build_dir/compile_commands.json lists no compile commands." >&2
  exit 1
fi

link_flags() {
  sed -n "s/^$1:[A-Z]*=//p" "$build_dir/CMakeCache.txt"
}
exe_link=" $(link_flags CMAKE_EXE_LINKER_FLAGS) "
shared_link=" $(link_flags CMAKE_SHARED_LINKER_FLAGS) "

status=0
for flag in "${flags[@]}"; do
  carried=$(grep -c -F -e "$flag" <<< "$commands" || true)
  echo "$flag: $carried of $total compile commands"
  if [ "$carried" -ne "$total" ]; then
    status=1
  fi
  for link in "$exe_link" "$shared_link"; do
    case "$link" in
      *" $flag "*) ;;
      *)
        echo "$flag is missing from the link flags:$link" >&2
        status=1
        ;;
    esac
  done
done
exit "$status"
