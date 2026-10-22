# The shipped templates' folders in examples/, as examples/templates.json lists them (examples/README.md): the rendering
# check plays their scenarios, and the browser build preloads them.

# Sets <variable> to the templates' folders (e.g. "flow"), in the order of their names; a change to the list
# configures again.
function(affrescoui_template_folders variable)
	set(file ${PROJECT_SOURCE_DIR}/examples/templates.json)
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${file})
	file(READ ${file} text)
	string(JSON count LENGTH "${text}" templates)
	math(EXPR last "${count} - 1")
	set(folders)
	foreach(index RANGE ${last})
		string(JSON name MEMBER "${text}" templates ${index})
		string(JSON folder GET "${text}" templates ${name})
		list(APPEND folders ${folder})
	endforeach()
	set(${variable} ${folders} PARENT_SCOPE)
endfunction()
