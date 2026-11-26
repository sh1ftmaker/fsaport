# make compile
#
# Needs emscripten on PATH (source ../fsadecomp/tools/emsdk/emsdk_env.sh).

JOBS ?= $(shell nproc)

.PHONY: compile assets undefined clean

compile: assets
	python3 tools/compile_check.py -j $(JOBS)
	python3 tools/undef_report.py > build/undefined.txt
	@head -1 build/undefined.txt

assets: build/generated/globals.c

# The storage of the game's global data that no unit defines (no game bytes:
# the game fills it from the player's main.dol at startup).
build/generated/globals.c: tools/gen_globals.py tools/globals_layout.json
	python3 tools/gen_globals.py emit build/generated

clean:
	rm -rf build
