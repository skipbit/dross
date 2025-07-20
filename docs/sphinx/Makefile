# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = ../build/sphinx

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Custom targets
.PHONY: clean-all
clean-all: clean
	rm -rf ../build

.PHONY: doxygen
doxygen:
	cd ../doxygen && doxygen Doxyfile

.PHONY: html-with-doxygen
html-with-doxygen: doxygen html

.PHONY: livehtml
livehtml:
	sphinx-autobuild "$(SOURCEDIR)" "$(BUILDDIR)/html" $(SPHINXOPTS) $(O) \
		--watch ../../include \
		--watch ../../src \
		--ignore "*~" \
		--ignore "*.swp" \
		--ignore "*.swo"

.PHONY: linkcheck
linkcheck:
	$(SPHINXBUILD) -b linkcheck $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/linkcheck"
	@echo
	@echo "Link check complete; look for any errors in the above output " \
	      "or in $(BUILDDIR)/linkcheck/output.txt."

.PHONY: spelling
spelling:
	$(SPHINXBUILD) -b spelling $(SPHINXOPTS) "$(SOURCEDIR)" "$(BUILDDIR)/spelling"
	@echo
	@echo "Spell check complete; look for any errors in the above output " \
	      "or in $(BUILDDIR)/spelling/output.txt."