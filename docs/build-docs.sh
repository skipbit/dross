#!/bin/bash

# Simple documentation build script
# Builds documentation locally without Docker

set -e

echo "🔧 Building dross documentation..."

# Check if we're in the docs directory
if [[ ! -f "sphinx/source/conf.py" ]]; then
    echo "❌ Please run this script from the docs/ directory"
    exit 1
fi

# Check dependencies
if ! command -v doxygen &> /dev/null; then
    echo "❌ Doxygen not found. Please install doxygen and graphviz"
    exit 1
fi

if ! command -v sphinx-build &> /dev/null; then
    echo "❌ Sphinx not found. Please install: pip install -r sphinx/requirements.txt"
    exit 1
fi

# The version comes from the same selection the build uses, through the one
# implementation of it (cmake/ProjectVersion.cmake). Asking git here instead
# would be a second encoding of which tags count as releases.
if [ -z "${DROSS_VERSION:-}" ]; then
    version_file="$(mktemp)"
    if cmake -D "SOURCE_DIR=$(cd .. && pwd)" -D "OUTPUT=$version_file" \
             -P ../cmake/PrintVersion.cmake > /dev/null 2>&1; then
        DROSS_VERSION="$(sed -n 1p "$version_file")"
    else
        DROSS_VERSION="unknown"
    fi
    rm -f "$version_file"
fi
export DROSS_VERSION
echo "📌 Version: ${DROSS_VERSION}"

# Build documentation
echo "📖 Building Doxygen documentation..."
cd doxygen && doxygen Doxyfile && cd ..

echo "📚 Building Sphinx documentation..."
cd sphinx && make html && cd ..

echo "✅ Documentation built successfully!"
echo "📁 Output: $(pwd)/build/sphinx/html/"
echo "🌐 To serve locally: python -m http.server 8000 -d build/sphinx/html/"