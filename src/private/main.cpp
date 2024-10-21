#include <civitai_downloader.hpp>

#define INPUT_BUFFER_SIZE 1024

int main()
{
	fprintf(stdout, "CivitAI model downloader V0.2.2\n\n");
	CivitaiDownloader CIDownloader = CivitaiDownloader();

	// Some test links
	// Checkpoint
	// https://civitai.com/models/317902/t-ponynai3?modelVersionId=673299
	// LoRA
	// https://civitai.com/models/352581/vixons-pony-styles?modelVersionId=689357
	// Same LoRA, different version
	// https://civitai.com/models/352581?modelVersionId=687759
	// Same LoRA, no model version
	// https://civitai.com/models/352581

	char inputBuffer[INPUT_BUFFER_SIZE];
	while (true) {
		fprintf(stdout, "Enter link to download, or \"Q\" to exit:\n");
		fscanf_s(stdin, "%s", inputBuffer, INPUT_BUFFER_SIZE);
		fprintf(stdout, "\n");

		std::string inputBufferS(inputBuffer);
		if (inputBufferS == "Q" || inputBufferS == "q") {
			break;
		}
		else {
			if (!CIDownloader.downloadFromLink(inputBufferS)) {
				fprintf(stdout, "\n>>>SUCCESS<<<\n\n");
			}
			else {
				fprintf(stdout, "\n>>>FAILED<<<\n\n");
			};
		};
	};

	return 0;
}
