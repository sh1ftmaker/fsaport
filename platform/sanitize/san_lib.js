// FSA_SANITIZE=address only (--js-library): emscripten's emscripten_pc_get_file
// passes an undefined file name to stringToNewUTF8 when the source map has a
// segment without a source (Aurora's third-party code), which threw out of the
// ASan report and killed the game thread. Same function, minus that.
addToLibrary({
  emscripten_pc_get_file__deps: ['$convertPCtoSourceLocation', 'free', '$stringToNewUTF8'],
  emscripten_pc_get_file__noleakcheck: true,
  emscripten_pc_get_file: (pc) => {
    var result = convertPCtoSourceLocation(pc);
    if (!result || !result.file) return 0;
    _free(_emscripten_pc_get_file.ret ?? 0);
    _emscripten_pc_get_file.ret = stringToNewUTF8(result.file);
    return _emscripten_pc_get_file.ret;
  },
});
