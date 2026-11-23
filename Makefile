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

assets: build/assets/assets/JSystem/JUtility/Ascfont_fix12.inc build/generated/globals.c

build/assets/assets/JSystem/JUtility/Ascfont_fix12.inc: tools/extract_assets.py
	python3 tools/extract_assets.py $(DOL) build/assets

# The game's global data that no unit defines, filled in from your main.dol.
build/generated/globals.c: tools/gen_globals.py tools/globals_layout.json
	python3 tools/gen_globals.py emit $(DOL) build/generated

clean:
	rm -rf build
