/// @file
/// @brief SignIn's device flow (RFC 8628): codes for the person to type elsewhere, then polls until they did.

#include "OAuthSignIn.hpp"

namespace affresco::net {

void SignIn::startDevice(Entry& entry) {
	for(const std::string* url : {&entry.provider.deviceUrl, &entry.provider.tokenUrl})
		if(const auto refused = network_.check(*url)) {
			fail(entry, *refused);
			return;
		}
	std::string scope;
	for(const auto& one : entry.provider.scopes) scope += (scope.empty() ? "" : " ") + one;
	std::vector<std::pair<std::string, std::string>> fields = {{"client_id", entry.provider.clientId}};
	if(!scope.empty()) fields.emplace_back("scope", scope);
	post(entry, entry.provider.deviceUrl, fields,
	     [this, &entry](std::optional<std::string> body, const std::string&, const std::string& error) {
		     if(!body) {
			     fail(entry, error);
			     return;
		     }
		     auto read = oauth::readDeviceCodes(*body);
		     if(const auto* why = std::get_if<std::string>(&read)) {
			     fail(entry, *why);
			     return;
		     }
		     auto& codes = std::get<oauth::DeviceCodes>(read);
		     entry.deviceCode = std::move(codes.deviceCode);
		     entry.userCode = std::move(codes.userCode);
		     entry.verificationUrl = std::move(codes.verificationUrl);
		     entry.interval = codes.interval;
		     entry.deadline = std::chrono::steady_clock::now() + codes.lifetime;
		     entry.transfer.reset();
		     entry.timer = timer_->after(entry.interval, [this, &entry] {
			     entry.timer = 0;
			     pollDevice(entry);
		     });
		     changed(); // shows the code
	     });
}

void SignIn::pollDevice(Entry& entry) {
	if(std::chrono::steady_clock::now() >= entry.deadline) {
		fail(entry, "the code expired before the sign-in was finished");
		return;
	}
	post(entry, entry.provider.tokenUrl,
	     {{"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
	      {"device_code", entry.deviceCode},
	      {"client_id", entry.provider.clientId}},
	     [this, &entry](std::optional<std::string> body, const std::string& failure, const std::string& error) {
		     if(body) {
			     entry.deviceCode.clear();
			     accept(entry, *body, {});
			     return;
		     }
		     const std::string code = oauth::errorCode(failure);
		     if(code != "authorization_pending" && code != "slow_down") {
			     fail(entry, error);
			     return;
		     }
		     if(code == "slow_down") entry.interval += std::chrono::seconds{5};
		     entry.transfer.reset();
		     entry.timer = timer_->after(entry.interval, [this, &entry] {
			     entry.timer = 0;
			     pollDevice(entry);
		     });
	     });
}

} // namespace affresco::net
