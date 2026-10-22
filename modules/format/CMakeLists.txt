# format: the JSON screen format (rules, the checker, expressions, and the runtime: its engine, subsystems and
# registries; README.md)
affrescoui_module(format
	src/Expression.cpp
	src/ExpressionParser.cpp
	src/ExpressionLower.cpp
	src/ExpressionMachine.cpp
	src/ExpressionFunctions.cpp
	src/ExpressionFunctionsLogic.cpp
	src/ExpressionFunctionsNumbers.cpp
	src/ExpressionFunctionsText.cpp
	src/ExpressionFunctionsLists.cpp
	src/ExpressionFunctionsItems.cpp
	src/ExpressionFunctionsTime.cpp
	src/ExpressionFunctionsDates.cpp
	src/ExpressionFunctionsFuzzy.cpp
	src/ExpressionFunctionsColor.cpp
	src/ExpressionFunctionsKeyframes.cpp
	src/ExpressionFunctionsLocale.cpp
	src/ExpressionFunctionsForm.cpp
	src/LocaleData.cpp
	src/LocalePlurals.cpp
	src/LocaleNumbers.cpp
	src/LocaleDates.cpp
	src/LocaleSystem.cpp
	src/Strings.cpp
	src/StringsMessage.cpp
	src/Value.cpp
	src/Rules.cpp
	src/BehaviourLines.cpp
	src/StateTypes.cpp
	src/Limits.cpp
	src/Runtime.cpp
	src/Engine.cpp
	src/DocumentContext.cpp
	src/ItemIndex.cpp
	src/AsyncCalls.cpp
	src/ServiceCalls.cpp
	src/ServiceRules.cpp
	src/Models.cpp
	src/Reads.cpp
	src/Binding.cpp
	src/CompiledNode.cpp
	src/Builder.cpp
	src/Reconciler.cpp
	src/ListItems.cpp
	src/Events.cpp
	src/Shortcuts.cpp
	src/Settings.cpp
	src/Usage.cpp
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
	src/ServiceSteps.cpp
	src/Checker.cpp
	src/CheckerNodes.cpp
	src/CheckerValues.cpp
	src/CheckerTypes.cpp
	src/NodeIds.cpp
	src/Migrate.cpp
	src/RulesNodes.cpp
	${AFFRESCOUI_MONOCYPHER_SOURCES})
target_link_libraries(affrescoui_format PUBLIC affrescoui::core affrescoui::render nlohmann_json::nlohmann_json)
if(APPLE)
	target_link_libraries(affrescoui_format PRIVATE "-framework CoreFoundation") # systemLocale()
endif()
target_link_libraries(affrescoui_format PRIVATE affrescoui::services)
target_include_directories(affrescoui_format SYSTEM PRIVATE ${AFFRESCOUI_MONOCYPHER_DIR} ${AFFRESCOUI_MONOCYPHER_DIR}/optional)

# widgets: the node types and their properties, the reference, sections, connections, features, composition
# and adapters, a folder each (src/<folder>/README.md)
# types: the type registry and the node types: panels, surfaces, split, tabs, lists, diff, editor, image
target_sources(affrescoui_format PRIVATE src/types/TypeRegistry.cpp src/types/Types.cpp src/types/ImageType.cpp
	src/types/SurfaceType.cpp src/types/SplitType.cpp src/types/TabsType.cpp src/types/ListType.cpp src/types/DiffType.cpp
	src/types/EditorType.cpp src/types/SyntaxHighlighter.cpp)
# controls: the form controls: toggle, slider, calendar, code input, carousel
target_sources(affrescoui_format PRIVATE src/controls/ToggleType.cpp src/controls/SliderType.cpp
	src/controls/CalendarType.cpp src/controls/CodeInputType.cpp src/controls/CarouselType.cpp)
# components: the built-in components, made of other nodes
target_sources(affrescoui_format PRIVATE src/components/BuiltInComponents.cpp src/components/BuiltInFormComponents.cpp)
# creative: the creative types (colour picker, gradient, curves, canvas, graph, timeline, path) and their values
target_sources(affrescoui_format PRIVATE src/creative/ColorPickerType.cpp src/creative/GradientType.cpp
	src/creative/CurvesType.cpp src/creative/CreativeValues.cpp src/creative/CanvasType.cpp src/creative/GraphType.cpp
	src/creative/TimelineType.cpp src/creative/PathType.cpp)
# properties: the properties node types share: look, layout, accessibility, transform, text fields, icons, runs
target_sources(affrescoui_format PRIVATE src/properties/Properties.cpp src/properties/CommonProperties.cpp
	src/properties/PropertyValues.cpp src/properties/LayoutProperties.cpp src/properties/AccessibilityProperties.cpp
	src/properties/TransformProperties.cpp src/properties/TextFieldProperties.cpp src/properties/IconProperty.cpp
	src/properties/RunsProperty.cpp)
# reference: the reference of nodes, sections and tables (the schema, SCREEN_FORMAT.md) and the catalog
target_sources(affrescoui_format PRIVATE src/reference/Reference.cpp src/reference/ReferenceNodes.cpp
	src/reference/ReferenceNodeTypes.cpp src/reference/ReferenceNodeEvents.cpp
	src/reference/ReferenceSections.cpp src/reference/ReferenceTables.cpp src/reference/Catalog.cpp)
# sections: the section registry, the sections, the capability sections and the design tokens
target_sources(affrescoui_format PRIVATE src/sections/Sections.cpp src/sections/CapabilitySections.cpp
	src/sections/DesignTokens.cpp)
# connections: connections (RFC 0003): their specs, their HTTP and socket calls, signing in and its steps
target_sources(affrescoui_format PRIVATE src/connections/ConnectionSpec.cpp src/connections/Connections.cpp
	src/connections/ConnectionAuth.cpp src/connections/AuthSteps.cpp)
# features: features (RFC 0004): manifests, catalogues, signatures, trusted publishers
target_sources(affrescoui_format PRIVATE src/features/FeatureManifest.cpp src/features/FeatureCatalog.cpp
	src/features/FeatureSignatures.cpp src/features/Features.cpp)
# composition: features merged into a screen and proven, contribution points, contributions (RFC 0001), versions
target_sources(affrescoui_format PRIVATE src/composition/Composition.cpp src/composition/ComposedVersions.cpp
	src/composition/CompositionProof.cpp src/composition/ContributionPoints.cpp src/composition/Contributions.cpp)
# adapters: other UI formats (A2UI, Adaptive Cards) read into screens, and a feature's needs bound to an app
target_sources(affrescoui_format PRIVATE src/adapters/Layout.cpp src/adapters/A2ui.cpp src/adapters/A2uiComponents.cpp
	src/adapters/AdaptiveCards.cpp src/adapters/AdaptiveCardActions.cpp src/adapters/Adapters.cpp
	src/adapters/AdapterMapping.cpp)
target_include_directories(affrescoui_format PRIVATE src/types src/controls src/components src/creative src/properties
	src/reference src/sections src/connections src/features src/composition src/adapters)
