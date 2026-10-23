/// @file
/// @brief systemSecretStore on Linux and the BSDs: the Secret Service (GNOME Keyring, KWallet) through libsecret,
/// loaded at run time when it is installed (it is neither linked nor shipped); else a MemorySecretStore.

#include "affresco/net/SignIn.hpp"

#include "affresco/core/Log.hpp"

#include <dlfcn.h>

#include <array>

namespace affresco::net {

namespace {

/// @brief libsecret's SecretSchemaAttribute (secret-schema.h).
struct SchemaAttribute {
	const char* name; ///< The attribute's name.
	int type;         ///< SECRET_SCHEMA_ATTRIBUTE_STRING is 0.
};

/// @brief libsecret's SecretSchema (secret-schema.h): its layout, which is part of libsecret's stable ABI.
struct Schema {
	const char* name;                         ///< The schema's name.
	int flags;                                ///< SECRET_SCHEMA_NONE is 0.
	std::array<SchemaAttribute, 32> attributes; ///< Ended by a null name.
	int reserved;                             ///< Reserved.
	std::array<void*, 7> reservedPointers;    ///< Reserved.
};

using Store = int (*)(const Schema*, const char*, const char*, const char*, void*, void**, ...);   ///< secret_password_store_sync
using Lookup = char* (*)(const Schema*, void*, void**, ...);                                      ///< secret_password_lookup_sync
using Clear = int (*)(const Schema*, void*, void**, ...);                                         ///< secret_password_clear_sync
using Free = void (*)(char*);                                                                     ///< secret_password_free

/// @brief The Secret Service, through a libsecret loaded at run time.
class SecretServiceStore final : public SecretStore {
public:
	/// @brief Takes the loaded library.
	/// @param library dlopen's handle.
	/// @param service The service.
	SecretServiceStore(void* library, std::string service)
	    : library_(library), service_(std::move(service)),
	      store_(reinterpret_cast<Store>(dlsym(library, "secret_password_store_sync"))),
	      lookup_(reinterpret_cast<Lookup>(dlsym(library, "secret_password_lookup_sync"))),
	      clear_(reinterpret_cast<Clear>(dlsym(library, "secret_password_clear_sync"))),
	      free_(reinterpret_cast<Free>(dlsym(library, "secret_password_free"))) {
		schema_.name = "org.affrescoui.Secret";
		schema_.attributes[0] = {"service", 0};
		schema_.attributes[1] = {"key", 0};
	}
	/// @brief Unloads the library.
	~SecretServiceStore() override { dlclose(library_); }
	SecretServiceStore(const SecretServiceStore&) = delete;
	SecretServiceStore& operator=(const SecretServiceStore&) = delete;
	SecretServiceStore(SecretServiceStore&&) = delete;
	SecretServiceStore& operator=(SecretServiceStore&&) = delete;

	/// @brief Whether every function was found.
	[[nodiscard]] bool complete() const noexcept { return store_ && lookup_ && clear_ && free_; }

	/// @brief Reads a secret (see SecretStore::read).
	[[nodiscard]] std::optional<std::string> read(const std::string& key) override {
		char* found = lookup_(&schema_, nullptr, nullptr, "service", service_.c_str(), "key", key.c_str(), nullptr);
		if(!found) return std::nullopt;
		std::string secret = found;
		free_(found); // secret_password_free clears it too
		return secret;
	}

	/// @brief Keeps a secret, replacing the one under the same key (see SecretStore::write).
	bool write(const std::string& key, const std::string& secret) override {
		const std::string label = service_ + " sign-in";
		return store_(&schema_, nullptr, label.c_str(), secret.c_str(), nullptr, nullptr, "service", service_.c_str(),
		              "key", key.c_str(), nullptr) != 0;
	}

	/// @brief Forgets a secret (see SecretStore::erase).
	void erase(const std::string& key) override {
		clear_(&schema_, nullptr, nullptr, "service", service_.c_str(), "key", key.c_str(), nullptr);
	}

	/// @brief The store's name for people (see SecretStore::name).
	[[nodiscard]] std::string name() const override { return "Secret Service"; }

private:
	void* library_;       ///< dlopen's handle.
	std::string service_; ///< See the constructor.
	Schema schema_{};     ///< Its attributes: "service" and "key".
	Store store_;         ///< secret_password_store_sync.
	Lookup lookup_;       ///< secret_password_lookup_sync.
	Clear clear_;         ///< secret_password_clear_sync.
	Free free_;           ///< secret_password_free.
};

} // namespace

std::shared_ptr<SecretStore> systemSecretStore(const std::string& service) {
	if(void* library = dlopen("libsecret-1.so.0", RTLD_NOW | RTLD_LOCAL)) {
		auto store = std::make_shared<SecretServiceStore>(library, service);
		if(store->complete()) return store;
	}
	log(LogLevel::Warning, "AffrescoUI sign-in: libsecret isn't installed; sign-ins are kept in memory only");
	return std::make_shared<MemorySecretStore>();
}

} // namespace affresco::net
