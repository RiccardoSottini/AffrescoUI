/// @file
/// @brief A host application with its own OpenGL scene that embeds an AffrescoUI screen through the C API: drawn over
/// the scene (the default), or with --texture into a texture shown on a rotating quad. AFFRESCOUI_SCREENSHOT=out.ppm
/// writes the first frame and exits.

#include <affresco/affresco.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <numbers>
#include <string>
#include <vector>

// NOLINTBEGIN(modernize-use-std-print): GCC 13, which CI builds with, has no <print>

namespace {

/// @brief The scene's shader: a quad of a half size (in heights of the window) turned by an angle, then fitted to the
/// window's ratio (its height over its width).
constexpr const char* vertexShader = R"(#version 410 core
layout(location = 0) in vec2 corner;
uniform float angle;
uniform vec2 halfSize;
uniform float ratio;
out vec2 uv;
void main() {
	vec2 p = corner * halfSize;
	vec2 turned = vec2(p.x * cos(angle) - p.y * sin(angle), p.x * sin(angle) + p.y * cos(angle));
	gl_Position = vec4(turned.x * ratio, turned.y, 0.0, 1.0);
	uv = corner * 0.5 + 0.5;
})";

/// @brief The scene's fragments: a gradient, or the UI's texture (premultiplied) over a dark card.
constexpr const char* fragmentShader = R"(#version 410 core
in vec2 uv;
uniform sampler2D picture;
uniform int textured;
out vec4 color;
void main() {
	vec4 base = vec4(uv.x, 0.3, uv.y, 1.0);
	if(textured == 0) { color = base; return; }
	vec4 ui = texture(picture, uv);
	color = vec4(ui.rgb + vec3(0.06, 0.08, 0.12) * (1.0 - ui.a), 1.0);
})";

/// @brief What the host keeps: the screen, its scene, and what the UI changed.
struct Host {
	afr_screen* screen = nullptr; ///< The embedded screen.
	GLFWwindow* window = nullptr; ///< The host's window.
	GLuint program = 0;           ///< The scene's shader.
	GLuint vertices = 0;          ///< The quad's vertex array.
	GLuint buffer = 0;            ///< The quad's corners.
	GLuint blank = 0;             ///< A 1 by 1 texture, bound when the quad has no picture.
	bool texture = false;         ///< --texture: the UI on the quad rather than over the scene.
	double speed = 1.0;           ///< Turns per ten seconds, set by the UI's buttons.
	bool paused = false;          ///< state.paused, read when the screen reports a revision.
	bool stateChanged = false;    ///< The screen reported an event since the last frame.
};

/// @brief Prints a failed call's error and exits.
/// @param status What the call returned.
/// @param what The call.
void check(afr_status status, const char* what) {
	if(status == AFR_OK) return;
	std::fprintf(stderr, "embed: %s failed: %s\n", what, afr_last_error());
	std::exit(1);
}

/// @brief Compiles one shader stage.
/// @param kind GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
/// @param source Its source.
/// @return The shader.
GLuint compile(GLenum kind, const char* source) {
	const GLuint shader = glCreateShader(kind);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if(ok == 0) {
		std::array<char, 1024> log{};
		glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
		std::fprintf(stderr, "embed: shader: %s\n", log.data());
		std::exit(1);
	}
	return shader;
}

/// @brief Makes the scene's shader and quad.
/// @param host The host.
void makeScene(Host& host) {
	host.program = glCreateProgram();
	const GLuint vertex = compile(GL_VERTEX_SHADER, vertexShader);
	const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentShader);
	glAttachShader(host.program, vertex);
	glAttachShader(host.program, fragment);
	glLinkProgram(host.program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	constexpr std::array<float, 8> corners{-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
	glGenVertexArrays(1, &host.vertices);
	glBindVertexArray(host.vertices);
	glGenBuffers(1, &host.buffer);
	glBindBuffer(GL_ARRAY_BUFFER, host.buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	constexpr std::array<unsigned char, 4> white{255, 255, 255, 255};
	glGenTextures(1, &host.blank);
	glBindTexture(GL_TEXTURE_2D, host.blank);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
}

/// @brief Draws the scene: the quad, turned by the time, with the UI's texture or a gradient.
/// @param host The host.
/// @param seconds The scene's time.
/// @param width The framebuffer's width.
/// @param height Its height.
/// @param picture The UI's texture, or 0.
void drawScene(const Host& host, double seconds, int width, int height, GLuint picture) {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, width, height);
	glClearColor(0.09f, 0.1f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(host.program);
	glUniform1f(glGetUniformLocation(host.program, "angle"), static_cast<float>(seconds * 0.2 * std::numbers::pi));
	glUniform2f(glGetUniformLocation(host.program, "halfSize"), picture != 0 ? 0.9f : 0.5f,
	            picture != 0 ? 0.45f : 0.5f);
	glUniform1f(glGetUniformLocation(host.program, "ratio"), static_cast<float>(height) / static_cast<float>(width));
	glUniform1i(glGetUniformLocation(host.program, "textured"), picture != 0 ? 1 : 0);
	glUniform1i(glGetUniformLocation(host.program, "picture"), 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, picture != 0 ? picture : host.blank);
	glBindVertexArray(host.vertices);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/// @brief Writes the default framebuffer as a binary PPM, top row first.
/// @param path Where.
/// @param width Its width.
/// @param height Its height.
void writeScreenshot(const char* path, int width, int height) {
	const auto stride = static_cast<std::size_t>(width) * 3;
	std::vector<unsigned char> pixels(stride * static_cast<std::size_t>(height));
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	std::FILE* file = std::fopen(path, "wb");
	if(file == nullptr) {
		std::fprintf(stderr, "embed: can't write %s\n", path);
		std::exit(1);
	}
	std::fprintf(file, "P6\n%d %d\n255\n", width, height);
	for(int row = height - 1; row >= 0; --row)
		std::fwrite(&pixels[static_cast<std::size_t>(row) * stride], 1, stride, file);
	std::fclose(file);
	std::printf("embed: wrote %s\n", path);
}

/// @brief The "slower" and "faster" actions: change the speed, and show it in the state.
/// @param user The host.
/// @param action Which.
void changeSpeed(void* user, const char* action, const char* /*sender*/) {
	Host& host = *static_cast<Host*>(user);
	host.speed =
	    std::strcmp(action, "faster") == 0 ? std::min(host.speed * 2.0, 8.0) : std::max(host.speed / 2.0, 0.125);
	const std::string value = std::to_string(host.speed);
	check(afr_screen_set_state(host.screen, "speed", value.c_str(), value.size()), "afr_screen_set_state");
}

/// @brief Any event (a revision applied): read state.paused before the next frame.
/// @param user The host.
void stateEvent(void* user, const char* /*event*/) { static_cast<Host*>(user)->stateChanged = true; }

/// @brief Reads state.paused after the screen reported a change.
/// @param host The host.
void readPaused(Host& host) {
	if(!host.stateChanged) return;
	host.stateChanged = false;
	char* json = nullptr;
	check(afr_screen_get_state(host.screen, "paused", &json), "afr_screen_get_state");
	host.paused = std::strcmp(json, "true") == 0;
	afr_free(json);
}

/// @brief The host's screen of a GLFW window.
/// @param window It.
/// @return Its screen (in overlay mode; input isn't mapped onto the turning quad).
afr_screen* screenOf(GLFWwindow* window) {
	const Host& host = *static_cast<Host*>(glfwGetWindowUserPointer(window));
	return host.texture ? nullptr : host.screen;
}

/// @brief Forwards GLFW's input to the screen, in window units (GLFW's screen coordinates).
/// @param window The host's window.
void forwardInput(GLFWwindow* window) {
	glfwSetCursorPosCallback(window, [](GLFWwindow* from, double x, double y) {
		if(afr_screen* screen = screenOf(from)) afr_screen_mouse(screen, AFR_MOUSE_MOVE, x, y, 0, 0);
	});
	glfwSetMouseButtonCallback(window, [](GLFWwindow* from, int button, int action, int mods) {
		afr_screen* screen = screenOf(from);
		if(screen == nullptr) return;
		double x = 0, y = 0;
		glfwGetCursorPos(from, &x, &y);
		afr_screen_mouse(screen, action == GLFW_PRESS ? AFR_MOUSE_PRESS : AFR_MOUSE_RELEASE, x, y, button, mods);
	});
	glfwSetScrollCallback(window, [](GLFWwindow* from, double dx, double dy) {
		afr_screen* screen = screenOf(from);
		if(screen == nullptr) return;
		double x = 0, y = 0;
		glfwGetCursorPos(from, &x, &y);
		afr_screen_scroll(screen, x, y, dx, dy);
	});
	glfwSetKeyCallback(window, [](GLFWwindow* from, int key, int /*scancode*/, int action, int mods) {
		if(afr_screen* screen = screenOf(from)) afr_screen_key(screen, key, static_cast<afr_key_action>(action), mods);
	});
	glfwSetCharCallback(window, [](GLFWwindow* from, unsigned int codepoint) {
		afr_screen* screen = screenOf(from);
		if(screen == nullptr || codepoint > 0x10FFFF) return;
		std::array<char, 4> utf8{};
		std::size_t length = 0;
		if(codepoint < 0x80) utf8[length++] = static_cast<char>(codepoint);
		else if(codepoint < 0x800) {
			utf8[length++] = static_cast<char>(0xC0 | (codepoint >> 6));
			utf8[length++] = static_cast<char>(0x80 | (codepoint & 0x3F));
		} else {
			if(codepoint < 0x10000) utf8[length++] = static_cast<char>(0xE0 | (codepoint >> 12));
			else {
				utf8[length++] = static_cast<char>(0xF0 | (codepoint >> 18));
				utf8[length++] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
			}
			utf8[length++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			utf8[length++] = static_cast<char>(0x80 | (codepoint & 0x3F));
		}
		afr_screen_text(screen, utf8.data(), length);
	});
}

/// @brief Whether AFFRESCOUI_HIDDEN asks for hidden windows, read as affresco::hiddenWindowsRequested() reads it.
/// @return true unless it is unset, empty or "0".
bool hiddenRequested() {
	const char* value = std::getenv("AFFRESCOUI_HIDDEN");
	return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

/// @brief Opens the host's window with an OpenGL 4.1 core context, and loads its functions.
/// @param hidden Whether to keep it hidden (a screenshot run).
/// @return The window.
GLFWwindow* openWindow(bool hidden) {
	glfwInitHint(GLFW_COCOA_MENUBAR, hidden ? GLFW_FALSE : GLFW_TRUE); // hidden: no regular app, so not in the Dock
	if(glfwInit() == GLFW_FALSE) {
		std::fprintf(stderr, "embed: GLFW could not start\n");
		std::exit(1);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_VISIBLE, hidden ? GLFW_FALSE : GLFW_TRUE);
	GLFWwindow* window = glfwCreateWindow(800, 500, "AffrescoUI embedded", nullptr, nullptr);
	if(window == nullptr) {
		std::fprintf(stderr, "embed: no OpenGL 4.1 window\n");
		std::exit(1);
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	if(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) == 0) {
		std::fprintf(stderr, "embed: OpenGL functions could not be loaded\n");
		std::exit(1);
	}
	return window;
}

/// @brief Loads the screen embedded in the current context, and answers its actions.
/// @param host The host.
void openScreen(Host& host) {
	afr_options options;
	check(afr_options_init(&options), "afr_options_init");
	options.mode = AFR_MODE_EMBEDDED;
	options.history_in_memory = 1;
	options.gl_loader = reinterpret_cast<afr_gl_loader>(glfwGetProcAddress);
	static constexpr std::array<const char*, 3> natives{"slower", "faster", nullptr}; // answered below
	options.native_actions = natives.data();
	check(afr_screen_create_from_file(AFFRESCOUI_EMBED_DIR "/embed.screen.json", &options, &host.screen),
	      "afr_screen_create_from_file");
	check(afr_screen_on_action(host.screen, "slower", changeSpeed, &host), "afr_screen_on_action");
	check(afr_screen_on_action(host.screen, "faster", changeSpeed, &host), "afr_screen_on_action");
	check(afr_screen_on_event(host.screen, stateEvent, &host), "afr_screen_on_event");
}

/// @brief Draws one frame: the scene, and the UI over it or on it.
/// @param host The host.
/// @param seconds The scene's time.
void drawFrame(Host& host, double seconds) {
	int width = 0, height = 0, windowWidth = 0;
	glfwGetFramebufferSize(host.window, &width, &height);
	glfwGetWindowSize(host.window, &windowWidth, nullptr);
	if(width <= 0 || height <= 0 || windowWidth <= 0) return;
	const float scale = static_cast<float>(width) / static_cast<float>(windowWidth);
	check(afr_screen_step(host.screen, 0), "afr_screen_step");
	readPaused(host);
	if(host.texture) {
		uint32_t picture = 0;
		check(afr_screen_render_texture(host.screen, 640, 320, 2.0f, &picture), "afr_screen_render_texture");
		drawScene(host, seconds, width, height, picture);
		return;
	}
	drawScene(host, seconds, width, height, 0);
	check(afr_screen_frame(host.screen, width, height, scale, 0), "afr_screen_frame");
}

} // namespace

/// @brief Runs the demo.
/// @param count The number of arguments.
/// @param arguments "--texture" for the UI on the quad.
/// @return The exit status.
int main(int count, char** arguments) { // NOLINT(misc-const-correctness): main's own signature
	try {
		Host host;
		host.texture = count > 1 && std::strcmp(arguments[1], "--texture") == 0;
		const char* screenshot = std::getenv("AFFRESCOUI_SCREENSHOT");
		host.window = openWindow(screenshot != nullptr || hiddenRequested());
		glfwSetWindowUserPointer(host.window, &host);
		makeScene(host);
		openScreen(host);
		forwardInput(host.window);
		double seconds = 0.0, last = glfwGetTime();
		while(glfwWindowShouldClose(host.window) == GLFW_FALSE) {
			const double now = glfwGetTime();
			if(!host.paused && screenshot == nullptr) seconds += (now - last) * host.speed;
			last = now;
			drawFrame(host, screenshot != nullptr ? 0.6 : seconds);
			if(screenshot != nullptr) {
				int width = 0, height = 0;
				glfwGetFramebufferSize(host.window, &width, &height);
				writeScreenshot(screenshot, width, height);
				break;
			}
			glfwSwapBuffers(host.window);
			glfwPollEvents();
		}
		afr_screen_destroy(host.screen);
		glDeleteTextures(1, &host.blank);
		glDeleteBuffers(1, &host.buffer);
		glDeleteVertexArrays(1, &host.vertices);
		glDeleteProgram(host.program);
		glfwDestroyWindow(host.window);
		glfwTerminate();
		return 0;
	} catch(const std::exception& error) { // out of memory: the calls that fail on their own exit with a message
		std::fprintf(stderr, "embed: %s\n", error.what());
		return 1;
	}
}

// NOLINTEND(modernize-use-std-print)
