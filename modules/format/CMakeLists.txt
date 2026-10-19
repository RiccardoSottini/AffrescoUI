# format: the JSON screen format (rules, the checker, expressions, and the runtime: its engine, subsystems and
# registries; README.md)
affrescoui_module(format
	src/Expression.cpp
	src/Rules.cpp
	src/Sections.cpp
	src/CapabilitySections.cpp
	src/Runtime.cpp
	src/Engine.cpp
	src/Catalog.cpp
	src/DocumentContext.cpp
	src/Builder.cpp
	src/Reconciler.cpp
	src/Properties.cpp
	src/PropertyValues.cpp
	src/TypeRegistry.cpp
	src/Types.cpp
	src/EditorType.cpp
	src/ToggleType.cpp
	src/SliderType.cpp
	src/Events.cpp
	src/Shortcuts.cpp
	src/Scheduler.cpp
	src/Steps.cpp
	src/StepRegistry.cpp
	src/StateSteps.cpp
	src/FlowSteps.cpp
	src/OutsideSteps.cpp
	src/ReviewSteps.cpp
	src/NavigationSteps.cpp
	src/TimeSteps.cpp
	src/OtherSteps.cpp
	src/Checker.cpp
	src/CheckerNodes.cpp
	src/CheckerValues.cpp)
target_link_libraries(affrescoui_format PUBLIC affrescoui::core affrescoui::render nlohmann_json::nlohmann_json)
