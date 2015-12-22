// This is the main DLL file.

#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "speex/speex_callbacks.h"
#include "wave_out.h"
#include <io.h>
#include <fcntl.h>
#include "skeleton.h"
#include "speexencoder.h";
#include "SpeexDecoder.h"

using namespace std;

static char * ReadAllBytes(const char * filename, size_t * buffer_size)
{
	FILE * pFile;
	long lSize;
	char * buffer;
	size_t result;
	pFile = fopen(filename, "rb");
	if (pFile == NULL) { fputs("File error", stderr); exit(1); }

	// obtain file size:
	fseek(pFile, 0, SEEK_END);
	lSize = ftell(pFile);
	rewind(pFile);

	// allocate memory to contain the whole file:
	buffer = (char*)malloc(sizeof(unsigned char)*lSize);
	if (buffer == NULL) { fputs("Memory error", stderr); exit(2); }
	// copy the file into the buffer:
	result = fread(buffer, 1, lSize, pFile);
	if (result != lSize) { fputs("Reading error", stderr); exit(3); }
	/* the whole file is now loaded in the memory buffer. */
	*buffer_size = result;
	// terminate
	fclose(pFile);
	return buffer;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

bool  __stdcall EncodeSpeexFromBuffer(const  char *outFilename, int qualityIn, int bandMode, int channels, char * buffer, size_t buffer_size, int pcmRate)
{
	SpeexEncoder* encoder = new SpeexEncoder();
	char * modeInput = "narrowband";
	if (bandMode == 1){ modeInput = "wideband"; }
	else if (bandMode == 2){ modeInput = "ultra-wideband"; }
	encoder->Initialize(outFilename, modeInput, channels, pcmRate);
	encoder->SetQuality(qualityIn);
	encoder->EncodeFromBuffer(buffer, buffer_size);
	encoder->Close();
	return 0;
}

bool  __stdcall EncodeSpeexFromFile(const char *inFilename, const char *outFilename, int qualityIn, int bandMode, int channels, int pcmRate)
{
	SpeexEncoder* encoder = new SpeexEncoder();
	char * modeInput = "narrowband";
	if (bandMode == 1){ modeInput = "wideband"; }
	else if (bandMode == 2){ modeInput = "ultra-wideband"; }
	encoder->Initialize(outFilename, modeInput, channels, pcmRate);
	encoder->SetQuality(qualityIn);
	FILE *fin = fopen(inFilename, "rb");
	if (!fin)
	{
		perror(inFilename);
		exit(1);
	}
	encoder->EncodeFromFile(fin);
	encoder->Close();
	return 0;
}

extern  "C" __declspec(dllexport) bool __stdcall DecodeSpeex(const char *inFileName, char** outBuffer, int* outBufferSize)
{
    SpeexDecoder* decoder = new SpeexDecoder();
    decoder->Initialize(inFileName);
    return decoder->Decode(outBuffer, outBufferSize);
}