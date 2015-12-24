
bool  __stdcall EncodeSpeexFromBuffer(const  char *outFilename, int qualityIn, char * buffer, size_t buffer_size, int pcmRate);
bool  __stdcall EncodeSpeexFromFile(const char *inFilename, const char *outFilename, int qualityIn, int pcmRate);
bool  __stdcall EncodeSpeex(const char *inFile, int qualityIn, const   char *outFile);
bool __stdcall DecodeSpeex(const char *inFileName, char** outBuffer, int* outBufferSize);
int  __stdcall EncodeTest(const short *data, char** outBuffer);