# make compile DOL=path/to/your/main.dol
#
# Needs emscripten on PATH (source ../fsadecomp/tools/emsdk/emsdk_env.sh).

DOL ?= ../fsadecomp/fsa-decomp/orig/sys/main.dol
JOBS ?= $(shell nproc)

.PHONY: compile assets undefined clean

compile: assets
	python3 tools/compile_check.py -j $(JOBS)
	python3 tools/undef_report.py > build/undefined.txt
	@head -1 build/undefined.txt

assets: build/assets/assets/JSystem/JUtility/Ascfont_fix12.inc

build/assets/assets/JSystem/JUtility/Ascfont_fix12.inc: tools/extract_assets.py
	python3 tools/extract_assets.py $(DOL) build/assets

clean:
	rm -rf build
