# An application that uses an installed AffrescoUI, the way find_package users do. check.py installs AffrescoUI to
# a temporary prefix and builds this against it; AFFRESCOUI_VERSION is the version it asks for.
cmake_minimum_required(VERSION 3.25)
project(AffrescoUIPackageTest LANGUAGES CXX)

set(AFFRESCOUI_VERSION 0.1 CACHE STRING "The AffrescoUI version find_package asks for")
find_package(AffrescoUI ${AFFRESCOUI_VERSION} CONFIG REQUIRED)

# The whole library, GLFW included (the static render library leaves it to this link)
add_executable(screen_consumer screen_consumer.cpp)
target_link_libraries(screen_consumer PRIVATE affrescoui::affrescoui)

# One module on its own: version control, with no window or OpenGL
add_executable(vcs_consumer vcs_consumer.cpp)
target_link_libraries(vcs_consumer PRIVATE affrescoui::vcs)
