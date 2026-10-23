# format: the JSON screen format (rules, the checker, expressions, and the runtime: its engine, subsystems and
# registries; README.md)
affrescoui_module(format
	src/Sections.cpp
	src/CapabilitySections.cpp
	src/Catalog.cpp
	src/Reference.cpp
	src/ReferenceNodes.cpp
	src/ReferenceSections.cpp
	src/ReferenceTables.cpp
	src/DesignTokens.cpp
	src/ConnectionAuth.cpp
	src/ConnectionSpec.cpp
	src/Connections.cpp
	src/BuiltInComponents.cpp
	src/Properties.cpp
	src/AccessibilityProperties.cpp
	src/PropertyValues.cpp
	src/LayoutProperties.cpp
	src/TypeRegistry.cpp
	src/Types.cpp
	src/EditorType.cpp
	src/SyntaxHighlighter.cpp
	src/DiffType.cpp
	src/ToggleType.cpp
	src/SliderType.cpp
	src/ImageType.cpp
	src/SurfaceType.cpp
	src/SplitType.cpp
	src/TabsType.cpp
	src/ListType.cpp
	src/IconProperty.cpp
	src/TransformProperties.cpp
	src/RunsProperty.cpp
	src/PathType.cpp
	src/CreativeValues.cpp
	src/ColorPickerType.cpp
	src/GradientType.cpp
	src/CurvesType.cpp
	src/CanvasType.cpp
	src/GraphType.cpp
	src/TimelineType.cpp
	src/CalendarType.cpp
	src/TextFieldProperties.cpp
	src/CodeInputType.cpp
	src/CarouselType.cpp
	src/BuiltInFormComponents.cpp
	src/Contributions.cpp
	src/AuthSteps.cpp
	src/FeatureManifest.cpp
	src/Composition.cpp
	src/ComposedVersions.cpp
	src/FeatureSignatures.cpp
	src/FeatureCatalog.cpp
	src/Features.cpp
	src/ContributionPoints.cpp
	src/AdapterMapping.cpp
	src/CompositionProof.cpp
	src/adapters/Layout.cpp
	src/adapters/A2ui.cpp
	src/adapters/A2uiComponents.cpp
	src/adapters/AdaptiveCards.cpp
	src/adapters/AdaptiveCardActions.cpp
	src/adapters/Adapters.cpp
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
	src/steps/ServiceSteps.cpp)
target_include_directories(affrescoui_format PRIVATE src/steps)
# src/calls/: asynchronous and service calls
target_sources(affrescoui_format PRIVATE
	src/calls/AsyncCalls.cpp
	src/calls/ServiceCalls.cpp)
target_include_directories(affrescoui_format PRIVATE src/calls)
# widgets
