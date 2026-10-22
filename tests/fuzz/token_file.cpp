/// @file
/// @brief Fuzzes the document server's token file (examples/server/TokenFile.hpp): the file of `<sha256 hex> <name>`
/// lines the server reads at start, which whoever can write that folder controls, and the tokens peers present.
///
/// The input is written to a file and read. A file that reads lists valid names only, at most maxEntries of them, and
/// verify() gives a listed name or nothing for any token, the input itself included.

#include "Fuzz.hpp"

#include "TokenFile.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

/// @brief The file the input is written to: one per process, so parallel runs (-fork, -jobs) don't share it.
/// @return Its path.
const std::filesystem::path& tokenFile() {
	static const std::filesystem::path file =
	    std::filesystem::temp_directory_path() / ("affrescoui-fuzz-tokens-" + std::to_string(::getpid()));
	return file;
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const auto input = affresco::fuzz::text(data, size);
	{
		std::ofstream file(tokenFile(), std::ios::binary | std::ios::trunc);
		file.write(input.data(), static_cast<std::streamsize>(input.size()));
		if(!file) affresco::fuzz::broken("the token file can be written");
	}
	try {
		affresco::server::TokenFile tokens(tokenFile());
		const auto people = tokens.people();
		if(people.size() > affresco::server::TokenFile::maxEntries) affresco::fuzz::broken("at most maxEntries lines");
		for(const std::string& name : people)
			if(!affresco::server::TokenFile::validName(name)) affresco::fuzz::broken("every name is valid");
		for(const std::string_view token : {input, std::string_view("not a token")})
			if(const auto name = tokens.verify(token); name && std::ranges::find(people, *name) == people.end())
				affresco::fuzz::broken("a verified token names someone in the file");
	} catch(const std::runtime_error&) {
		return 0; // malformed, as it should be refused
	}
	return 0;
}
