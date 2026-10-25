# themes: the kits' components and the themes' design tokens, as JSON files built into the library, offered to every
# document through core's ComponentLibrary (README.md here). Uses core. The format never links it: affresco::Screen
# (the composition root) and the other entry points install it.

# The JSON files, in the order a document's lookups see them; a new kit or theme is one more line here
set(AFFRESCOUI_THEMES_FILES
	components/core.json components/form.json components/bars.json components/overlays.json
	components/controls.json components/dock.json components/panels.json components/chat.json
	tokens/kit.json)
list(TRANSFORM AFFRESCOUI_THEMES_FILES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/ OUTPUT_VARIABLE themes_inputs)
set(themes_embedded ${CMAKE_CURRENT_BINARY_DIR}/Embedded.cpp)
add_custom_command(OUTPUT ${themes_embedded}
	COMMAND ${CMAKE_COMMAND} -DBASE=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT=${themes_embedded}
		"-DFILES=${AFFRESCOUI_THEMES_FILES}" -P ${CMAKE_CURRENT_SOURCE_DIR}/src/Embed.cmake
	DEPENDS ${themes_inputs} ${CMAKE_CURRENT_SOURCE_DIR}/src/Embed.cmake
	COMMENT "Embedding the themes' JSON files" VERBATIM)

affrescoui_module(themes src/Themes.cpp ${themes_embedded})
target_link_libraries(affrescoui_themes PUBLIC affrescoui::core nlohmann_json::nlohmann_json)
