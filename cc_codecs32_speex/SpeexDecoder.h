#define MAX_FRAME_SIZE 2000

enum OutputType
{
    PCM,
    WAV
};

class SpeexDecoder
{
public:
    SpeexDecoder();
    ~SpeexDecoder();

    int     Initialize(const char* spxFileName);
    char*   Decode(char** outBuf, int* size);
    void    Close();
    int     GetChannels() { return channels; }
    long    GetRate() { return rate; }
    int     Duration() { return duration; }

private:
    int     channels;
    long    rate;
    int     duration;
    bool    closed;
    char*   spxFileName;
    FILE *fin;
    int close_in;
    ogg_sync_state oy;
    SpeexBits bits;
    void* process_header(ogg_packet *op, spx_int32_t enh_enabled, spx_int32_t *frame_size, int *granule_frame_size, spx_int32_t *rate, int *nframes, int forceMode, int *channels, SpeexStereoState *stereo, int *extra_headers, int quiet);
    void SpeexDecoder::print_comments(char *comments, int length);
    FILE* SpeexDecoder::out_file_open(char *outFile, int rate, int *channels);
};
