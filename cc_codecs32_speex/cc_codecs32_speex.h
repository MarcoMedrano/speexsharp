int __stdcall EncodeSpeex(const   char *inFile, int qualityIn, const   char *outFile);
bool __stdcall DecodeSpeex(const   char *inFile, unsigned char **outBuf, int* size);