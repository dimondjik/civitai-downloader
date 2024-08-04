#pragma once
#include <string>
#include <vector>

#include <cryptopp/cryptlib.h>
#include <cryptopp/filters.h>
#include <cryptopp/files.h>
#include <cryptopp/crc.h>
#include <cryptopp/hex.h>

#include <string>
#include <iostream>


namespace helpers {
	void splitByDelimeter(std::string source, std::vector<std::string>& out, char delimeter);

	void rstrip(std::string& source);
	void lstrip(std::string& source);
	void strip(std::string& source);

	void removeEntriesWithCharacters(std::vector<std::string>& source, std::string characters);

	void generateCRC32(std::string path, std::string& checksum);
};
