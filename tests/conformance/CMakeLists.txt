# The conformance run (README.md): the language measured against the cross-dialect Clojure test suite. The runner and
# the everyday probes (everyday.lisp, ours) always build and run; the suite itself is fetched, at one commit and by its
# checksum, only with AFFRESCOUI_CONFORMANCE=ON, and no file of it enters this repository.
option(AFFRESCOUI_CONFORMANCE "Fetch the Clojure test suite and test the language against it" OFF)

add_executable(affrescoui_conformance runner.cpp prepare.cpp)
target_link_libraries(affrescoui_conformance PRIVATE affrescoui::lisp)
target_compile_options(affrescoui_conformance PRIVATE ${AFFRESCOUI_WARNINGS})
# --min: the probes that give Clojure's value today, a ratchet; the three gaps left are two sets, which the reader doesn't
# take yet, and a vector called as a function
add_test(NAME cli_lisp_everyday COMMAND affrescoui_conformance --everyday ${CMAKE_CURRENT_SOURCE_DIR}/everyday.lisp
	--min 160)

if(AFFRESCOUI_CONFORMANCE)
	# github.com/jank-lang/clojure-test-suite (MPL-2.0), its main branch at a9f32fd7, 2026-11-14. To bump it: README.md.
	# SOURCE_SUBDIR names a folder it hasn't, so nothing of it is built: its test files are only read
	FetchContent_Declare(clojure_test_suite
		URL https://github.com/jank-lang/clojure-test-suite/archive/a9f32fd7bf0cf3ef04eb62db31a731bc7dcb6dc6.tar.gz
		URL_HASH SHA256=ba2ff3c8ca6d2a1eb3fce134a0ea716d502b85866eece01c3fb1d7e18b93b807 SOURCE_SUBDIR none)
	FetchContent_MakeAvailable(clojure_test_suite)
	find_package(Python3 COMPONENTS Interpreter REQUIRED)
	add_test(NAME cli_lisp_conformance COMMAND Python3::Interpreter -I ${CMAKE_CURRENT_SOURCE_DIR}/run.py --check
		--runner $<TARGET_FILE:affrescoui_conformance> --suite ${clojure_test_suite_SOURCE_DIR}/test)
endif()
