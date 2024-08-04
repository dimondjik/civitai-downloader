#include <civitai_downloader.hpp>

DownloadStruct::DownloadStruct() {
	this->modelType = LORA;
	this->path = "";
	this->triggerWords = {};
	this->fs = nullptr;

	this->flushBuffer();
};

DownloadStruct::~DownloadStruct() {

};

void DownloadStruct::flushBuffer() {
	for (char item : this->writeBuffer) {
		item = 0;
	};
};

ProgressStruct::ProgressStruct() {
	this->curl = nullptr;
	this->lastDownloaded = 0;
	this->lastTime = 0;
	this->lastSpeed = 0;
};

ProgressStruct::~ProgressStruct() {

};

CivitaiDownloader::CivitaiDownloader() {
	this->modelTypeTranslation["LORA"] = LORA;
	this->modelTypeTranslation["Checkpoint"] = SD;

	this->reset();
};

CivitaiDownloader::~CivitaiDownloader() {

};

int CivitaiDownloader::downloadFromLink(std::string link) {
	this->link = link;

	std::vector<int (*)(CivitaiDownloader*)> pipeline;

	pipeline.push_back([](CivitaiDownloader* c) -> int { return c->parseConfig(); });
	pipeline.push_back([](CivitaiDownloader* c) -> int { return c->getIdFromURL(); });
	pipeline.push_back([](CivitaiDownloader* c) -> int { return c->getModelInfo(); });
	pipeline.push_back([](CivitaiDownloader* c) -> int { return c->downloadModel(); });
	pipeline.push_back([](CivitaiDownloader* c) -> int { return c->downloadImage(); });

	for (int i = 0; i < pipeline.size(); i++) {
		if (pipeline[i](this)) {
			return 1;
		};
	};

	this->reset();

	return 0;
};

int CivitaiDownloader::parseConfig() {
	config4cpp::Configuration* cfg = config4cpp::Configuration::create();

	try {
		cfg->parse(".\\downloader.cfg");

		this->loraFolder = cfg->lookupString("", "lora-folder");
		this->sdFolder = cfg->lookupString("", "sd-folder");
		this->token = cfg->lookupString("", "token");

		cfg->destroy();
		return 0;
	}
	catch (const config4cpp::ConfigurationException& ex) {
		fprintf(stderr, "Error reading configuration!\n%s\n", ex.c_str());
		cfg->destroy();
		return 1;
	};
};

int CivitaiDownloader::getIdFromURL() {
	std::smatch sm;
	std::regex_search(this->link, sm, std::regex("https:\\/\\/civitai\\.com\\/models\\/(\\d+).*\\?modelVersionId=(\\d+)"));

	if (sm.size() < 3) {
		std::regex_search(this->link, sm, std::regex("https:\\/\\/civitai\\.com\\/models\\/(\\d+)"));

		if (sm.size() < 2) {
			fprintf(stderr, "Can't get Model ID from the link!\n");
			return 1;
		}
		else {
			fprintf(stdout, "Found Model ID: %s\n", sm[1].str().c_str());
			this->idx[0] = stoi(sm[1]);
			return 0;
		};
	}
	else {
		fprintf(stdout, "Found Model ID: %s\nFound Version ID: %s\n", sm[1].str().c_str(), sm[2].str().c_str());

		this->idx[0] = stoi(sm[1]);
		this->idx[1] = stoi(sm[2]);
		return 0;
	};
};

int CivitaiDownloader::getModelInfo() {
	CURL* curl;
	CURLcode res;
	curl_slist* headers = NULL;
	char errbuf[CURL_ERROR_SIZE];
	std::string data;

	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, std::string("Authorization: Bearer ").append(this->token).c_str());

	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, std::string("https://civitai.com/api/v1/models/").append(std::to_string(this->idx[0])).c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CivitaiDownloader::curlToString);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	errbuf[0] = 0;
	res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);

	if (CURLE_OK == res) {
		nlohmann::json dataJSON = nlohmann::json::parse(data);

		// fprintf(stdout, "%s", dataJSON.dump().c_str());
		// return 1;

		fprintf(stdout, "Model name: %s\n", dataJSON["name"].get<std::string>().c_str());

		this->ds.modelType = this->modelTypeTranslation[dataJSON["type"].get<std::string>()];

		fprintf(stdout, "Model type: %s\n", dataJSON["type"].get<std::string>().c_str());

		// Model version index
		int vi = 0;

		// Getting model version
		if (this->idx[1] == 0) {
			fprintf(stdout, "No version got from link, will use the first version found\n");
			for (vi; vi < dataJSON["modelVersions"].size(); vi++) {
				if (dataJSON["modelVersions"][vi]["index"].get<int>() == 0) {
					this->idx[1] = dataJSON["modelVersions"][vi]["id"].get<int>();
					break;
				};
			};
		}
		else {
			for (vi; vi < dataJSON["modelVersions"].size(); vi++) {
				if (dataJSON["modelVersions"][vi]["id"].get<int>() == this->idx[1]) {
					break;
				};
			};
		};
		fprintf(stdout, "Model version: %s\n", dataJSON["modelVersions"][vi]["name"].get<std::string>().c_str());

		// Getting trigger words if that's a LoRA (cleaning up if needed)
		if (this->ds.modelType == LORA) {
			if (dataJSON["modelVersions"][vi]["trainedWords"].size() > 0) {
				this->ds.triggerWords = dataJSON["modelVersions"][vi]["trainedWords"].get<std::vector<std::string>>();

				fprintf(stdout, "Cleaning up trigger words\n");

				if (this->ds.triggerWords.size() == 1) {
					fprintf(stdout, "Single trigger word found, trying to split by commas...\n");

					helpers::splitByDelimeter(this->ds.triggerWords[0], this->ds.triggerWords, ',');
				};

				fprintf(stdout, "Removing excess whitespaces...\n");

				for (int j = 0; j < this->ds.triggerWords.size(); j++) {
					helpers::strip(this->ds.triggerWords[j]);
				};

				fprintf(stdout, "Removing trigger words with disallowed symbols...\n");

				helpers::removeEntriesWithCharacters(this->ds.triggerWords, "\\/:*?\"<>|");

				fprintf(stdout, "Trigger words: ");
				for (int i = 0; i < this->ds.triggerWords.size(); i++) {
					fprintf(stdout, "%s%s", this->ds.triggerWords[i].c_str(), ((i != this->ds.triggerWords.size() - 1) ? ", " : ""));
				};
				fprintf(stdout, "\n");
			}
			else {
				fprintf(stdout, "No trigger words found\n");
			};
		}
		else if (this->ds.modelType == SD) {
			fprintf(stdout, "Not using trigger words, since it's a Checkpoint\n");
		}
		else {
			fprintf(stderr, "Unsupported model type!\n");
			return 1;
		};

		// File index
		int fi = 0;

		for (fi; fi < dataJSON["modelVersions"][vi]["files"].size(); fi++)
		{
			if (dataJSON["modelVersions"][vi]["downloadUrl"].get<std::string>() == dataJSON["modelVersions"][vi]["files"][fi]["downloadUrl"]) {
				break;
			};
		};

		this->downloadURL = dataJSON["modelVersions"][vi]["files"][fi]["downloadUrl"].get<std::string>();

		fprintf(stdout, "URL: %s\n", downloadURL.c_str());

		this->remoteChecksum = dataJSON["modelVersions"][vi]["files"][fi]["hashes"]["CRC32"].get<std::string>();

		// fprintf(stdout, "Remote CRC32: %s\n", remoteChecksum.c_str());

		// Does every model have images?
		this->imageURL = dataJSON["modelVersions"][vi]["images"][0]["url"].get<std::string>();

		fprintf(stdout, "First found image URL: %s\n", this->imageURL.c_str());

		return 0;
	}
	else {
		fprintf(stderr, "CURL error getting model info!\n");

		size_t errbufLen = strlen(errbuf);

		if (errbufLen) {
			fprintf(stderr, "%s%s", errbuf,
				((errbuf[errbufLen - 1] != '\n') ? "\n" : ""));
		}
		else {
			fprintf(stderr, "%s\n", curl_easy_strerror(res));
		};

		return 1;
	};
};

int CivitaiDownloader::downloadModel() {
	switch (this->ds.modelType) {
	case LORA:
		this->ds.path = loraFolder;
		break;
	case SD:
		this->ds.path = sdFolder;
		break;
	default:
		fprintf(stderr, "This error should not have happened :(\n");
		return 1;
	};

	CURL* curl;
	CURLcode res;
	curl_slist* headers = nullptr;
	char errbuf[CURL_ERROR_SIZE];
	std::fstream fs;

	fprintf(stdout, "Downloading model to: %s\n", this->ds.path.c_str());

	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, std::string("Authorization: Bearer ").append(this->token).c_str());

	curl = curl_easy_init();

	this->ds.fs = &fs;

	this->ps.lastDownloaded = 0;
	this->ps.lastTime = 0;
	this->ps.lastSpeed = 0;
	this->ps.curl = curl;

	curl_easy_setopt(curl, CURLOPT_URL, this->downloadURL.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CivitaiDownloader::curlToDisk);

	// Follow redirects
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &this->ds);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &this->ds);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CivitaiDownloader::curlHeaderCallback);

	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CivitaiDownloader::curlProgressCallback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &this->ps);

	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	HANDLE out = GetStdHandle(STD_ERROR_HANDLE);
	CONSOLE_CURSOR_INFO cursor_info;
	GetConsoleCursorInfo(out, &cursor_info);
	cursor_info.bVisible = false;
	SetConsoleCursorInfo(out, &cursor_info);

	errbuf[0] = 0;
	res = curl_easy_perform(curl);

	GetConsoleCursorInfo(out, &cursor_info);
	cursor_info.bVisible = true;
	SetConsoleCursorInfo(out, &cursor_info);

	curl_easy_cleanup(curl);

	if (fs.is_open()) {
		fs.close();
	};
	this->ds.fs = nullptr;
	this->ds.flushBuffer();

	// Next line after progress bar
	fprintf(stdout, "\n");

	if (CURLE_OK == res) {
		std::string checksum;

		helpers::generateCRC32(this->ds.path, checksum);

		if (checksum != this->remoteChecksum) {
			fprintf(stderr, "Checksums not equal!\nGenerated CRC32: %s\nRemote CRC32: %s\n", checksum.c_str(), this->remoteChecksum.c_str());
			return 1;
		}
		else {
			fprintf(stdout, "Checksum check success\nGenerated CRC32: %s\nRemote CRC32: %s\n", checksum.c_str(), this->remoteChecksum.c_str());
			return 0;
		};
	}
	else {
		fprintf(stderr, "CURL error downloading model!\n");

		size_t errbufLen = strlen(errbuf);

		if (errbufLen) {
			fprintf(stderr, "%s%s", errbuf,
				((errbuf[errbufLen - 1] != '\n') ? "\n" : ""));
		}
		else {
			fprintf(stderr, "%s\n", curl_easy_strerror(res));
		};

		return 1;
	};
};

int CivitaiDownloader::downloadImage() {
	std::string path = this->ds.path.substr(0, this->ds.path.find_last_of(".") + 1) + "png";

	std::fstream fs;

	fs.open(path, std::ios::out | std::ios::binary);

	CURL* curl;
	CURLcode res;
	curl_slist* headers = nullptr;
	curl_off_t prevDownloaded = 0;
	char errbuf[CURL_ERROR_SIZE];
	fprintf(stdout, "Downloading image to: %s\n", path.c_str());

	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, std::string("Authorization: Bearer ").append(this->token).c_str());

	curl = curl_easy_init();

	this->ps.lastDownloaded = 0;
	this->ps.lastTime = 0;
	this->ps.lastSpeed = 0;
	this->ps.curl = curl;

	curl_easy_setopt(curl, CURLOPT_URL, this->imageURL.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CivitaiDownloader::curlToDiskSimple);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fs);

	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CivitaiDownloader::curlProgressCallback);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &this->ps);

	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	HANDLE out = GetStdHandle(STD_ERROR_HANDLE);
	CONSOLE_CURSOR_INFO cursor_info;
	GetConsoleCursorInfo(out, &cursor_info);
	cursor_info.bVisible = false;
	SetConsoleCursorInfo(out, &cursor_info);

	errbuf[0] = 0;
	res = curl_easy_perform(curl);

	GetConsoleCursorInfo(out, &cursor_info);
	cursor_info.bVisible = true;
	SetConsoleCursorInfo(out, &cursor_info);

	curl_easy_cleanup(curl);

	if (fs.is_open()) {
		fs.close();
	};

	// Next line after progress bar
	fprintf(stdout, "\n");

	if (CURLE_OK == res) {
		return 0;
	}
	else {
		fprintf(stderr, "CURL error downloading image!\n");

		size_t errbufLen = strlen(errbuf);

		if (errbufLen) {
			fprintf(stderr, "%s%s", errbuf,
				((errbuf[errbufLen - 1] != '\n') ? "\n" : ""));
		}
		else {
			fprintf(stderr, "%s\n", curl_easy_strerror(res));
		};

		return 1;
	};
};

void CivitaiDownloader::reset() {
	this->idx = std::vector<int>(2, 0);;

	this->link;

	this->loraFolder = "";
	this->sdFolder = "";
	this->token = "";

	this->downloadURL = "";
	this->imageURL = "";

	this->remoteChecksum = "";

	this->ds = DownloadStruct();

	this->ps = ProgressStruct();
};

size_t CivitaiDownloader::curlToString(void* ptr, size_t size, size_t nmemb, std::string* out) {
	char* ptrC = static_cast<char*>(ptr);

	for (int i = 0; i < size * nmemb; i++) {
		*out += ptrC[i];
	};

	return size * nmemb;
};

size_t CivitaiDownloader::curlToDisk(void* ptr, size_t size, size_t nmemb, DownloadStruct* ds) {
	if (!ds->fs->is_open()) {
		ds->fs->open(ds->path, std::ios::out | std::ios::binary);
	};

	char* ptrC = static_cast<char*>(ptr);

	size_t written = 0;

	while (nmemb >= BUFFER_SIZE_BYTES)
	{
		memcpy(ds->writeBuffer, ptrC, BUFFER_SIZE_BYTES);
		ds->fs->write(ds->writeBuffer, BUFFER_SIZE_BYTES);
		ptrC += BUFFER_SIZE_BYTES;

		nmemb -= BUFFER_SIZE_BYTES;
		written += BUFFER_SIZE_BYTES;
	};

	if (nmemb != 0)
	{
		memcpy(ds->writeBuffer, ptrC, nmemb);
		ds->fs->write(ds->writeBuffer, nmemb);

		written += nmemb;
	};

	return written;
};

size_t CivitaiDownloader::curlToDiskSimple(void* ptr, size_t size, size_t nmemb, std::fstream* fs) {
	char* ptrC = static_cast<char*>(ptr);

	fs->write(ptrC, nmemb);
	ptrC += BUFFER_SIZE_BYTES;

	return nmemb;
};

size_t CivitaiDownloader::curlHeaderCallback(char* buffer, size_t size, size_t nitems, DownloadStruct* ds) {
	std::string bufferStr = buffer;

	std::smatch sm;
	std::regex_search(bufferStr, sm, std::regex("Content-Disposition: attachment; filename=\"(.*)\""));

	if (sm.size() == 2) {
		std::string cdFname = sm[1];

		switch (ds->modelType)
		{
		case SD:
			ds->path += cdFname;
			break;
		case LORA:
			if (!ds->triggerWords.size()) {
				ds->path += cdFname;
			}
			else {
				for (int i = 0; i < ds->triggerWords.size(); i++) {
					ds->path += ds->triggerWords[i];
					if (i != ds->triggerWords.size() - 1) {
						ds->path += ", ";
					};
				};
				ds->path += cdFname.substr(cdFname.find_last_of("."));
			};
			break;
		};
	};

	return nitems * size;
};

int CivitaiDownloader::curlProgressCallback(ProgressStruct* ps, curl_off_t totalToDownload, curl_off_t nowDownloaded, curl_off_t totalToUpload, curl_off_t nowUploaded) {
	if (totalToDownload <= 0.0) {
		return 0;
	};

	int width = 40;

	double downloadedPercent = (double)nowDownloaded / (double)totalToDownload;

	int fill = static_cast<int>(round(downloadedPercent * width));

	int i = 0;

	fprintf(stdout, "%3.0f%% [", downloadedPercent * 100);

	for (i; i < fill; i++) {
		fprintf(stdout, "=");
	};

	for (i; i < width; i++) {
		fprintf(stdout, " ");
	};

	curl_off_t currentTime;
	curl_easy_getinfo(ps->curl, CURLINFO_TOTAL_TIME_T, &currentTime);

	// 1000000 us
	if ((currentTime - ps->lastTime) >= 1000000) {
		ps->lastTime = currentTime;
		ps->lastSpeed = nowDownloaded - ps->lastDownloaded;
		ps->lastDownloaded = nowDownloaded;
	};
	fprintf(stdout, "] (%.2f Mb / %.2f Mb) %3.2f Mb/s\r",
		static_cast<double>(nowDownloaded) / 1024.0 / 1024.0,
		static_cast<double>(totalToDownload) / 1024.0 / 1024.0,
		static_cast<double>(ps->lastSpeed) / 1024.0 / 1024.0);
	fflush(stdout);

	return 0;
};