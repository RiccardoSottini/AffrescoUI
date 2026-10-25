#pragma once

/// @file
/// @brief Everything in AffrescoUI. Each module can also be included on its own: affresco/core (the interfaces between
/// modules), affresco/render, affresco/format, affresco/vcs, affresco/channel.

#include "affresco/core/Channel.hpp"
#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/core/Json.hpp"
#include "affresco/core/Log.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/core/VersionControl.hpp"

#include "affresco/render/Accessibility.hpp"
#include "affresco/render/Application.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Controls.hpp"
#include "affresco/render/Events.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Geometry.hpp"
#include "affresco/render/Input.hpp"
#include "affresco/render/Label.hpp"
#include "affresco/render/Length.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/Stack.hpp"
#include "affresco/render/TextArea.hpp"
#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Window.hpp"

#include "affresco/channel/Channels.hpp"
#include "affresco/channel/Session.hpp"
#include "affresco/channel/Sockets.hpp"
#include "affresco/format/Rules.hpp"
#include "affresco/format/Runtime.hpp"
#include "affresco/themes/Themes.hpp"
#include "affresco/vcs/Repository.hpp"

#include "affresco/Screen.hpp"
