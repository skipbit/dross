# dross Documentation

This directory contains the complete documentation system for the dross library.

## Structure

```
docs/
├── README.md              # This file
├── sphinx/                # Sphinx documentation system
│   ├── conf.py           # Sphinx configuration
│   ├── requirements.txt   # Python dependencies
│   ├── Makefile          # Build commands
│   └── source/           # Documentation source files
│       ├── index.rst     # Homepage
│       ├── api/          # API reference
│       ├── user-guide/   # User guides and tutorials
│       ├── examples/     # Example code and use cases
│       ├── _static/      # Custom CSS and assets
│       └── _templates/   # HTML templates
├── doxygen/              # Doxygen configuration
│   └── Doxyfile          # Doxygen configuration file
└── build/                # Build output (git ignored)
    ├── doxygen/          # Doxygen XML and HTML output
    └── sphinx/           # Sphinx HTML output
```

## Building Documentation

### Prerequisites

Install required dependencies:

```bash
# Install Doxygen (system dependency)
# On macOS:
brew install doxygen graphviz

# On Ubuntu/Debian:
sudo apt-get install doxygen graphviz

# Install Python dependencies
cd docs/sphinx
pip install -r requirements.txt
```

### Building

Build the complete documentation:

```bash
cd docs/sphinx
make html-with-doxygen
```

Build only Sphinx documentation:

```bash
cd docs/sphinx
make html
```

Build only Doxygen documentation:

```bash
cd docs/doxygen
doxygen Doxyfile
```

### Development

For live development with auto-reload:

```bash
cd docs/sphinx
make livehtml
```

Then open http://localhost:8000 in your browser.

Quick local build script (optional):

```bash
# Simple build without Docker
./build-docs.sh
```

### Deployment

Documentation is automatically built and deployed to GitHub Pages via GitHub Actions when changes are pushed to the main branch.

## Writing Documentation

### API Documentation

- Add Doxygen comments to header files in `include/dross/`
- Use `@brief`, `@param`, `@return`, `@code`, `@endcode` tags
- Follow the existing style in the codebase

### User Guides

- Create `.rst` files in `sphinx/source/user-guide/`
- Use reStructuredText syntax
- Include practical examples and use cases

### Examples

- Add example code to `sphinx/source/examples/`
- Provide complete, runnable examples
- Explain the code and its purpose

## Themes and Styling

The documentation uses the **Furo** theme for a modern, clean appearance. Custom styling can be added to `sphinx/source/_static/custom.css`.

## Quality Assurance

Check documentation quality:

```bash
cd docs/sphinx
make linkcheck  # Check for broken links
make spelling   # Check spelling (requires sphinxcontrib-spelling)
```

## Contributing

When adding new modules or features:

1. Add Doxygen comments to all public APIs
2. Create user guide documentation
3. Provide practical examples
4. Update the API reference index
5. Test documentation builds locally before submitting