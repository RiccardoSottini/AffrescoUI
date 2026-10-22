/// @file
/// @brief systemSecretStore on Windows: the Credential Manager's generic credentials, kept for the signed-in user.

#include "affresco/net/SignIn.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>

namespace affresco::net {

namespace {

/// @brief UTF-8 text as UTF-16.
/// @param text The text.
/// @return It.
std::wstring wide(const std::string& text) {
	const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
	return result;
}

/// @brief The Credential Manager, under one service.
class CredentialSecretStore final : public SecretStore {
public:
	/// @brief Keeps the service.
	/// @param service The service.
	explicit CredentialSecretStore(std::string service) : service_(std::move(service)) {}

	[[nodiscard]] std::optional<std::string> read(const std::string& key) override {
		PCREDENTIALW credential = nullptr;
		if(!CredReadW(target(key).c_str(), CRED_TYPE_GENERIC, 0, &credential)) return std::nullopt;
		std::string secret(reinterpret_cast<const char*>(credential->CredentialBlob), credential->CredentialBlobSize);
		CredFree(credential);
		return secret;
	}

	bool write(const std::string& key, const std::string& secret) override {
		std::wstring name = target(key);
		std::string blob = secret;
		CREDENTIALW credential{};
		credential.Type = CRED_TYPE_GENERIC;
		credential.TargetName = name.data();
		credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
		credential.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
		credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
		const bool written = CredWriteW(&credential, 0) != FALSE;
		SecureZeroMemory(blob.data(), blob.size());
		return written;
	}

	void erase(const std::string& key) override { CredDeleteW(target(key).c_str(), CRED_TYPE_GENERIC, 0); }

	[[nodiscard]] std::string name() const override { return "Credential Manager"; }

private:
	/// @brief A credential's target name.
	/// @param key The key.
	/// @return "<service>/<key>".
	[[nodiscard]] std::wstring target(const std::string& key) const { return wide(service_ + "/" + key); }

	std::string service_; ///< See the constructor.
};

} // namespace

std::shared_ptr<SecretStore> systemSecretStore(const std::string& service) {
	return std::make_shared<CredentialSecretStore>(service);
}

} // namespace affresco::net
