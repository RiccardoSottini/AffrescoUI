/// @file
/// @brief readNumber and numberLengthFits (affresco/core/Numbers.hpp): numbers from outside the app are read only up
/// to maxNumberLength characters, as JSON's are.

#include <affresco/core/Numbers.hpp>

#include <doctest/doctest.h>

#include <string>

TEST_CASE("numbers: readNumber reads what strtod reads, up to maxNumberLength characters") {
	char* end = nullptr;
	const std::string plain = " 12.5px";
	CHECK(affresco::readNumber(plain.c_str(), &end) == 12.5);
	CHECK(std::string(end) == "px");

	const std::string exact = "1" + std::string(affresco::maxNumberLength - 1, '0');
	CHECK(affresco::readNumber(exact.c_str(), &end) == 1e39);
	CHECK(*end == '\0');

	for(const std::string& longer : {"1" + std::string(500, '0'), "0." + std::string(500, '1'), "0x" + std::string(60, 'f')}) {
		CHECK(affresco::readNumber(longer.c_str(), &end) == 0.0);
		CHECK(end == longer.c_str()); // nothing read: the caller reports a malformed number
	}
	const std::string none = "abc";
	CHECK(affresco::readNumber(none.c_str(), &end) == 0.0);
	CHECK(end == none.c_str());

	CHECK(affresco::numberLengthFits(exact));
	CHECK_FALSE(affresco::numberLengthFits(exact + "0"));
}
