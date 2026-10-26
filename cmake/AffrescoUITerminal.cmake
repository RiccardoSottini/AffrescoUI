# The VT terminal emulator of the format's `terminal` node (modules/format/src/terminal), included by the root
# CMakeLists.txt: libvterm, downloaded as a release archive pinned by its hash, its sources compiled into the format
# module (its own Makefile isn't run). Its generated tables (src/encoding/*.inc, src/fullwidth.inc) ship in the archive.
set(AFFRESCOUI_VTERM_VERSION 0.3.3)
FetchContent_Declare(vterm
	# Then Debian's copy of the same archive, with the same hash, when Launchpad doesn't answer: CMake tries each in turn
	URL https://launchpad.net/libvterm/trunk/v0.3/+download/libvterm-${AFFRESCOUI_VTERM_VERSION}.tar.gz
		https://deb.debian.org/debian/pool/main/libv/libvterm/libvterm_${AFFRESCOUI_VTERM_VERSION}.orig.tar.gz
	URL_HASH SHA256=09156f43dd2128bd347cbeebe50d9a571d32c64e0cf18d211197946aff7226e0
	SOURCE_SUBDIR affrescoui-compiles-its-sources SYSTEM) # a folder it doesn't have: it has no CMakeLists.txt anyway
FetchContent_MakeAvailable(vterm)

set(AFFRESCOUI_VTERM_DIR ${vterm_SOURCE_DIR})
set(AFFRESCOUI_VTERM_SOURCES)
foreach(name encoding keyboard mouse pen screen unicode vterm)
	list(APPEND AFFRESCOUI_VTERM_SOURCES ${vterm_SOURCE_DIR}/src/${name}.c)
endforeach()

# Fixes one of its sources, which the next fixes of that source build on. Stops if the source no longer has the text
# the fix replaces.
# name: the source, in src/; before: the text the fix replaces; after: the text it puts there.
function(affrescoui_fix_vterm name before after)
	get_property(source GLOBAL PROPERTY AFFRESCOUI_VTERM_FIXED_${name}) # its fixes so far, this configure
	if(NOT source)
		file(READ ${vterm_SOURCE_DIR}/src/${name} source)
	endif()
	string(FIND "${source}" "${before}" at)
	if(at EQUAL -1)
		message(FATAL_ERROR "libvterm's src/${name} has changed: check its fix in cmake/AffrescoUITerminal.cmake")
	endif()
	string(REPLACE "${before}" "${after}" source "${source}")
	set_property(GLOBAL PROPERTY AFFRESCOUI_VTERM_FIXED_${name} "${source}")
endfunction()

# Found by the terminal fuzzer (tests/fuzz/terminal.cpp). A write that ended after ESC and a control, inside a string
# (a title), gave the string's last piece the length -1, read past the write.
affrescoui_fix_vterm(parser.c [=[
    if(vt->parser.in_esc)
      string_len -= 1;
    string_fragment(vt, string_start, string_len, false);]=] [=[
    if(vt->parser.in_esc && string_len) // AffrescoUI: the ESC may be in an earlier write
      string_len -= 1;
    string_fragment(vt, string_start, string_len, false);]=])
# A long number in an escape sequence overflowed (a long may have 32 bits): one past 5 digits takes no more.
affrescoui_fix_vterm(parser.c [=[
        vt->parser.v.csi.args[vt->parser.v.csi.argi] *= 10;
        vt->parser.v.csi.args[vt->parser.v.csi.argi] += c - '0';]=] [=[
        if(vt->parser.v.csi.args[vt->parser.v.csi.argi] >= 100000) // AffrescoUI: no overflow
          break;
        vt->parser.v.csi.args[vt->parser.v.csi.argi] *= 10;
        vt->parser.v.csi.args[vt->parser.v.csi.argi] += c - '0';]=])
affrescoui_fix_vterm(parser.c [=[
        else
          vt->parser.v.osc.command *= 10;]=] [=[
        else if(vt->parser.v.osc.command >= 100000) // AffrescoUI: no overflow
          break;
        else
          vt->parser.v.osc.command *= 10;]=])
# A CSI sequence's arguments past its 16th were written past their array: they replace the 16th.
affrescoui_fix_vterm(parser.c [=[
      if(c == ';') {
        vt->parser.v.csi.argi++;]=] [=[
      if(c == ';') {
        if(vt->parser.v.csi.argi < CSI_ARGS_MAX - 1) // AffrescoUI: within the array
          vt->parser.v.csi.argi++;]=])
# A C1 control written in UTF-8 (U+0080 to U+009F) has the width -1: it moved the cursor off the screen, where a tab
# stop set next wrote outside its table. It covers one column (the terminal node shows it as U+FFFD).
affrescoui_fix_vterm(state.c [=[
      int this_width = vterm_unicode_width(codepoints[i]);]=] [=[
      int this_width = vterm_unicode_width(codepoints[i]);
      if(this_width < 0) // AffrescoUI: a C1 control covers one column
        this_width = 1;]=])
# A resize kept the scroll region's top and left: a screen made shorter or narrower than them left an upside down
# region, which scrolled a negative number of lines. Such a region is the whole screen, as DECSTBM and DECSLRM make it.
affrescoui_fix_vterm(state.c [=[
  if(state->scrollregion_right > -1)
    UBOUND(state->scrollregion_right, state->cols);
]=] [=[
  if(state->scrollregion_right > -1)
    UBOUND(state->scrollregion_right, state->cols);
  if(SCROLLREGION_BOTTOM(state) <= state->scrollregion_top) { // AffrescoUI: never upside down
    state->scrollregion_top = 0;
    state->scrollregion_bottom = -1;
  }
  if(SCROLLREGION_RIGHT(state) <= state->scrollregion_left) {
    state->scrollregion_left = 0;
    state->scrollregion_right = -1;
  }
]=])
# REP repeated the last glyph by stepping its width: a glyph of no width (before any glyph, or a zero-width space)
# never reached the column, and the loop never ended; a wide glyph was put in the row's last column, its second half
# past the row. A glyph of no width isn't repeated, and a repeat ends where the glyph no longer fits.
affrescoui_fix_vterm(state.c [=[
    while (state->pos.col < col) {]=] [=[
    while (state->combine_width > 0 && state->pos.col < col && // AffrescoUI: it ends, within the row
           state->pos.col + state->combine_width <= row_width) {]=])
# A status request (DECRQSS) of bytes past 0x7F shifted negative chars (undefined behaviour): its bytes are read as
# unsigned.
affrescoui_fix_vterm(state.c [=[
  switch(tmp[0] | tmp[1]<<8 | tmp[2]<<16) {]=] [=[
  switch((unsigned char)tmp[0] | (unsigned char)tmp[1]<<8 | (unsigned char)tmp[2]<<16) { // AffrescoUI: no sign]=])
# Restoring the cursor (DECRC, or leaving the alternate screen) put it where it was saved, past the screen when the
# terminal had become smaller since: it is kept on the screen.
affrescoui_fix_vterm(state.c [=[
    state->pos = state->saved.pos;]=] [=[
    state->pos = state->saved.pos; // AffrescoUI: on the screen, which may have shrunk since
    if(state->pos.row >= state->rows) state->pos.row = state->rows - 1;
    if(state->pos.col >= state->cols) state->pos.col = state->cols - 1;]=])

# The fixed copies, in the build folder, are compiled instead (they include the originals' headers:
# modules/format/CMakeLists.txt), each written only when it changes, so that it isn't compiled again each time
foreach(name parser.c state.c)
	get_property(source GLOBAL PROPERTY AFFRESCOUI_VTERM_FIXED_${name})
	set(fixed ${CMAKE_BINARY_DIR}/affrescoui-vterm/${name})
	set(old "")
	if(EXISTS ${fixed})
		file(READ ${fixed} old)
	endif()
	if(NOT old STREQUAL source)
		file(WRITE ${fixed} "${source}")
	endif()
	list(APPEND AFFRESCOUI_VTERM_SOURCES ${fixed})
endforeach()
