MOZJS=../../src/mozjs-24.2.0

help:
	@echo "Options:"
	@echo "  make build     - Creates a standard build using system libraries"
	@echo "  make src-build - Creates a build using a local source tree (at $(MOZJS))"
	@echo "  make clean     - Removes build directory"

build:
	python setup.py build

clean:
	rm -rf build

src-build:
	python setup.py --mozjs-source $(MOZJS) build 
