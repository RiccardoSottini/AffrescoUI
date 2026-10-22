/// @file
/// @brief systemSecretStore on macOS: the Keychain's generic passwords (Security framework).

#include "affresco/net/SignIn.hpp"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <memory>

namespace affresco::net {

namespace {

/// @brief Releases a Core Foundation object.
struct Release {
	/// @brief Releases it.
	/// @param object The object, or null.
	void operator()(const void* object) const noexcept {
		if(object) CFRelease(object);
	}
};

/// @brief A Core Foundation object, released when it goes.
template<typename Type> using Owned = std::unique_ptr<std::remove_pointer_t<Type>, Release>;

/// @brief A CFString of UTF-8 text.
/// @param text The text.
/// @return It.
Owned<CFStringRef> text(const std::string& text) {
	return Owned<CFStringRef>(CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(text.data()),
	                                                  static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
}

/// @brief The Keychain's generic passwords, under one service.
class KeychainSecretStore final : public SecretStore {
public:
	/// @brief Keeps the service.
	/// @param service The service.
	explicit KeychainSecretStore(std::string service) : service_(std::move(service)) {}

	[[nodiscard]] std::optional<std::string> read(const std::string& key) override {
		auto query = base(key);
		CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
		CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);
		CFTypeRef found = nullptr;
		if(SecItemCopyMatching(query.get(), &found) != errSecSuccess || !found) return std::nullopt;
		const Owned<CFDataRef> data(static_cast<CFDataRef>(found));
		return std::string(reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())),
		                   static_cast<std::size_t>(CFDataGetLength(data.get())));
	}

	bool write(const std::string& key, const std::string& secret) override {
		const Owned<CFDataRef> data(CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(secret.data()),
		                                         static_cast<CFIndex>(secret.size())));
		auto query = base(key);
		auto change = Owned<CFMutableDictionaryRef>(CFDictionaryCreateMutable(
		    nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
		CFDictionarySetValue(change.get(), kSecValueData, data.get());
		if(SecItemUpdate(query.get(), change.get()) == errSecSuccess) return true;
		CFDictionarySetValue(query.get(), kSecValueData, data.get());
		CFDictionarySetValue(query.get(), kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
		return SecItemAdd(query.get(), nullptr) == errSecSuccess;
	}

	void erase(const std::string& key) override { SecItemDelete(base(key).get()); }

	[[nodiscard]] std::string name() const override { return "Keychain"; }

private:
	/// @brief The query of one item.
	/// @param key Its account.
	/// @return The query.
	[[nodiscard]] Owned<CFMutableDictionaryRef> base(const std::string& key) const {
		Owned<CFMutableDictionaryRef> query(
		    CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
		const auto service = text(service_);
		const auto account = text(key);
		CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);
		CFDictionarySetValue(query.get(), kSecAttrService, service.get());
		CFDictionarySetValue(query.get(), kSecAttrAccount, account.get());
		return query;
	}

	std::string service_; ///< See the constructor.
};

} // namespace

std::shared_ptr<SecretStore> systemSecretStore(const std::string& service) {
	return std::make_shared<KeychainSecretStore>(service);
}

} // namespace affresco::net
