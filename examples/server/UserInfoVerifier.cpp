/// @file
/// @brief UserInfoVerifier and ChainVerifier.

#include "UserInfoVerifier.hpp"

#include "TokenFile.hpp"

#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/core/Sha256.hpp>
#include <affresco/net/Url.hpp>

#include <algorithm>
#include <fstream>
#include <future>
#include <stdexcept>

namespace affresco::server {

class UserInfoVerifier::Inline final : public Executor {
public:
	/// @brief Runs a task at once, on the calling thread (the client's worker).
	/// @param task The task.
	void post(std::function<void()> task) override { task(); }
};

namespace {

/// @brief Tells whether a subject may be in the people file: printable ASCII without spaces, at most 255 bytes.
/// @param subject The subject.
/// @return true if it may.
bool validSubject(std::string_view subject) noexcept {
	return !subject.empty() && subject.size() <= 255 &&
	       std::ranges::all_of(subject, [](char c) { return c > ' ' && c < 127; });
}

} // namespace

UserInfoVerifier::UserInfoVerifier(std::string_view userinfo, const std::filesystem::path& people,
                                   net::ClientOptions options)
    : userinfo_(userinfo), executor_(std::make_unique<Inline>()),
      client_(std::make_unique<net::Client>(*executor_, std::move(options))) {
	const auto url = net::parseUrl(userinfo);
	if(!url || (url->scheme != "https" && url->scheme != "http"))
		throw std::runtime_error("--userinfo is the provider's userinfo endpoint, an https:// URL");
	if(const auto refused = client_->allow(url->origin())) throw std::runtime_error("--userinfo: " + *refused);
	std::ifstream file(people);
	if(!file) throw std::runtime_error("can't read the people file " + people.string());
	std::string line;
	for(int number = 1; std::getline(file, line); ++number) {
		if(line.ends_with('\r')) line.pop_back();
		if(line.empty() || line.starts_with('#')) continue;
		const std::size_t space = line.find(' ');
		const std::string subject = line.substr(0, space);
		const std::string name = space == std::string::npos ? std::string() : line.substr(space + 1);
		if(!validSubject(subject) || !TokenFile::validName(name) || subjects_.size() >= maxPeople)
			throw std::runtime_error("the people file's line " + std::to_string(number) +
			                         " isn't \"<subject> <name>\" (or there are too many lines)");
		subjects_[subject] = name;
		if(std::ranges::find(people_, name) == people_.end()) people_.push_back(name);
	}
}

UserInfoVerifier::~UserInfoVerifier() = default;

std::optional<std::string> UserInfoVerifier::verify(std::string_view token) {
	if(token.empty() || token.size() > 4096) return std::nullopt;
	const std::array<std::uint8_t, 32> hash = sha256::digest(token);
	const std::scoped_lock lock(mutex_);
	const Clock::time_point now = Clock::now();
	std::erase_if(remembered_, [now](const auto& entry) { return entry.second.second <= now; });
	if(const auto found = remembered_.find(hash); found != remembered_.end()) return found->second.first;
	const auto subject = ask(token);
	if(!subject) return std::nullopt;
	const auto person = subjects_.find(*subject);
	if(person == subjects_.end()) return std::nullopt;
	if(remembered_.size() < maxRemembered) remembered_[hash] = {person->second, now + remembered};
	return person->second;
}

std::optional<std::string> UserInfoVerifier::ask(std::string_view token) {
	auto answer = std::make_shared<std::promise<std::optional<std::string>>>();
	std::future<std::optional<std::string>> result = answer->get_future();
	network::Request request;
	request.url = userinfo_;
	request.headers = {{"Authorization", "Bearer " + std::string(token)}, {"Accept", "application/json"}};
	request.timeout = timeout;
	request.maxBodyBytes = maxAnswerBytes;
	network::Handlers handlers;
	handlers.done = [answer](network::Response response) {
		const json body = json::parse(response.body, nullptr, false);
		const auto subject = body.is_object() ? body.find("sub") : body.end();
		answer->set_value(body.is_object() && subject != body.end() && subject->is_string()
		                      ? std::optional<std::string>(subject->get<std::string>())
							  : std::nullopt);
	};
	handlers.failed = [answer](const network::Failure&) { answer->set_value(std::nullopt); };
	const std::unique_ptr<network::Transfer> transfer = client_->start(std::move(request), std::move(handlers));
	if(result.wait_for(timeout + std::chrono::milliseconds{500}) != std::future_status::ready) return std::nullopt;
	return result.get(); // the transfer, ended, is destroyed after
}

std::vector<std::string> UserInfoVerifier::people() const { return people_; }

std::optional<std::string> ChainVerifier::verify(std::string_view token) {
	for(const auto& verifier : verifiers_)
		if(auto name = verifier->verify(token)) return name;
	return std::nullopt;
}

} // namespace affresco::server
