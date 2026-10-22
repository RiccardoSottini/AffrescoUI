/**
 * @file
 * @brief The C API's test, compiled as C11: every function of affresco/affresco.h, headless, in a hidden window and
 * embedded in a context of its own, plus the argument checks. Usage: affrescoui_capi_test <scratch folder>.
 */

#include <affresco/affresco.h>

#include <GLFW/glfw3.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief How many checks failed. */
static int failures = 0;

/** @brief Checks a condition, reporting where it failed. */
#define CHECK(condition)                                                                                               \
	do {                                                                                                               \
		if(!(condition)) {                                                                                             \
			fprintf(stderr, "%s:%d: failed: %s (%s)\n", __FILE__, __LINE__, #condition, afr_last_error());             \
			++failures;                                                                                                \
		}                                                                                                              \
	} while(0)

/** @brief Checks that a call succeeds. */
#define CHECK_OK(call) CHECK((call) == AFR_OK)

/** @brief The test's document: a button that runs a native action, one that emits and calls, one that fails. */
static const char document[] =
    "{ \"format\": \"affrescoui-screen\", \"version\": 2,\n"
    "  \"window\": { \"title\": \"C API\", \"width\": 200, \"height\": 120 },\n"
    "  \"state\": { \"count\": 1, \"result\": 0, \"failed\": false, \"items\": [{ \"title\": \"a/b\" }] },\n"
    "  \"root\": { \"id\": \"root\", \"children\": [\n"
    "    { \"id\": \"go\", \"anchors\": { \"left\": 0, \"top\": 0 }, \"color\": \"#204080\",\n"
    "      \"width\": 80, \"height\": 30, \"onClick\": \"hostAction\" },\n"
    "    { \"id\": \"calc\", \"anchors\": { \"left\": 0, \"top\": 30 }, \"width\": 80, \"height\": 30, \"onClick\": [\n"
    "      { \"emit\": \"clicked\", \"data\": { \"n\": 1 } },\n"
    "      { \"call\": \"double\", \"with\": { \"n\": 21 }, \"into\": \"state.result\" } ] },\n"
    "    { \"id\": \"broken\", \"anchors\": { \"left\": 0, \"top\": 60 },\n"
    "      \"width\": 80, \"height\": 30, \"onClick\": [\n"
    "      { \"call\": \"broken\", \"onError\": [{ \"set\": \"state.failed\", \"to\": true }] } ] }\n"
    "  ] } }\n";

/** @brief What the callbacks saw. */
typedef struct Seen {
	int actions;      /**< Native actions run. */
	char sender[64];  /**< The last one's sender. */
	int events;       /**< Events heard. */
	int calls;        /**< Native calls answered. */
	int tasks;        /**< Tasks run. */
	afr_screen* quit; /**< A screen the task quits, or NULL. */
} Seen;

/** @brief A native action: counts it. */
static void onAction(void* user, const char* action, const char* sender) {
	Seen* seen = (Seen*)user;
	CHECK(strcmp(action, "hostAction") == 0);
	snprintf(seen->sender, sizeof seen->sender, "%s", sender);
	++seen->actions;
}

/** @brief An event: counts the ones named "clicked". */
static void onEvent(void* user, const char* event) {
	if(strstr(event, "clicked") != NULL) ++((Seen*)user)->events;
}

/** @brief The native function "double": returns twice its n. */
static void onDouble(void* user, const char* arguments, afr_call* call) {
	const char* n = strstr(arguments, "\"n\":");
	const int value = n != NULL ? atoi(n + 4) : 0;
	char answer[32];
	const int length = snprintf(answer, sizeof answer, "%d", value * 2);
	CHECK_OK(afr_call_return(call, answer, (size_t)length));
	CHECK(afr_call_return(call, "{nope", 5) == AFR_ERROR_ARGUMENT); /* not JSON: refused, the first answer stays */
	++((Seen*)user)->calls;
}

/** @brief The native function "broken": fails. */
static void onBroken(void* user, const char* arguments, afr_call* call) {
	(void)arguments;
	CHECK_OK(afr_call_fail(call, "no disk"));
	++((Seen*)user)->calls;
}

/** @brief A posted task: counts itself, and quits a window screen if asked. */
static void task(void* user) {
	Seen* seen = (Seen*)user;
	++seen->tasks;
	if(seen->quit != NULL) CHECK_OK(afr_screen_quit(seen->quit));
}

/** @brief Clicks at a point. */
static void click(afr_screen* screen, double x, double y) {
	CHECK_OK(afr_screen_mouse(screen, AFR_MOUSE_ENTER, x, y, 0, 0));
	CHECK_OK(afr_screen_mouse(screen, AFR_MOUSE_MOVE, x, y, 0, 0));
	CHECK_OK(afr_screen_mouse(screen, AFR_MOUSE_PRESS, x, y, 0, 0));
	CHECK_OK(afr_screen_mouse(screen, AFR_MOUSE_RELEASE, x, y, 0, 0));
}

/** @brief Checks that the state at a path is some JSON text. */
static void expectState(afr_screen* screen, const char* path, const char* expected) {
	char* json = NULL;
	CHECK_OK(afr_screen_get_state(screen, path, &json));
	if(json != NULL && strcmp(json, expected) != 0) {
		fprintf(stderr, "state %s is %s, not %s\n", path, json, expected);
		++failures;
	}
	afr_free(json);
}

/** @brief The calls that need no screen, and the argument checks that need none either. */
static void testGlobals(void) {
	CHECK(afr_version() == AFR_VERSION);
	CHECK((afr_version() >> 16) == AFR_VERSION_MAJOR);
	CHECK(afr_options_init(NULL) == AFR_ERROR_ARGUMENT);
	CHECK(strlen(afr_last_error()) > 0);
	afr_options options;
	CHECK_OK(afr_options_init(&options));
	CHECK(strlen(afr_last_error()) == 0);
	CHECK(options.size == sizeof options && options.mode == AFR_MODE_WINDOW && options.scale == 1.0f);
	afr_free(NULL);
	afr_screen_destroy(NULL);

	afr_screen* screen = NULL;
	CHECK(afr_screen_create_from_string(NULL, 4, NULL, NULL, &screen) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_create_from_string(document, sizeof document - 1, NULL, NULL, NULL) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_create_from_string("{ broken", 8, NULL, NULL, &screen) != AFR_OK && screen == NULL);
	CHECK(afr_screen_create_from_file(NULL, NULL, &screen) == AFR_ERROR_ARGUMENT);
	options.size = 4; /* smaller than any version of the struct */
	CHECK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen) == AFR_ERROR_ARGUMENT);
	CHECK_OK(afr_options_init(&options));
	options.mode = AFR_MODE_HEADLESS;
	options.scale = NAN;
	CHECK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen) == AFR_ERROR_ARGUMENT);
	options.scale = 1.0f;
	options.mode = AFR_MODE_HEADLESS;
	static const char* const unnamed[] = {"", NULL}; /* an action needs a name */
	options.native_actions = unnamed;
	CHECK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen) == AFR_ERROR_ARGUMENT);
	options.native_actions = NULL;
	options.mode = (afr_mode)9; /* not an afr_mode */
	CHECK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen) == AFR_ERROR_ARGUMENT);
	options.mode = AFR_MODE_EMBEDDED; /* no gl_loader */
	CHECK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_step(NULL, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_call_return(NULL, "1", 1) == AFR_ERROR_ARGUMENT);
	CHECK(afr_call_fail(NULL, "x") == AFR_ERROR_ARGUMENT);

	/* Before, a picture had no size limit: 16384 window units at a scale of 16 asked for terabytes */
	options.mode = AFR_MODE_HEADLESS;
	options.width = 16384;
	options.height = 16;
	options.scale = 2.0f;
	CHECK_OK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen));
	if(screen == NULL) return;
	int w = 0, h = 0;
	uint8_t* rgba = NULL;
	CHECK(afr_screen_render_rgba(screen, &w, &h, &rgba) == AFR_ERROR_ARGUMENT && rgba == NULL);
	afr_screen_destroy(screen);
}

/** @brief The state, revisions and document calls. */
static void testState(afr_screen* screen) {
	char* json = NULL;
	expectState(screen, "count", "1");
	expectState(screen, "/items/0/title", "\"a/b\"");
	expectState(screen, "items.0.title", "\"a/b\"");
	CHECK(afr_screen_get_state(screen, "missing", &json) == AFR_ERROR_NOT_FOUND && json == NULL);
	CHECK(afr_screen_get_state(screen, "count", NULL) == AFR_ERROR_ARGUMENT);
	CHECK_OK(afr_screen_set_state(screen, "count", "5", 1));
	expectState(screen, "count", "5");
	CHECK(afr_screen_set_state(screen, "count", "{nope", 5) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_set_state(screen, "count", NULL, 3) == AFR_ERROR_ARGUMENT);

	const char revision[] = "{ \"author\": \"application\", \"message\": \"more\", \"set\": { \"/state/count\": 7 } }";
	CHECK_OK(afr_screen_submit_revision(screen, revision, sizeof revision - 1, &json));
	CHECK(json != NULL && strstr(json, "\"applied\"") != NULL);
	afr_free(json);
	json = NULL;
	expectState(screen, "count", "7");
	const char bad[] = "{ \"author\": \"application\", \"patch\": [{ \"op\": \"remove\", \"path\": \"/nowhere\" }] }";
	CHECK(afr_screen_submit_revision(screen, bad, sizeof bad - 1, &json) == AFR_ERROR_REFUSED);
	CHECK(json != NULL && strstr(json, "\"rejected\"") != NULL);
	afr_free(json);
	CHECK_OK(afr_screen_submit_revision(screen, revision, sizeof revision - 1, NULL));

	CHECK_OK(afr_screen_replace_document(screen, document, sizeof document - 1));
	expectState(screen, "count", "1");
	CHECK(afr_screen_replace_document(screen, "[1]", 3) != AFR_OK);
}

/** @brief Registers the callbacks. */
static void registerCallbacks(afr_screen* screen, Seen* seen) {
	CHECK_OK(afr_screen_on_action(screen, "hostAction", onAction, seen));
	CHECK(afr_screen_on_action(screen, "hostAction", NULL, seen) == AFR_ERROR_ARGUMENT);
	CHECK_OK(afr_screen_on_event(screen, onEvent, seen));
	CHECK_OK(afr_screen_on_call(screen, "double", onDouble, seen));
	CHECK_OK(afr_screen_on_call(screen, "broken", onBroken, seen));
	CHECK(afr_screen_on_call(screen, NULL, onBroken, seen) == AFR_ERROR_ARGUMENT);
}

/** @brief Input: the clicks reach the callbacks, keys, text and scrolling are taken, bad input is refused. */
static void testInput(afr_screen* screen, Seen* seen) {
	click(screen, 10, 10);
	CHECK(seen->actions == 1 && strcmp(seen->sender, "go") == 0);
	click(screen, 10, 40);
	CHECK(seen->events == 1 && seen->calls == 1);
	expectState(screen, "result", "42");
	click(screen, 10, 70);
	CHECK(seen->calls == 2);
	expectState(screen, "failed", "true");

	CHECK_OK(afr_screen_key(screen, 65, AFR_KEY_PRESS, AFR_MOD_SHIFT));
	CHECK_OK(afr_screen_key(screen, 65, AFR_KEY_RELEASE, AFR_MOD_SHIFT));
	CHECK_OK(afr_screen_text(screen, "h\xC3\xA9llo", 6));
	CHECK_OK(afr_screen_text(screen, "", 0));
	CHECK_OK(afr_screen_scroll(screen, 10, 10, 0, -1));
	CHECK(afr_screen_key(screen, 9999, AFR_KEY_PRESS, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_key(screen, 65, (afr_key_action)7, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_text(screen, "\xC3", 1) == AFR_ERROR_ARGUMENT);         /* cut short */
	CHECK(afr_screen_text(screen, "\xED\xA0\x80", 3) == AFR_ERROR_ARGUMENT); /* a surrogate */
	CHECK(afr_screen_text(screen, NULL, 2) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_mouse(screen, AFR_MOUSE_MOVE, NAN, 0, 0, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_mouse(screen, AFR_MOUSE_PRESS, 0, 0, 8, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_mouse(screen, (afr_mouse_action)42, 0, 0, 0, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_scroll(screen, 0, 0, INFINITY, 0) == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_post(screen, NULL, NULL) == AFR_ERROR_ARGUMENT);
}

/** @brief Draws a picture and checks its size, and that the first button shows at the top left. */
static void testPicture(afr_screen* screen, int width, int height) {
	int w = 0, h = 0;
	uint8_t* rgba = NULL;
	CHECK_OK(afr_screen_render_rgba(screen, &w, &h, &rgba));
	CHECK(w == width && h == height && rgba != NULL);
	if(rgba != NULL && w > 0 && h > 0) {
		const uint8_t* corner = rgba + ((size_t)2 * (size_t)w + 2) * 4; /* top left, inside the first button */
		CHECK(corner[0] == 0x20 && corner[1] == 0x40 && corner[2] == 0x80 && corner[3] == 255);
	}
	afr_free(rgba);
	CHECK(afr_screen_render_rgba(screen, NULL, &h, &rgba) == AFR_ERROR_ARGUMENT);
}

/** @brief A headless screen from a file: everything but the window and the host's context. */
static void testHeadless(const char* folder) {
	char path[1024];
	snprintf(path, sizeof path, "%s/test.screen.json", folder);
	FILE* file = fopen(path, "wb");
	CHECK(file != NULL);
	if(file == NULL) return;
	fwrite(document, 1, sizeof document - 1, file);
	fclose(file);

	afr_options options;
	CHECK_OK(afr_options_init(&options));
	options.mode = AFR_MODE_HEADLESS;
	options.history_in_memory = 1;
	options.scale = 2.0f;
	options.socket_directory = folder;
	static const char* const natives[] = {"hostAction", NULL}; /* answered by registerCallbacks() */
	options.native_actions = natives;
	afr_screen* screen = NULL;
	CHECK_OK(afr_screen_create_from_file(path, &options, &screen));
	if(screen == NULL) return;
	Seen seen = {0};
	registerCallbacks(screen, &seen);
	testState(screen);
	CHECK_OK(afr_screen_post(screen, task, &seen));
	CHECK(seen.tasks == 0);
	CHECK_OK(afr_screen_step(screen, 16));
	CHECK(seen.tasks == 1);
	CHECK(afr_screen_step(screen, -1) == AFR_ERROR_ARGUMENT);
	testInput(screen, &seen);
	CHECK_OK(afr_screen_step(screen, 500));
	testPicture(screen, 400, 240);

	CHECK(afr_screen_run(screen) == AFR_ERROR_STATE);
	CHECK(afr_screen_quit(screen) == AFR_ERROR_STATE);
	uint32_t texture = 0;
	CHECK(afr_screen_frame(screen, 10, 10, 1, 0) == AFR_ERROR_STATE);
	CHECK(afr_screen_render_texture(screen, 10, 10, 1, &texture) == AFR_ERROR_STATE);
	CHECK_OK(afr_screen_listen(screen, "capi-test.sock", "agent"));
	CHECK(afr_screen_listen(screen, "../escape.sock", "agent") == AFR_ERROR_ARGUMENT);
	CHECK(afr_screen_listen(screen, "other.sock", "") == AFR_ERROR_ARGUMENT);
	afr_screen_destroy(screen);
}

/** @brief A hidden window: a posted task quits the run. */
static void testWindow(void) {
	afr_options options;
	CHECK_OK(afr_options_init(&options));
	options.hidden = 1;
	afr_screen* screen = NULL;
	CHECK_OK(afr_screen_create_from_string(document, sizeof document - 1, NULL, &options, &screen));
	if(screen == NULL) return;
	Seen seen = {0};
	seen.quit = screen;
	CHECK(afr_screen_step(screen, 0) == AFR_ERROR_STATE);
	CHECK_OK(afr_screen_post(screen, task, &seen));
	CHECK_OK(afr_screen_run(screen));
	CHECK(seen.tasks == 1);
	afr_screen_destroy(screen);
}

/** @brief Looks GL functions up for the embedded screen. */
static void* loadGl(const char* name) { return (void*)glfwGetProcAddress(name); }

/** @brief An embedded screen in a hidden GLFW window's context of the test's own. */
static void testEmbedded(void) {
	if(!glfwInit()) {
		fprintf(stderr, "no GLFW: the embedded mode is not tested\n");
		++failures;
		return;
	}
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	GLFWwindow* host = glfwCreateWindow(200, 120, "host", NULL, NULL);
	CHECK(host != NULL);
	if(host != NULL) {
		glfwMakeContextCurrent(host);
		afr_options options;
		CHECK_OK(afr_options_init(&options));
		options.mode = AFR_MODE_EMBEDDED;
		options.gl_loader = loadGl;
		afr_screen* screen = NULL;
		CHECK_OK(afr_screen_create_from_string(document, sizeof document - 1, ".", &options, &screen));
		if(screen != NULL) {
			Seen seen = {0};
			registerCallbacks(screen, &seen);
			CHECK_OK(afr_screen_post(screen, task, &seen));
			CHECK_OK(afr_screen_step(screen, 0));
			CHECK(seen.tasks == 1);
			CHECK_OK(afr_screen_frame(screen, 200, 120, 1.0f, 0));
			testInput(screen, &seen);
			uint32_t texture = 0;
			CHECK_OK(afr_screen_render_texture(screen, 100, 60, 0.5f, &texture));
			CHECK(texture != 0);
			CHECK(afr_screen_render_texture(screen, 0, 60, 1.0f, &texture) == AFR_ERROR_ARGUMENT);
			CHECK(afr_screen_frame(screen, 200, 120, 100.0f, 0) == AFR_ERROR_ARGUMENT);
			CHECK_OK(afr_screen_frame(screen, 400, 240, 2.0f, 0));
			testPicture(screen, 400, 240);
			CHECK(afr_screen_run(screen) == AFR_ERROR_STATE);
			afr_screen_destroy(screen);
		}
		glfwDestroyWindow(host);
	}
	glfwTerminate();
}

/** @brief Runs the tests. @return 0 if they all pass. */
int main(int argc, char** argv) {
	if(argc != 2) {
		fprintf(stderr, "usage: affrescoui_capi_test <scratch folder>\n");
		return 2;
	}
	testGlobals();
	testHeadless(argv[1]);
	testWindow();
	testEmbedded();
	if(failures > 0) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("capi: all checks passed\n");
	return 0;
}
