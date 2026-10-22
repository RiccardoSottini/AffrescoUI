/// @file
/// @brief What every function of the C API shares (errors, checks, memory, the headless task queue), and the functions
/// that take no screen.

#include "Handle.hpp"

#include <cstdlib>
#include <cstring>
#include <new>

namespace affresco::capi {

namespace {

/// @brief Why the last call on this thread failed: afr_last_error(). One per thread, as errno is.
thread_local std::string lastError; // NOLINT: per-thread by design, like errno

} // namespace

afr_status fail(afr_status status, std::string_view message) noexcept {
	try {
		lastError.assign(message);
	} catch(...) {
		lastError.clear(); // no memory for the message: the status still says what happened
	}
	return status;
}

void clearError() noexcept { lastError.clear(); }

afr_status reportException() noexcept {
	try {
		throw;
	} catch(const Failure& failure) {
		return fail(failure.status, failure.message);
	} catch(const std::bad_alloc&) {
		return fail(AFR_ERROR_INTERNAL, "out of memory");
	} catch(const DocumentError& error) {
		return fail(AFR_ERROR_DOCUMENT, error.what());
	} catch(const json::exception& error) {
		return fail(AFR_ERROR_ARGUMENT, error.what());
	} catch(const std::exception& error) {
		return fail(AFR_ERROR_INTERNAL, error.what());
	} catch(...) {
		return fail(AFR_ERROR_INTERNAL, "unknown error");
	}
}

std::string_view checkedString(const char* text, std::string_view what, std::size_t limit) {
	if(text == nullptr) throw Failure{AFR_ERROR_ARGUMENT, std::string(what) + " is NULL"};
	const void* end = std::memchr(text, '\0', limit + 1);
	if(end == nullptr) throw Failure{AFR_ERROR_ARGUMENT, std::string(what) + " is too long"};
	return {text, static_cast<std::size_t>(static_cast<const char*>(end) - text)};
}

std::string_view checkedText(const char* text, std::size_t length, std::string_view what) {
	if(text == nullptr && length > 0) throw Failure{AFR_ERROR_ARGUMENT, std::string(what) + " is NULL"};
	if(length > maximumText) throw Failure{AFR_ERROR_ARGUMENT, std::string(what) + " is too long"};
	return length == 0 ? std::string_view{} : std::string_view{text, length};
}

void copyOut(std::string_view text, char** out) {
	auto* copy = static_cast<char*>(std::malloc(text.size() + 1));
	if(copy == nullptr) throw std::bad_alloc();
	std::memcpy(copy, text.data(), text.size());
	copy[text.size()] = '\0';
	*out = copy;
}

void require(const void* pointer, std::string_view what) {
	if(pointer == nullptr) throw Failure{AFR_ERROR_ARGUMENT, std::string(what) + " is NULL"};
}

void TaskQueue::post(std::function<void()> task) {
	const std::lock_guard lock(mutex_);
	tasks_.push_back(std::move(task));
}

void TaskQueue::run() {
	{
		const std::lock_guard lock(mutex_);
		running_.swap(tasks_);
	}
	runTasks(running_); // emptied even if one throws: a failed step doesn't run them again
}

} // namespace affresco::capi

affresco::Executor& afr_screen::executor() noexcept {
	if(embedding) return *embedding;
	if(mode == AFR_MODE_HEADLESS) return tasks;
	return *application;
}

extern "C" {

uint32_t afr_version(void) { return AFR_VERSION; }

const char* afr_last_error(void) { return affresco::capi::lastError.c_str(); }

void afr_free(void* memory) { std::free(memory); }

afr_status afr_options_init(afr_options* options) {
	if(options == nullptr) return affresco::capi::fail(AFR_ERROR_ARGUMENT, "options is NULL");
	*options = afr_options{};
	options->size = sizeof(afr_options);
	options->mode = AFR_MODE_WINDOW;
	options->scale = 1.0f;
	affresco::capi::clearError();
	return AFR_OK;
}

} // extern "C"
