#pragma once
#include <curl\curl.h>
#include <string>
#include <iostream>
#include <nlohmann\json.hpp>
#include <vector>
#include <regex>
#include <config4cpp\Configuration.h>
#include <helper_functions.hpp>

#include <Windows.h>
#include <fstream>

#define BUFFER_SIZE_BYTES 8192

enum modelTypeEnum
{
	LORA = 0,
	SD = 1
};

class DownloadStruct {
public:
	modelTypeEnum modelType;
	std::string path;
	std::vector<std::string> triggerWords;
	std::fstream* fs;
	char writeBuffer[BUFFER_SIZE_BYTES];

	DownloadStruct();

	~DownloadStruct();

	void flushBuffer();
};

class ProgressStruct {
public:
	CURL* curl;
	curl_off_t lastDownloaded;
	curl_off_t lastTime;
	curl_off_t lastSpeed;

	ProgressStruct();

	~ProgressStruct();
};

class CivitaiDownloader {
public:
	CivitaiDownloader();

	~CivitaiDownloader();

	int downloadFromLink(std::string link);

private:
	std::vector<int> idx;

	std::string link;

	std::string loraFolder;
	std::string sdFolder;
	std::string token;

	std::map<std::string, modelTypeEnum> modelTypeTranslation;

	std::string downloadURL;
	std::string imageURL;

	std::string remoteChecksum;

	DownloadStruct ds;

	ProgressStruct ps;

	int parseConfig();

	int getIdFromURL();

	int getModelInfo();

	int downloadModel();

	int downloadImage();

	void reset();

	static size_t curlToString(void* ptr, size_t size, size_t nmemb, std::string* out);

	static size_t curlToDisk(void* ptr, size_t size, size_t nmemb, DownloadStruct* ds);

	static size_t curlToDiskSimple(void* ptr, size_t size, size_t nmemb, std::fstream* fs);

	static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, DownloadStruct* ds);

	static int curlProgressCallback(ProgressStruct* ps, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded);
};