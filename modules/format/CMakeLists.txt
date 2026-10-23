# format: the JSON screen format (rules, the checker, expressions, and the runtime: its engine, subsystems and
# registries; README.md)
affrescoui_module(format
	${AFFRESCOUI_MONOCYPHER_SOURCES})
target_link_libraries(affrescoui_format PUBLIC affrescoui::core affrescoui::render nlohmann_json::nlohmann_json)
if(APPLE)
	target_link_libraries(affrescoui_format PRIVATE "-framework CoreFoundation") # systemLocale()
endif()
target_link_libraries(affrescoui_format PRIVATE affrescoui::services)
target_include_directories(affrescoui_format SYSTEM PRIVATE ${AFFRESCOUI_MONOCYPHER_DIR} ${AFFRESCOUI_MONOCYPHER_DIR}/optional)

# src/expression/: the expression language: parser, lowering to typed nodes, the machine, and its values
target_sources(affrescoui_format PRIVATE
	src/expression/Expression.cpp
	src/expression/ExpressionParser.cpp
	src/expression/ExpressionLower.cpp
	src/expression/ExpressionMachine.cpp
	src/expression/Value.cpp)
target_include_directories(affrescoui_format PRIVATE src/expression)
# src/functions/: the expression functions, by family
target_sources(affrescoui_format PRIVATE
	src/functions/ExpressionFunctions.cpp
	src/functions/ExpressionFunctionsColor.cpp
	src/functions/ExpressionFunctionsDates.cpp
	src/functions/ExpressionFunctionsForm.cpp
	src/functions/ExpressionFunctionsFuzzy.cpp
	src/functions/ExpressionFunctionsGeometry.cpp
	src/functions/ExpressionFunctionsItems.cpp
	src/functions/ExpressionFunctionsKeyframes.cpp
	src/functions/ExpressionFunctionsLists.cpp
	src/functions/ExpressionFunctionsLocale.cpp
	src/functions/ExpressionFunctionsLogic.cpp
	src/functions/ExpressionFunctionsNumbers.cpp
	src/functions/ExpressionFunctionsText.cpp
	src/functions/ExpressionFunctionsTime.cpp)
target_include_directories(affrescoui_format PRIVATE src/functions)
# src/locale/: locale data (CLDR subset), number, date and plural formatting, and the strings tables
target_sources(affrescoui_format PRIVATE
	src/locale/LocaleData.cpp
	src/locale/LocaleDates.cpp
	src/locale/LocaleNumbers.cpp
	src/locale/LocalePlurals.cpp
	src/locale/LocaleSystem.cpp
	src/locale/Strings.cpp
	src/locale/StringsMessage.cpp)
target_include_directories(affrescoui_format PRIVATE src/locale)
# src/checker/: shapes, state types, and the node, value and type checks
target_sources(affrescoui_format PRIVATE
	src/checker/Checker.cpp
	src/checker/CheckerNodes.cpp
	src/checker/CheckerTypes.cpp
	src/checker/CheckerValues.cpp
	src/checker/StateTypes.cpp)
target_include_directories(affrescoui_format PRIVATE src/checker)
# src/rules/: limits, node ids, behaviour lines, service rules and migrations
target_sources(affrescoui_format PRIVATE
	src/rules/Rules.cpp
	src/rules/RulesNodes.cpp
	src/rules/RulesControl.cpp
	src/rules/BehaviourLines.cpp
	src/rules/NodeIds.cpp
	src/rules/Limits.cpp
	src/rules/Migrate.cpp
	src/rules/ServiceRules.cpp)
target_include_directories(affrescoui_format PRIVATE src/rules)
# src/runtime/: the engine, its document context, settings, scheduler and usage
target_sources(affrescoui_format PRIVATE
	src/runtime/Runtime.cpp
	src/runtime/Engine.cpp
	src/runtime/DocumentContext.cpp
	src/runtime/Settings.cpp
	src/runtime/Scheduler.cpp
	src/runtime/Usage.cpp)
target_include_directories(affrescoui_format PRIVATE src/runtime)
# src/nodes/: compiled nodes: the builder, bindings, node keys and records
target_sources(affrescoui_format PRIVATE
	src/nodes/Builder.cpp
	src/nodes/CompiledNode.cpp
	src/nodes/Binding.cpp)
target_include_directories(affrescoui_format PRIVATE src/nodes)
# src/updates/: reads, models, list items, the item index and the reconciler
target_sources(affrescoui_format PRIVATE
	src/updates/Reconciler.cpp
	src/updates/Reads.cpp
	src/updates/ItemIndex.cpp
	src/updates/Models.cpp
	src/updates/ListItems.cpp)
target_include_directories(affrescoui_format PRIVATE src/updates)
# src/events/: pointer, keys, focus and shortcuts
target_sources(affrescoui_format PRIVATE
	src/events/Events.cpp
	src/events/EventsPointer.cpp
	src/events/EventsKeys.cpp
	src/events/EventsFocus.cpp
	src/events/Shortcuts.cpp)
target_include_directories(affrescoui_format PRIVATE src/events)
# src/steps/: the registry and the step groups
target_sources(affrescoui_format PRIVATE
	src/steps/Steps.cpp
	src/steps/StepRegistry.cpp
	src/steps/StateSteps.cpp
	src/steps/FlowSteps.cpp
	src/steps/OutsideSteps.cpp
	src/steps/ReviewSteps.cpp
	src/steps/NavigationSteps.cpp
	src/steps/TimeSteps.cpp
	src/steps/OtherSteps.cpp
	src/steps/ServiceSteps.cpp
	src/steps/SceneSteps.cpp)
target_include_directories(affrescoui_format PRIVATE src/steps)
# src/calls/: asynchronous and service calls
target_sources(affrescoui_format PRIVATE
	src/calls/AsyncCalls.cpp
	src/calls/ServiceCalls.cpp)
target_include_directories(affrescoui_format PRIVATE src/calls)
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
target_sources(affrescoui_format PRIVATE src/components/BuiltInComponents.cpp src/components/BuiltInFormComponents.cpp
	src/components/KitTokens.cpp src/components/KitBars.cpp
	src/components/KitOverlays.cpp)
# creative: the creative types (colour picker, gradient, curves, canvas, graph, timeline, path) and their values
target_sources(affrescoui_format PRIVATE src/creative/ColorPickerType.cpp src/creative/GradientType.cpp
	src/creative/CurvesType.cpp src/creative/CreativeValues.cpp src/creative/CanvasType.cpp src/creative/GraphType.cpp
	src/creative/TimelineType.cpp src/creative/PathType.cpp src/creative/SceneType.cpp)
# scene: the 3D scene node and its camera step
target_link_libraries(affrescoui_format PRIVATE affrescoui::scene3d)
# properties: the properties node types share: look, layout, accessibility, transform, text fields, icons, runs
target_sources(affrescoui_format PRIVATE src/properties/Properties.cpp src/properties/CommonProperties.cpp
	src/properties/PropertyValues.cpp src/properties/LayoutProperties.cpp src/properties/AccessibilityProperties.cpp
	src/properties/TransformProperties.cpp src/properties/TextFieldProperties.cpp src/properties/IconProperty.cpp
	src/properties/RunsProperty.cpp)
# reference: the reference of nodes, sections and tables (the schema, SCREEN_FORMAT.md) and the catalog
target_sources(affrescoui_format PRIVATE src/reference/Reference.cpp src/reference/ReferenceNodes.cpp
	src/reference/ReferenceNodeTypes.cpp src/reference/ReferenceSceneKeys.cpp src/reference/ReferenceNodeEvents.cpp
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
