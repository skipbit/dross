#!/bin/bash
#
# Runs the formatting tools at the version docker/Dockerfile pins. The pull
# request checks call this too, so a source that passes here passes there.
#
#   scripts/tools.sh format        rewrite the sources in place
#   scripts/tools.sh format-check  fail on a source that is not formatted
#
# Outside the container it starts one and runs itself inside it.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ -z "${DROSS_TOOLS_CONTAINER:-}" ]; then
  exec env REPO_ROOT="$root" HOST_UID="$(id -u)" HOST_GID="$(id -g)" \
    docker compose -f "$root/docker/docker-compose.yml" run --rm --build tools \
    scripts/tools.sh "$@"
fi

cd "$root"

formatted_sources() {
  find src include test -type f \( -name '*.cpp' -o -name '*.h' \) -print0
}

case "${1:-}" in
  format)
    formatted_sources | xargs -0 -r clang-format -i
    ;;
  format-check)
    clang-format --version
    # A pattern that stopped matching would otherwise check nothing and pass.
    count="$(formatted_sources | tr -cd '\0' | wc -c)"
    echo "$count files"
    if [ "$count" -eq 0 ]; then
      echo "no sources to check" >&2
      exit 1
    fi
    formatted_sources | xargs -0 -r clang-format --dry-run --Werror
    ;;
  *)
    echo "usage: scripts/tools.sh format | format-check" >&2
    exit 2
    ;;
esac
