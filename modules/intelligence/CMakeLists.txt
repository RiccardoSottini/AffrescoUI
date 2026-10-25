# intelligence: the providers of intelligence a host's assistant uses (README.md here): their catalogue (providers.json,
# built in), the person's settings and keys, the settings' natives and the agent connected to an app's screen. Uses
# the library (affresco::Screen), net for the providers' APIs and services to run a command-line provider.

# The catalogue, built in as text: a changed providers.json configures again
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/providers.json)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/providers.json AFFRESCOUI_PROVIDERS_JSON)
configure_file(src/Catalog.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Catalog.hpp @ONLY)

affrescoui_module(intelligence
	src/Settings.cpp      # the catalogue, the settings in intelligence.json, where keys are kept
	src/Http.cpp          # HTTP to one origin from a worker thread
	src/Providers.cpp     # what the API providers are sent and answer
	src/ScreenQueries.cpp # the screen as an agent reads it, and the format guide by topic
	src/ClaudeCode.cpp    # the claude-code provider: the person's command line, with no tools
	src/Agent.cpp         # the agent inside the host
	src/Intelligence.cpp) # the settings' natives, assistant.models and HostAssistant
target_include_directories(affrescoui_intelligence PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(affrescoui_intelligence PUBLIC affrescoui::affrescoui)
