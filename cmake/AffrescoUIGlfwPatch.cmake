# The one change AffrescoUI makes to GLFW 3.4 (zlib licence: an altered source, marked as such), run by
# FetchContent's PATCH_COMMAND in CMakeLists.txt on the unpacked archive: cmake -DFILE=<src/nsgl_context.m> -P <this>.
#
# On macOS, GLFW asks only for a hardware-accelerated OpenGL pixel format. A Mac without a usable GPU (a virtual
# machine, such as the hosted CI runners) has none, so every window failed to open with "NSGL: Failed to find a
# suitable pixel format", though Apple's software renderer offers OpenGL 4.1 core there. With this change, when no
# accelerated format matches, the same attributes are asked for once more without NSOpenGLPFAAccelerated (always the
# first attribute), so the software renderer is used. A Mac with a GPU gets exactly the format it got before.

if(NOT FILE)
	message(FATAL_ERROR "AffrescoUIGlfwPatch.cmake: pass -DFILE=<GLFW's src/nsgl_context.m>")
endif()
file(READ "${FILE}" text)
set(marker "AffrescoUI: without an accelerated format")
string(FIND "${text}" "${marker}" patched)
if(NOT patched EQUAL -1)
	return() # already patched (FetchContent can run the step again on the same folder)
endif()

set(first "    ADD_ATTRIB(NSOpenGLPFAAccelerated);\n    ADD_ATTRIB(NSOpenGLPFAClosestPolicy);\n")
set(original [=[
    window->context.nsgl.pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    if (window->context.nsgl.pixelFormat == nil)
]=])
set(replacement [=[
    window->context.nsgl.pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
    // AffrescoUI: without an accelerated format (no GPU, as in a virtual machine), the same attributes without
    // NSOpenGLPFAAccelerated, attribs[0]: Apple's software renderer
    if (window->context.nsgl.pixelFormat == nil)
    {
        window->context.nsgl.pixelFormat =
            [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs + 1];
    }
    if (window->context.nsgl.pixelFormat == nil)
]=])
string(FIND "${text}" "${first}" at_first)
string(FIND "${text}" "${original}" at_original)
if(NOT at_first EQUAL -1 AND NOT at_original EQUAL -1)
	string(REPLACE "${original}" "${replacement}" text "${text}")
	file(WRITE "${FILE}" "${text}")
else()
	message(FATAL_ERROR "AffrescoUIGlfwPatch.cmake: ${FILE} isn't GLFW 3.4's: the change can't be made")
endif()
