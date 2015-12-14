
bool  __stdcall EncodeSpeexFromBuffer(const  char *outFilename, int qualityIn, char * buffer, size_t buffer_size);
bool  __stdcall EncodeSpeexFromFile(const char *inFilename, const char *outFilename, int qualityIn);
bool  __stdcall EncodeSpeex(const char *inFile, int qualityIn, const   char *outFile);
bool __stdcall DecodeSpeex(const char *inFileName, char** outBuffer, int* outBufferSize);