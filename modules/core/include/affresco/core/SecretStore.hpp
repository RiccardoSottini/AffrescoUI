#pragma once

/// @file
/// @brief The SecretStore interface: where an application keeps what must stay secret (refresh tokens), out of its
/// documents, state, history and logs. The net module gives the system's (Keychain, Credential Manager, libsecret);
/// MemorySecretStore is for tests.

#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace affresco {

/// @brief Keeps secrets by key, e.g. "affrescoui/forge/documentServer".
///
/// Every member may be called from any thread. A secret never leaves the store but through read().
class SecretStore {
public:
	SecretStore() = default;
	SecretStore(const SecretStore&) = delete;
	SecretStore& operator=(const SecretStore&) = delete;
	SecretStore(SecretStore&&) = delete;
	SecretStore& operator=(SecretStore&&) = delete;
	virtual ~SecretStore() = default;

	/// @brief Reads a secret.
	/// @param key Its key.
	/// @return It, or nullopt if there is none (or the store can't be read).
	[[nodiscard]] virtual std::optional<std::string> read(const std::string& key) = 0;
	/// @brief Keeps a secret, replacing the one under the same key.
	/// @param key Its key.
	/// @param secret The secret.
	/// @return Whether it was kept.
	virtual bool write(const std::string& key, const std::string& secret) = 0;
	/// @brief Forgets a secret.
	/// @param key Its key.
	virtual void erase(const std::string& key) = 0;
	/// @brief The store's name for people, e.g. "Keychain" or "memory".
	[[nodiscard]] virtual std::string name() const = 0;
};

/// @brief A SecretStore in memory: nothing outlives it. For tests, and where the system has no store.
class MemorySecretStore final : public SecretStore {
public:
	/// @brief Reads a secret.
	/// @param key Its key.
	/// @return It, or nullopt.
	[[nodiscard]] std::optional<std::string> read(const std::string& key) override {
		const std::scoped_lock lock(mutex_);
		const auto found = secrets_.find(key);
		return found == secrets_.end() ? std::nullopt : std::optional<std::string>(found->second);
	}
	/// @brief Keeps a secret.
	/// @param key Its key.
	/// @param secret The secret.
	/// @return true.
	bool write(const std::string& key, const std::string& secret) override {
		const std::scoped_lock lock(mutex_);
		secrets_[key] = secret;
		return true;
	}
	/// @brief Forgets a secret.
	/// @param key Its key.
	void erase(const std::string& key) override {
		const std::scoped_lock lock(mutex_);
		secrets_.erase(key);
	}
	/// @brief "memory".
	[[nodiscard]] std::string name() const override { return "memory"; }

private:
	std::mutex mutex_;                           ///< Guards secrets_.
	std::map<std::string, std::string> secrets_; ///< By key.
};

} // namespace affresco
