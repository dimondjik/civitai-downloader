#include <helper_functions.hpp>

void helpers::splitByDelimeter(std::string source, std::vector<std::string>& out, char delimeter) {
	size_t tokenPos = 0;
	std::string token;
	std::vector<std::string> tokens;

	while ((tokenPos = source.find(',')) != std::string::npos) {
		token = source.substr(0, tokenPos);
		tokens.push_back(token);
		source.erase(0, tokenPos + 1);
	};

	if (!tokens.size()) {
		tokens.push_back(source);
	};	
	
	out = tokens;
};

void helpers::lstrip(std::string& source) {
	source.erase(source.begin(), std::find_if(source.begin(), source.end(),
		[](unsigned char c) {
			return !std::isspace(c);
		}));
};

void helpers::rstrip(std::string& source) {
	source.erase(std::find_if(source.rbegin(), source.rend(),
		[](unsigned char c) {
			return !std::isspace(c);
		}
	).base(), source.end());
};

void helpers::strip(std::string& source) {
	helpers::rstrip(source);
	helpers::lstrip(source);
};

void helpers::removeEntriesWithCharacters(std::vector<std::string>& source, std::string characters) {
	std::vector<std::string> sourceBuf = source;

	for (int i = 0; i < sourceBuf.size(); i++) {
		for (char c : "\\/:*?\"<>|") {
			if (sourceBuf[i].find(c) != std::string::npos) {
				source.erase(std::remove(source.begin(), source.end(), sourceBuf[i]), source.end());
			};
		};
	};
};

void helpers::replaceCharactersInEntries(std::vector<std::string>& source, std::string characters) {
	std::vector<std::string> sourceBuf = source;

	for (int i = 0; i < source.size(); i++) {
		for (char c : "\\/:*?\"<>|") {
			std::replace(source[i].begin(), source[i].end(), c, '#');
		};
	};
};

void helpers::generateCRC32(std::string path, std::string &checksum) {

	using namespace CryptoPP;

	try
	{
		CRC32 crc32;
		
		HashFilter* filter = new HashFilter(crc32, new HexEncoder(new StringSink(checksum)));

		FileSource source(path.c_str(), true, filter);
	}
	catch (const Exception& ex)
	{
		fprintf(stderr, "%s\n", ex.what());
	};
};
