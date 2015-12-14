#define MAX_FRAME_SIZE 2000

class SpeexDecoder
{
public:
    SpeexDecoder();
    ~SpeexDecoder();

    int     Initialize(const char* spxFileName);
    bool    Decode(char** outBuf, int* size);
    void    Close();
    int     GetChannels() { return channels; }
    long    GetRate() { return rate; }
    int     Duration() { return duration; }

private:
    int     channels;
    long    rate;
    int     duration;
    char*   spxFileName;
    FILE*   fin;
    bool    canCloseFileInput;
    ogg_sync_state  oy;
    SpeexBits       bits;

    void*    ProcessHeader(ogg_packet *op, spx_int32_t enh_enabled, spx_int32_t *frame_size, int *granule_frame_size, spx_int32_t *rate, int *nframes, int forceMode, int *channels, SpeexStereoState *stereo, int *extra_headers, int quiet);
    void     PrintComments(char *comments, int length);
};
