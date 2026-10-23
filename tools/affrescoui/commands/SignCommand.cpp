/// @file
/// @brief `affrescoui sign`: signs a feature's manifest or a catalogue's index (RFC 0004), and makes signing keys.

#include "Cli.hpp"
#include "JsonText.hpp"

#include "FeatureSignatures.hpp"

#include <affresco/affresco.hpp>

#include <array>
#include <cstdio>
#include <iostream>

namespace affresco::cli {

namespace {

using format::SigningKey;

/// The options sign takes that have a value.
constexpr std::array<std::string_view, 3> signOptions = {"--key", "--new-key", "-o"};

/// The largest key file read: a key and a line break, with room to spare.
constexpr std::size_t maxKeyFile = 256;

/// @brief Makes a key file only its owner can read, refusing to replace one.
/// @param file The file.
/// @return Success; the public key is printed.
/// @throws FileProblem If it exists or can't be written.
Exit newKey(const std::filesystem::path& file) {
	const SigningKey key = SigningKey::generate();
	// "x": never over an existing file; the permissions are set before the secret is written
	std::FILE* out = std::fopen(file.string().c_str(), "wx");
	if(!out) throw FileProblem(file.string() + ": exists already or can't be written");
	std::error_code error;
	std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
	                             error);
	std::string text = key.toText();
	const bool written = !error && std::fwrite(text.data(), 1, text.size(), out) == text.size();
	format::wipe(text); // as readKey() does: the secret stays in memory no longer than it is needed
	if(std::fclose(out) != 0 || !written) throw FileProblem(file.string() + ": can't be written");
	std::cout << key.publisher() << '\n';
	return Exit::Success;
}

/// @brief Reads a key file.
/// @param file The file.
/// @return The key.
/// @throws FileProblem If it isn't one.
SigningKey readKey(const std::filesystem::path& file) {
	std::string text = readFile(file);
	if(text.size() > maxKeyFile) throw FileProblem(file.string() + ": isn't a key file");
	try {
		SigningKey key = SigningKey::fromText(text);
		format::wipe(text);
		return key;
	} catch(const DocumentError&) {
		format::wipe(text);
		throw FileProblem(file.string() + ": isn't a key file (expected \"ed25519-secret:...\")");
	}
}

/// @brief Sets each release's digest and size of a catalogue's index from its manifest, so the index signs what is
/// there; the manifests are signed first.
/// @param index The index.
/// @param folder The index's folder: the releases' URLs are relative to it.
/// @throws FileProblem If a manifest can't be read.
void refreshReleases(json& index, const std::filesystem::path& folder) {
	const auto features = index.find("features");
	if(features == index.end() || !features->is_object()) return;
	for(auto& [name, feature] : features->items()) {
		const auto releases = feature.find("releases");
		if(releases == feature.end() || !releases->is_array()) continue;
		for(json& release : *releases) {
			if(!release.is_object() || !release.contains("url") || !release["url"].is_string()) continue;
			const std::string bytes = readFile(folder / release["url"].get<std::string>());
			release["digest"] = format::digestOf(bytes);
			release["size"] = bytes.size();
		}
	}
}

} // namespace

Exit signFile(std::span<const std::string_view> args) {
	const Arguments arguments(args, signOptions, {});
	if(const auto file = arguments.value("--new-key")) {
		if(!arguments.positional().empty() || arguments.value("--key"))
			throw UsageError("--new-key takes no other argument");
		return newKey(*file);
	}
	const std::filesystem::path file = arguments.file();
	const auto keyFile = arguments.value("--key");
	if(!keyFile) throw UsageError("sign needs --key KEYFILE (affrescoui sign --new-key KEYFILE makes one)");
	json document;
	try {
		document = json::parse(readFile(file));
	} catch(const json::parse_error& error) {
		throw FileProblem(file.string() + ": isn't JSON (" + error.what() + ")");
	}
	if(!document.is_object()) throw FileProblem(file.string() + ": isn't a manifest or a catalogue's index");
	if(document.value("format", json()) == "affrescoui-catalog") refreshReleases(document, file.parent_path());
	const SigningKey key = readKey(*keyFile);
	const json signedDocument = key.sign(std::move(document));
	writeFile(arguments.value("-o").value_or(file.string()), printText(parseText(signedDocument.dump())));
	std::cerr << file.string() << ": signed by " << key.publisher() << '\n';
	return Exit::Success;
}

} // namespace affresco::cli
