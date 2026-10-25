# Comparisons

AffrescoUI is young, and most of the tools below are mature, widely used and better at many things. This page says
what each is, where it is stronger, and where AffrescoUI differs. Facts about the others come from their own
documentation, linked in each section; facts about AffrescoUI are about the code this site was built from.

## What AffrescoUI is for

A native C++ engine whose whole UI, behaviour included, is a JSON document that people and AI agents change while the
app runs, under version control and permission rules, aimed at large apps such as IDEs and creative tools. Its
distinctive parts are the live revisions, the review of an agent's change before it applies, and the history of every
change. It runs on macOS, Linux and Windows and in the browser (WebAssembly); CI builds and tests it on Linux and
Windows and builds it on macOS, where it is developed. Screen readers reach it through each platform's accessibility
API (VoiceOver, Narrator, Orca). It is used from C++, C and Python, and its ecosystem is small.

## Web views and Electron

[Electron](https://www.electronjs.org/docs/latest/) builds desktop apps with JavaScript, HTML and CSS by embedding
Chromium and Node.js, for Windows, macOS and Linux. Web views in general give an app the web platform.

**Where they are better:** the largest UI ecosystem there is (libraries, components, developers, tooling), mature
accessibility and text input, all desktop platforms, and the same code on the web. An agent can already generate HTML
and JavaScript well.

**Where AffrescoUI differs:** no browser engine and no web view. An agent's change is a checked JSON
revision that permission rules judge and a person can review and revert, rather than code that can do anything a page
can. The screen reaches the outside world only through capabilities the host grants.

## Qt and QML

[Qt](https://doc.qt.io/) is a cross-platform C++ framework; [QML](https://doc.qt.io/qt-6/qmlapplications.html) is its
declarative language for user interfaces, with JavaScript expressions and property bindings. It runs on desktop,
mobile and embedded systems, under commercial and open-source licences.

**Where it is better:** decades of maturity, every major platform, accessibility, internationalisation, a large set of
modules (multimedia, networking, 3D, charts), design tools and commercial support. QML's bindings and components cover
much of what AffrescoUI's format does, with far more widgets.

**Where AffrescoUI differs:** a screen is data, not a language with scripts, so it can be validated against a schema,
checked for references that don't resolve, diffed and applied as atomic revisions with permissions and review while the
app runs. Apache-2.0 for every use.

## Dear ImGui

[Dear ImGui](https://github.com/ocornut/imgui) is an immediate-mode GUI library for C++: code describes the UI every
frame, with few dependencies and any renderer. It is MIT-licensed and popular for tools, editors and debug UIs.

**Where it is better:** very little to learn or integrate, the UI lives next to the code that uses it, many backends and
platforms, and a large community of tool builders.

**Where AffrescoUI differs:** a retained, declarative document that is redrawn only where something changed (an idle
window sleeps), styled with themes and tokens, and changeable without recompiling, by people and agents, under review. ImGui's
UI is code, so changing it means changing and rebuilding the program.

## Flutter

[Flutter](https://docs.flutter.dev/) is Google's UI toolkit: apps written in Dart, drawn by its own engine, for mobile,
web, desktop and embedded from one codebase, with hot reload during development. It is BSD-licensed.

**Where it is better:** every major platform including mobile and the web, a large widget library and package
ecosystem, accessibility, mature tooling, and a big community.

**Where AffrescoUI differs:** the UI is a document an agent changes in the running, released app, not Dart compiled
ahead of time; hot reload is a development feature, while AffrescoUI's live changes are its runtime model, with
permissions and history. It is embedded from C++, C or Python.

## A2UI

[A2UI](https://a2ui.org/) ([repository](https://github.com/google/A2UI)) is an open standard from Google for agents to
send user interfaces as declarative JSON. The client renders them with its own catalog of trusted components, and
updates can stream. Renderers include Flutter's GenUI.

**Where it is better:** an open protocol meant to be rendered by many clients (web, mobile, desktop) with their own
native components, designed for an agent to send UI into a conversation or another app.

**Where AffrescoUI differs:** it is not a protocol for many renderers but one engine for a whole application. The
document holds the app's behaviour too (state, actions, navigation, timers), and changes to it are versioned, reviewed
and revertible. The two ideas share the core point: agents send data that a trusted client renders, never code.
`affrescoui import --from a2ui` converts an A2UI surface into a screen.

## Adaptive Cards

[Adaptive Cards](https://learn.microsoft.com/en-us/adaptive-cards/) are platform-agnostic UI snippets in JSON that
the applications hosting them render natively with their own look. They are declarative, with templating
and SDKs for JavaScript, .NET, Android and iOS.

**Where they are better:** an established format with many hosts, designers and SDKs, ideal for cards exchanged
between services and shown inside other apps.

**Where AffrescoUI differs:** it describes a whole application rather than a card, with layout, state, behaviour and
navigation, and its changes are revisions under permissions and version control. `affrescoui import --from
adaptive-cards` converts a card into a screen.

## Performance

The repository's `BENCHMARKS.md` reports AffrescoUI's own measurements, the targets it misses included (a full frame
in 0.44 ms, on the one machine it was measured on). There are no comparative benchmarks against the tools above, so this page makes no claims
about which is faster.
