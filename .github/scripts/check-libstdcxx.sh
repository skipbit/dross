#!/usr/bin/env bash
# Reports which libstdc++ Clang builds against with the job's CXX and CXXFLAGS.
#
# Without --gcc-install-dir in CXXFLAGS, the selection Clang makes on its own
# is reported and never fails: those jobs build against whatever the image
# provides, so a change of release is news. Clang selects the newest GCC
# installation it finds, so the image name does not tell which one that is.
#
# With --gcc-install-dir, the job's name claims that release, and a green
# build is no evidence for it. The include search path is read back, and the
# first entry under /usr/include/c++ has to be that release's headers. The
# directory comes from libgcc-N-dev and the headers from libstdc++-N-dev, so
# the directory can exist with no headers behind it; that is reported too.
set -euo pipefail

read -r -a flags <<< "${CXXFLAGS:-}"
install_dir=
for flag in "${flags[@]}"; do
  case "$flag" in
    --gcc-install-dir=*) install_dir=${flag#--gcc-install-dir=} ;;
  esac
done

if [ -n "$install_dir" ] && [ ! -d "$install_dir" ]; then
  echo "$install_dir does not exist: the packages did not bring in the GCC installation this job pins." >&2
  exit 1
fi

if ! probe=$("$CXX" -std=c++23 "${flags[@]}" -E -x c++ -v /dev/null 2>&1); then
  printf '%s\n' "$probe" >&2
  echo "The driver failed with CXXFLAGS='${CXXFLAGS:-}'; its output is above." >&2
  if [ -z "$install_dir" ]; then
    exit 0
  fi
  exit 1
fi

if [ -z "$install_dir" ]; then
  if ! grep -m1 'Selected GCC installation:' <<< "$probe"; then
    printf '%s\n' "$probe"
    echo "(the driver reported no GCC installation selection)"
  fi
  exit 0
fi

expected=/usr/include/c++/$(basename "$install_dir")

entries=$(printf '%s\n' "$probe" | awk '
  /search starts here:/ { inside = 1; next }
  /End of search list/  { inside = 0 }
  inside && NF          { sub(/^[ \t]+/, ""); print }')
if [ -z "$entries" ]; then
  echo "No include search path in the driver output below; the marker Clang prints may have changed." >&2
  printf '%s\n' "$probe" >&2
  exit 1
fi
printf '%s\n' "$entries"

# Clang prints the entries relative to the GCC installation, such as
# .../13/../../../../include/c++/13.
selected=
while read -r entry; do
  resolved=$(readlink -m "$entry")
  case "$resolved" in
    /usr/include/c++/*)
      selected=$resolved
      break
      ;;
  esac
done <<< "$entries"

if [ "$selected" != "$expected" ]; then
  echo "Expected $expected to come first, found ${selected:-no standard library headers at all}." >&2
  exit 1
fi
echo "$selected comes first in the include search path, as pinned."
