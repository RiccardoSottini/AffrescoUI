#pragma once

/// @file
/// @brief Everything in sGLGUI. Each module can also be included on its own: sgl/core (the interfaces between
/// modules), sgl/render, sgl/format, sgl/vcs, sgl/channel.

#include "sgl/core/Channel.hpp"
#include "sgl/core/DocumentRules.hpp"
#include "sgl/core/Executor.hpp"
#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"
#include "sgl/core/VersionControl.hpp"

#include "sgl/render/Application.hpp"
#include "sgl/render/Color.hpp"
#include "sgl/render/Events.hpp"
#include "sgl/render/Font.hpp"
#include "sgl/render/Geometry.hpp"
#include "sgl/render/Input.hpp"
#include "sgl/render/Label.hpp"
#include "sgl/render/Length.hpp"
#include "sgl/render/Painter.hpp"
#include "sgl/render/Panel.hpp"
#include "sgl/render/Stack.hpp"
#include "sgl/render/TextArea.hpp"
#include "sgl/render/Window.hpp"

#include "sgl/channel/Channels.hpp"
#include "sgl/channel/Session.hpp"
#include "sgl/channel/Sockets.hpp"
#include "sgl/format/Rules.hpp"
#include "sgl/format/Runtime.hpp"
#include "sgl/vcs/Repository.hpp"

#include "sgl/Screen.hpp"
