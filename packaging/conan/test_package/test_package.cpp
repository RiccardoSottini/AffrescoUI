/// @file
/// @brief Conan's test of the AffrescoUI package: parses a document with the library's JSON and prints it.

#include <affresco/core/Json.hpp>

#include <iostream>

/// @brief Parses a small document and checks it.
/// @return 0 if the document reads back, 1 otherwise.
int main() {
	const affresco::json document = affresco::json::parse(R"({"title": "Groceries"})");
	if(document.at("title") != "Groceries") return 1;
	std::cout << "test_package: AffrescoUI found, " << document.dump() << '\n';
	return 0;
}
