// This is the main DLL file.

#include "stdafx.h"
#include <stdio.h>
#if !defined WIN32 && !defined _WIN32
#include <unistd.h>
#endif
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/*Encode stuff*/
#include <speex/speex.h>
#include <ogg/ogg.h>
#include "wav_io.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include <speex/speex_stereo.h>
#include <speex/speex_header.h>
/*Finished Encode stuff*/
/*Decode stuff*/
#include "speex/speex_callbacks.h"
#include "wave_out.h"
#include <io.h>
#include <fcntl.h>
/*Finished decode stuff*/

#if defined WIN32 || defined _WIN32
/* We need the following two to set stdout to binary */
#include <io.h>
#include <fcntl.h>
#endif

#include "skeleton.h"


/*Write an Ogg page to a file pointer*/
int oe_write_page(ogg_page *page, FILE *fp)
{
	int written;
	written = fwrite(page->header, 1, page->header_len, fp);
	written += fwrite(page->body, 1, page->body_len, fp);

	return written;
}


#define MAX_FRAME_SIZE 2000
#define MAX_FRAME_BYTES 2000

/* Convert input audio bits, endians and channels */
static int read_samples(FILE *fin, int frame_size, int bits, int channels, int lsb, short * input, char *buff, spx_int32_t *size)
{
	unsigned char in[MAX_FRAME_BYTES * 2];
	int i;
	short *s;
	int nb_read;

	if (size && *size <= 0)
	{
		return 0;
	}
	/*Read input audio*/
	if (size)
		*size -= bits / 8 * channels*frame_size;
	if (buff)
	{
		for (i = 0; i<12; i++)
			in[i] = buff[i];
		nb_read = fread(in + 12, 1, bits / 8 * channels*frame_size - 12, fin) + 12;
		if (size)
			*size += 12;
	}
	else {
		nb_read = fread(in, 1, bits / 8 * channels* frame_size, fin);
	}
	nb_read /= bits / 8 * channels;

	/*fprintf (stderr, "%d\n", nb_read);*/
	if (nb_read == 0)
		return 0;

	s = (short*)in;
	if (bits == 8)
	{
		/* Convert 8->16 bits */
		for (i = frame_size*channels - 1; i >= 0; i--)
		{
			s[i] = (in[i] << 8) ^ 0x8000;
		}
	}
	else
	{
		/* convert to our endian format */
		for (i = 0; i<frame_size*channels; i++)
		{
			if (lsb)
				s[i] = le_short(s[i]);
			else
				s[i] = be_short(s[i]);
		}
	}

	/* FIXME: This is probably redundent now */
	/* copy to float input buffer */
	for (i = 0; i<frame_size*channels; i++)
	{
		input[i] = (short)s[i];
	}

	for (i = nb_read*channels; i<frame_size*channels; i++)
	{
		input[i] = 0;
	}


	return nb_read;
}

void add_fishead_packet(ogg_stream_state *os) {

	fishead_packet fp;

	memset(&fp, 0, sizeof(fp));
	fp.ptime_n = 0;
	fp.ptime_d = 1000;
	fp.btime_n = 0;
	fp.btime_d = 1000;

	add_fishead_to_stream(os, &fp);
}

/*
* Adds the fishead packets in the skeleton output stream along with the e_o_s packet
*/
void add_fisbone_packet(ogg_stream_state *os, spx_int32_t serialno, SpeexHeader *header) {

	fisbone_packet fp;

	memset(&fp, 0, sizeof(fp));
	fp.serial_no = serialno;
	fp.nr_header_packet = 2 + header->extra_headers;
	fp.granule_rate_n = header->rate;
	fp.granule_rate_d = 1;
	fp.start_granule = 0;
	fp.preroll = 3;
	fp.granule_shift = 0;

	add_message_header_field(&fp, "Content-Type", "audio/x-speex");

	add_fisbone_to_stream(os, &fp);
}

void version()
{
	const char* speex_version;
	speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
	printf("speexenc (Speex encoder) version %s (compiled " __DATE__ ")\n", speex_version);
	printf("Copyright (C) 2002-2006 Jean-Marc Valin\n");
}

void version_short()
{
	const char* speex_version;
	speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
	printf("speexenc version %s\n", speex_version);
	printf("Copyright (C) 2002-2006 Jean-Marc Valin\n");
}



/*
Comments will be stored in the Vorbis style.
It is describled in the "Structure" section of
http://www.xiph.org/ogg/vorbis/doc/v-comment.html

The comment header is decoded as follows:
1) [vendor_length] = read an unsigned integer of 32 bits
2) [vendor_string] = read a UTF-8 vector as [vendor_length] octets
3) [user_comment_list_length] = read an unsigned integer of 32 bits
4) iterate [user_comment_list_length] times {
5) [length] = read an unsigned integer of 32 bits
6) this iteration's user comment = read a UTF-8 vector as [length] octets
}
7) [framing_bit] = read a single bit as boolean
8) if ( [framing_bit]  unset or end of packet ) then ERROR
9) done.

If you have troubles, please write to ymnk@jcraft.com.
*/

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
  	           	    (buf[base]&0xff))
#define writeint(buf, base, val) do{ buf[base+3]=((val)>>24)&0xff; \
                                     buf[base+2]=((val)>>16)&0xff; \
                                     buf[base+1]=((val)>>8)&0xff; \
                                     buf[base]=(val)&0xff; \
                                 }while(0)

void comment_init(char **comments, int* length, char *vendor_string)
{
	int vendor_length = strlen(vendor_string);
	int user_comment_list_length = 0;
	int len = 4 + vendor_length + 4;
	char *p = (char*)malloc(len);
	if (p == NULL){
		fprintf(stderr, "malloc failed in comment_init()\n");
		exit(1);
	}
	writeint(p, 0, vendor_length);
	memcpy(p + 4, vendor_string, vendor_length);
	writeint(p, 4 + vendor_length, user_comment_list_length);
	*length = len;
	*comments = p;
}
void comment_add(char **comments, int* length, char *tag, char *val)
{
	char* p = *comments;
	int vendor_length = readint(p, 0);
	int user_comment_list_length = readint(p, 4 + vendor_length);
	int tag_len = (tag ? strlen(tag) : 0);
	int val_len = strlen(val);
	int len = (*length) + 4 + tag_len + val_len;

	p = (char*)realloc(p, len);
	if (p == NULL){
		fprintf(stderr, "realloc failed in comment_add()\n");
		exit(1);
	}

	writeint(p, *length, tag_len + val_len);      /* length of comment */
	if (tag) memcpy(p + *length + 4, tag, tag_len);  /* comment */
	memcpy(p + *length + 4 + tag_len, val, val_len);  /* comment */
	writeint(p, 4 + vendor_length, user_comment_list_length + 1);

	*comments = p;
	*length = len;
}
#undef readint
#undef writeint

/******************DECODING STUFF******************/
static void *process_header(ogg_packet *op, spx_int32_t enh_enabled, spx_int32_t *frame_size, int *granule_frame_size, spx_int32_t *rate, int *nframes, int forceMode, int *channels, SpeexStereoState *stereo, int *extra_headers, int quiet)
{
    void *st;
    const SpeexMode *mode;
    SpeexHeader *header;
    int modeID;
    SpeexCallback callback;

    header = speex_packet_to_header((char*)op->packet, op->bytes);
    if (!header)
    {
        fprintf(stderr, "Cannot read header\n");
        return NULL;
    }
    if (header->mode >= SPEEX_NB_MODES || header->mode<0)
    {
        fprintf(stderr, "Mode number %d does not (yet/any longer) exist in this version\n",
            header->mode);
        free(header);
        return NULL;
    }

    modeID = header->mode;
    if (forceMode != -1)
        modeID = forceMode;

    mode = speex_lib_get_mode(modeID);

    if (header->speex_version_id > 1)
    {
        fprintf(stderr, "This file was encoded with Speex bit-stream version %d, which I don't know how to decode\n", header->speex_version_id);
        free(header);
        return NULL;
    }

    if (mode->bitstream_version < header->mode_bitstream_version)
    {
        fprintf(stderr, "The file was encoded with a newer version of Speex. You need to upgrade in order to play it.\n");
        free(header);
        return NULL;
    }
    if (mode->bitstream_version > header->mode_bitstream_version)
    {
        fprintf(stderr, "The file was encoded with an older version of Speex. You would need to downgrade the version in order to play it.\n");
        free(header);
        return NULL;
    }

    st = speex_decoder_init(mode);
    if (!st)
    {
        fprintf(stderr, "Decoder initialization failed.\n");
        free(header);
        return NULL;
    }
    speex_decoder_ctl(st, SPEEX_SET_ENH, &enh_enabled);
    speex_decoder_ctl(st, SPEEX_GET_FRAME_SIZE, frame_size);
    *granule_frame_size = *frame_size;

    if (!*rate)
        *rate = header->rate;
    /* Adjust rate if --force-* options are used */
    if (forceMode != -1)
    {
        if (header->mode < forceMode)
        {
            *rate <<= (forceMode - header->mode);
            *granule_frame_size >>= (forceMode - header->mode);
        }
        if (header->mode > forceMode)
        {
            *rate >>= (header->mode - forceMode);
            *granule_frame_size <<= (header->mode - forceMode);
        }
    }


    speex_decoder_ctl(st, SPEEX_SET_SAMPLING_RATE, rate);

    *nframes = header->frames_per_packet;

    if (*channels == -1)
        *channels = header->nb_channels;

    if (!(*channels == 1))
    {
        *channels = 2;
        callback.callback_id = SPEEX_INBAND_STEREO;
        callback.func = speex_std_stereo_request_handler;
        callback.data = stereo;
        speex_decoder_ctl(st, SPEEX_SET_HANDLER, &callback);
    }

    if (!quiet)
    {
        fprintf(stderr, "Decoding %d Hz audio using %s mode",
            *rate, mode->modeName);

        if (*channels == 1)
            fprintf(stderr, " (mono");
        else
            fprintf(stderr, " (stereo");

        if (header->vbr)
            fprintf(stderr, ", VBR)\n");
        else
            fprintf(stderr, ")\n");
        /*fprintf (stderr, "Decoding %d Hz audio at %d bps using %s mode\n",
        *rate, mode->bitrate, mode->modeName);*/
    }

    *extra_headers = header->extra_headers;

    free(header);
    return st;
}

FILE *out_file_open(char *outFile, int rate, int *channels)
{
    FILE *fout = NULL;
    /*Open output file*/
    if (strlen(outFile) == 0)
    {
#if defined HAVE_SYS_SOUNDCARD_H
        int audio_fd, format, stereo;
        audio_fd = open("/dev/dsp", O_WRONLY);
        if (audio_fd<0)
        {
            perror("Cannot open /dev/dsp");
            exit(1);
        }

        format = AFMT_S16_NE;
        if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format) == -1)
        {
            perror("SNDCTL_DSP_SETFMT");
            close(audio_fd);
            exit(1);
        }

        stereo = 0;
        if (*channels == 2)
            stereo = 1;
        if (ioctl(audio_fd, SNDCTL_DSP_STEREO, &stereo) == -1)
        {
            perror("SNDCTL_DSP_STEREO");
            close(audio_fd);
            exit(1);
        }
        if (stereo != 0)
        {
            if (*channels == 1)
                fprintf(stderr, "Cannot set mono mode, will decode in stereo\n");
            *channels = 2;
        }

        if (ioctl(audio_fd, SNDCTL_DSP_SPEED, &rate) == -1)
        {
            perror("SNDCTL_DSP_SPEED");
            close(audio_fd);
            exit(1);
        }
        fout = fdopen(audio_fd, "w");
#elif defined HAVE_SYS_AUDIOIO_H
        audio_info_t info;
        int audio_fd;

        audio_fd = open("/dev/audio", O_WRONLY);
        if (audio_fd<0)
        {
            perror("Cannot open /dev/audio");
            exit(1);
        }

        AUDIO_INITINFO(&info);
#ifdef AUMODE_PLAY    /* NetBSD/OpenBSD */
        info.mode = AUMODE_PLAY;
#endif
        info.play.encoding = AUDIO_ENCODING_SLINEAR;
        info.play.precision = 16;
        info.play.sample_rate = rate;
        info.play.channels = *channels;

        if (ioctl(audio_fd, AUDIO_SETINFO, &info) < 0)
        {
            perror("AUDIO_SETINFO");
            exit(1);
        }
        fout = fdopen(audio_fd, "w");
#elif defined WIN32 || defined _WIN32
        {
            unsigned int speex_channels = *channels;
            if (Set_WIN_Params(INVALID_FILEDESC, rate, SAMPLE_SIZE, speex_channels))
            {
                fprintf(stderr, "Can't access %s\n", "WAVE OUT");
                exit(1);
            }
        }
#else
        fprintf(stderr, "No soundcard support\n");
        exit(1);
#endif
    }
    else {
        if (strcmp(outFile, "-") == 0)
        {
#if defined WIN32 || defined _WIN32
            _setmode(_fileno(stdout), _O_BINARY);
#elif defined OS2
            _fsetmode(stdout, "b");
#endif
            fout = stdout;
        }
        else
        {
            fout = fopen(outFile, "wb");
            if (!fout)
            {
                perror(outFile);
                exit(1);
            }
            if (strcmp(outFile + strlen(outFile) - 4, ".wav") == 0 || strcmp(outFile + strlen(outFile) - 4, ".WAV") == 0)
                write_wav_header(fout, rate, *channels, 0, 0);
        }
    }
    return fout;
}

#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
                            (buf[base]&0xff))

static void print_comments(char *comments, int length)
{
    char *c = comments;
    int len, i, nb_fields;
    char *end;

    if (length<8)
    {
        fprintf(stderr, "Invalid/corrupted comments\n");
        return;
    }
    end = c + length;
    len = readint(c, 0);
    c += 4;
    if (len < 0 || c + len>end)
    {
        fprintf(stderr, "Invalid/corrupted comments\n");
        return;
    }
    fwrite(c, 1, len, stderr);
    c += len;
    fprintf(stderr, "\n");
    if (c + 4>end)
    {
        fprintf(stderr, "Invalid/corrupted comments\n");
        return;
    }
    nb_fields = readint(c, 0);
    c += 4;
    for (i = 0; i<nb_fields; i++)
    {
        if (c + 4>end)
        {
            fprintf(stderr, "Invalid/corrupted comments\n");
            return;
        }
        len = readint(c, 0);
        c += 4;
        if (len < 0 || c + len>end)
        {
            fprintf(stderr, "Invalid/corrupted comments\n");
            return;
        }
        fwrite(c, 1, len, stderr);
        c += len;
        fprintf(stderr, "\n");
    }
}

/***************Finish decoding stuff***************/
BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    return TRUE;
}

extern  "C" __declspec(dllexport) int test_plus_plus(int number)
{
    return ++number;
}

extern  "C" __declspec(dllexport) int encoder_encode(const   char *inFile, const   char *outFile)
{
	int nb_samples, total_samples = 0, nb_encoded;
	int c;
	int option_index = 0;
	//char *inFile, *outFile;
	FILE *fin, *fout;
	short input[MAX_FRAME_SIZE];
	spx_int32_t frame_size;
	int quiet = 0;
	spx_int32_t vbr_enabled = 0;
	spx_int32_t vbr_max = 0;
	int abr_enabled = 0;
	spx_int32_t vad_enabled = 0;
	spx_int32_t dtx_enabled = 0;
	int nbBytes;
	const SpeexMode *mode = NULL;
	int modeID = -1;
	void *st;
	SpeexBits bits;
	char cbits[MAX_FRAME_BYTES];
	spx_int32_t rate = 0;
	spx_int32_t size;
	int chan = 1;
	int fmt = 16;
	spx_int32_t quality = -1;
	float vbr_quality = -1;
	int lsb = 1;
	ogg_stream_state os;
	ogg_page 		 og;
	ogg_packet 		 op;
	int bytes_written = 0, ret, result;
	int id = -1;
	SpeexHeader header;
	int nframes = 1;
	spx_int32_t complexity = 3;
	const char* speex_version;
	char vendor_string[64];
	char *comments;
	int comments_length;
	int close_in = 0, close_out = 0;
	int eos = 0;
	spx_int32_t bitrate = 0;
	double cumul_bits = 0, enc_frames = 0;
	char first_bytes[12];
	int wave_input = 0;
	spx_int32_t tmp;
	SpeexPreprocessState *preprocess = NULL;
	int denoise_enabled = 1, agc_enabled = 0;
	spx_int32_t lookahead = 0;

	speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
	snprintf(vendor_string, sizeof(vendor_string), "Encoded with Speex %s", speex_version);

	comment_init(&comments, &comments_length, vendor_string);
	//inFile = "gate10.decode.raw";
	//outFile = "agmu.spx";

	/*Initialize Ogg stream struct*/
	srand(time(NULL));
	if (ogg_stream_init(&os, rand()) == -1)
	{
		fprintf(stderr, "Error: stream init failed\n");
		exit(1);
	}

	if (strcmp(inFile, "-") == 0)
	{
#if defined WIN32 || defined _WIN32
		_setmode(_fileno(stdin), _O_BINARY);
#elif defined OS2
		_fsetmode(stdin, "b");
#endif
		fin = stdin;
	}
	else
	{
		fin = fopen(inFile, "rb");
		if (!fin)
		{
			perror(inFile);
			exit(1);
		}
		close_in = 1;
	}

   {
	   fread(first_bytes, 1, 12, fin);
	   if (strncmp(first_bytes, "RIFF", 4) == 0 && strncmp(first_bytes, "RIFF", 4) == 0)
	   {
		   if (read_wav_header(fin, &rate, &chan, &fmt, &size) == -1)
			   exit(1);
		   wave_input = 1;
		   lsb = 1; /* CHECK: exists big-endian .wav ?? */
	   }
   }

   if (modeID == -1 && !rate)
   {
	   /* By default, use narrowband/8 kHz */
	   modeID = SPEEX_MODEID_NB;
	   rate = 8000;
   }
   else if (modeID != -1 && rate)
   {
	   mode = speex_lib_get_mode(modeID);
	   if (rate > 48000)
	   {
		   fprintf(stderr, "Error: sampling rate too high: %d Hz, try down-sampling\n", rate);
		   exit(1);
	   }
	   else if (rate > 25000)
	   {
		   if (modeID != SPEEX_MODEID_UWB)
		   {
			   fprintf(stderr, "Warning: Trying to encode in %s at %d Hz. I'll do it but I suggest you try ultra-wideband instead\n", mode->modeName, rate);
		   }
	   }
	   else if (rate > 12500)
	   {
		   if (modeID != SPEEX_MODEID_WB)
		   {
			   fprintf(stderr, "Warning: Trying to encode in %s at %d Hz. I'll do it but I suggest you try wideband instead\n", mode->modeName, rate);
		   }
	   }
	   else if (rate >= 6000)
	   {
		   if (modeID != SPEEX_MODEID_NB)
		   {
			   fprintf(stderr, "Warning: Trying to encode in %s at %d Hz. I'll do it but I suggest you try narrowband instead\n", mode->modeName, rate);
		   }
	   }
	   else {
		   fprintf(stderr, "Error: sampling rate too low: %d Hz\n", rate);
		   exit(1);
	   }
   }
   else if (modeID == -1)
   {
	   if (rate > 48000)
	   {
		   fprintf(stderr, "Error: sampling rate too high: %d Hz, try down-sampling\n", rate);
		   exit(1);
	   }
	   else if (rate > 25000)
	   {
		   modeID = SPEEX_MODEID_UWB;
	   }
	   else if (rate > 12500)
	   {
		   modeID = SPEEX_MODEID_WB;
	   }
	   else if (rate >= 6000)
	   {
		   modeID = SPEEX_MODEID_NB;
	   }
	   else {
		   fprintf(stderr, "Error: Sampling rate too low: %d Hz\n", rate);
		   exit(1);
	   }
   }
   else if (!rate)
   {
	   if (modeID == SPEEX_MODEID_NB)
		   rate = 8000;
	   else if (modeID == SPEEX_MODEID_WB)
		   rate = 16000;
	   else if (modeID == SPEEX_MODEID_UWB)
		   rate = 32000;
   }

   if (!quiet)
	   if (rate != 8000 && rate != 16000 && rate != 32000)
		   fprintf(stderr, "Warning: Speex is only optimized for 8, 16 and 32 kHz. It will still work at %d Hz but your mileage may vary\n", rate);

   if (!mode)
	   mode = speex_lib_get_mode(modeID);

   speex_init_header(&header, rate, 1, mode);
   header.frames_per_packet = nframes;
   header.vbr = vbr_enabled;
   header.nb_channels = chan;

   {
	   char *st_string = "mono";
	   if (chan == 2)
		   st_string = "stereo";
	   if (!quiet)
		   fprintf(stderr, "Encoding %d Hz audio using %s mode (%s)\n",
		   header.rate, mode->modeName, st_string);
   }

   /*Initialize Speex encoder*/
   st = speex_encoder_init(mode);
   if (strcmp(outFile, "-") == 0)
   {
#if defined WIN32 || defined _WIN32
	   _setmode(_fileno(stdout), _O_BINARY);
#endif
	   fout = stdout;
   }
   else
   {
	   fout = fopen(outFile, "wb");
	   if (!fout)
	   {
		   perror(outFile);
		   exit(1);
	   }
	   close_out = 1;
   }
   speex_encoder_ctl(st, SPEEX_GET_FRAME_SIZE, &frame_size);
   speex_encoder_ctl(st, SPEEX_SET_COMPLEXITY, &complexity);
   speex_encoder_ctl(st, SPEEX_SET_SAMPLING_RATE, &rate);
   if (quality >= 0)
   {
	   if (vbr_enabled)
	   {
		   if (vbr_max > 0)
			   speex_encoder_ctl(st, SPEEX_SET_VBR_MAX_BITRATE, &vbr_max);
		   speex_encoder_ctl(st, SPEEX_SET_VBR_QUALITY, &vbr_quality);
	   }
	   else
		   speex_encoder_ctl(st, SPEEX_SET_QUALITY, &quality);
   }
   if (bitrate)
   {
	   if (quality >= 0 && vbr_enabled)
		   fprintf(stderr, "Warning: --bitrate option is overriding --quality\n");
	   speex_encoder_ctl(st, SPEEX_SET_BITRATE, &bitrate);
   }
   if (vbr_enabled)
   {
	   tmp = 1;
	   speex_encoder_ctl(st, SPEEX_SET_VBR, &tmp);
   }
   else if (vad_enabled)
   {
	   tmp = 1;
	   speex_encoder_ctl(st, SPEEX_SET_VAD, &tmp);
   }
   if (dtx_enabled)
	   speex_encoder_ctl(st, SPEEX_SET_DTX, &tmp);
   if (dtx_enabled && !(vbr_enabled || abr_enabled || vad_enabled))
   {
	   fprintf(stderr, "Warning: --dtx is useless without --vad, --vbr or --abr\n");
   }
   else if ((vbr_enabled || abr_enabled) && (vad_enabled))
   {
	   fprintf(stderr, "Warning: --vad is already implied by --vbr or --abr\n");
   }

   if (abr_enabled)
   {
	   speex_encoder_ctl(st, SPEEX_SET_ABR, &abr_enabled);
   }

   speex_encoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);
   if (denoise_enabled || agc_enabled)
   {
	   preprocess = speex_preprocess_state_init(frame_size, rate);
	   speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_DENOISE, &denoise_enabled);
	   speex_preprocess_ctl(preprocess, SPEEX_PREPROCESS_SET_AGC, &agc_enabled);
	   lookahead += frame_size;
   }

   /*Write header*/
   {
	   int packet_size;
	   op.packet = (unsigned char *)speex_header_to_packet(&header, &packet_size);
	   op.bytes = packet_size;
	   op.b_o_s = 1;
	   op.e_o_s = 0;
	   op.granulepos = 0;
	   op.packetno = 0;
	   ogg_stream_packetin(&os, &op);
	   free(op.packet);

	   while ((result = ogg_stream_flush(&os, &og)))
	   {
		   if (!result) break;
		   ret = oe_write_page(&og, fout);
		   if (ret != og.header_len + og.body_len)
		   {
			   fprintf(stderr, "Error: failed writing header to output stream\n");
			   exit(1);
		   }
		   else
			   bytes_written += ret;
	   }

	   op.packet = (unsigned char *)comments;
	   op.bytes = comments_length;
	   op.b_o_s = 0;
	   op.e_o_s = 0;
	   op.granulepos = 0;
	   op.packetno = 1;
	   ogg_stream_packetin(&os, &op);
   }

   /* writing the rest of the speex header packets */
   while ((result = ogg_stream_flush(&os, &og)))
   {
	   if (!result) break;
	   ret = oe_write_page(&og, fout);
	   if (ret != og.header_len + og.body_len)
	   {
		   fprintf(stderr, "Error: failed writing header to output stream\n");
		   exit(1);
	   }
	   else
		   bytes_written += ret;
   }
   free(comments);

   speex_bits_init(&bits);

   if (!wave_input)
   {
	   nb_samples = read_samples(fin, frame_size, fmt, chan, lsb, input, first_bytes, NULL);
   }
   else {
	   nb_samples = read_samples(fin, frame_size, fmt, chan, lsb, input, NULL, &size);
   }
   if (nb_samples == 0)
	   eos = 1;
   total_samples += nb_samples;
   nb_encoded = -lookahead;
   /*Main encoding loop (one frame per iteration)*/
   while (!eos || total_samples > nb_encoded)
   {
	   id++;
	   /*Encode current frame*/
	   if (chan == 2)
		   speex_encode_stereo_int(input, frame_size, &bits);

	   if (preprocess)
		   speex_preprocess(preprocess, input, NULL);

	   speex_encode_int(st, input, &bits);

	   nb_encoded += frame_size;

	   if (wave_input)
	   {
		   nb_samples = read_samples(fin, frame_size, fmt, chan, lsb, input, NULL, &size);
	   }
	   else {
		   nb_samples = read_samples(fin, frame_size, fmt, chan, lsb, input, NULL, NULL);
	   }
	   if (nb_samples == 0)
	   {
		   eos = 1;
	   }
	   if (eos && total_samples <= nb_encoded)
		   op.e_o_s = 1;
	   else
		   op.e_o_s = 0;
	   total_samples += nb_samples;

	   if ((id + 1) % nframes != 0)
		   continue;

	   speex_bits_insert_terminator(&bits);
	   nbBytes = speex_bits_write(&bits, cbits, MAX_FRAME_BYTES);
	   speex_bits_reset(&bits);
	   op.packet = (unsigned char *)cbits;
	   op.bytes = nbBytes;
	   op.b_o_s = 0;
	   /*Is this redundent?*/
	   if (eos && total_samples <= nb_encoded)
		   op.e_o_s = 1;
	   else
		   op.e_o_s = 0;
	   op.granulepos = (id + 1)*frame_size - lookahead;
	   if (op.granulepos > total_samples)
		   op.granulepos = total_samples;
	   /*printf ("granulepos: %d %d %d %d %d %d\n", (int)op.granulepos, id, nframes, lookahead, 5, 6);*/
	   op.packetno = 2 + id / nframes;
	   ogg_stream_packetin(&os, &op);

	   /*Write all new pages (most likely 0 or 1)*/
	   while (ogg_stream_pageout(&os, &og))
	   {
		   ret = oe_write_page(&og, fout);
		   if (ret != og.header_len + og.body_len)
		   {
			   fprintf(stderr, "Error: failed writing header to output stream\n");
			   exit(1);
		   }
		   else
			   bytes_written += ret;
	   }
   }
   if ((id + 1) % nframes != 0)
   {
	   while ((id + 1) % nframes != 0)
	   {
		   id++;
		   speex_bits_pack(&bits, 15, 5);
	   }
	   nbBytes = speex_bits_write(&bits, cbits, MAX_FRAME_BYTES);
	   op.packet = (unsigned char *)cbits;
	   op.bytes = nbBytes;
	   op.b_o_s = 0;
	   op.e_o_s = 1;
	   op.granulepos = (id + 1)*frame_size - lookahead;
	   if (op.granulepos > total_samples)
		   op.granulepos = total_samples;

	   op.packetno = 2 + id / nframes;
	   ogg_stream_packetin(&os, &op);
   }
   /*Flush all pages left to be written*/
   while (ogg_stream_flush(&os, &og))
   {
	   ret = oe_write_page(&og, fout);
	   if (ret != og.header_len + og.body_len)
	   {
		   fprintf(stderr, "Error: failed writing header to output stream\n");
		   exit(1);
	   }
	   else
		   bytes_written += ret;
   }

   speex_encoder_destroy(st);
   speex_bits_destroy(&bits);
   ogg_stream_clear(&os);

   if (close_in)
	   fclose(fin);
   if (close_out)
	   fclose(fout);
   return 0;
}


extern  "C" __declspec(dllexport) bool decoder_decode(const   char *inFile, const   char *outFile)
{
    int c;
    int option_index = 0;
//    char *inFile, *outFile;
    FILE *fin, *fout = NULL;
    short out[MAX_FRAME_SIZE];
    short output[MAX_FRAME_SIZE];
    int frame_size = 0, granule_frame_size = 0;
    void *st = NULL;
    SpeexBits bits;
    int packet_count = 0;
    int stream_init = 0;
    int quiet = 0;
    ogg_int64_t page_granule = 0, last_granule = 0;
    int skip_samples = 0, page_nb_packets;
    
    ogg_sync_state oy;
    ogg_page       og;
    ogg_packet     op;
    ogg_stream_state os;
    int enh_enabled;
    int nframes = 2;
    int print_bitrate = 0;
    int close_in = 0;
    int eos = 0;
    int forceMode = -1;
    int audio_size = 0;
    float loss_percent = -1;
    SpeexStereoState stereo = SPEEX_STEREO_STATE_INIT;
    int channels = -1;
    int rate = 0;
    int extra_headers = 0;
    int wav_format = 0;
    int lookahead;
    int speex_serialno = -1;

    enh_enabled = 1;

    /*Open input file*/
    if (strcmp(inFile, "-") == 0)
    {
#if defined WIN32 || defined _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        fin = stdin;
    }
    else
    {
        fin = fopen(inFile, "rb");
        if (!fin)
        {
            perror(inFile);
            exit(1);
        }
        close_in = 1;
    }


    /*Init Ogg data struct*/
    ogg_sync_init(&oy);

    speex_bits_init(&bits);
    /*Main decoding loop*/

    while (1)
    {
        char *data;
        int i, j, nb_read;
        /*Get the ogg buffer for writing*/
        data = ogg_sync_buffer(&oy, 200);
        /*Read bitstream from input file*/
        nb_read = fread(data, sizeof(char), 200, fin);
        ogg_sync_wrote(&oy, nb_read);

        /*Loop for all complete pages we got (most likely only one)*/
        while (ogg_sync_pageout(&oy, &og) == 1)
        {
            int packet_no;
            if (stream_init == 0) {
                ogg_stream_init(&os, ogg_page_serialno(&og));
                stream_init = 1;
            }
            if (ogg_page_serialno(&og) != os.serialno) {
                /* so all streams are read. */
                ogg_stream_reset_serialno(&os, ogg_page_serialno(&og));
            }
            /*Add page to the bitstream*/
            ogg_stream_pagein(&os, &og);
            page_granule = ogg_page_granulepos(&og);
            page_nb_packets = ogg_page_packets(&og);
            if (page_granule>0 && frame_size)
            {
                /* FIXME: shift the granule values if --force-* is specified */
                skip_samples = frame_size*(page_nb_packets*granule_frame_size*nframes - (page_granule - last_granule)) / granule_frame_size;
                if (ogg_page_eos(&og))
                    skip_samples = -skip_samples;
                /*else if (!ogg_page_bos(&og))
                skip_samples = 0;*/
            }
            else
            {
                skip_samples = 0;
            }
            /*printf ("page granulepos: %d %d %d\n", skip_samples, page_nb_packets, (int)page_granule);*/
            last_granule = page_granule;
            /*Extract all available packets*/
            packet_no = 0;
            while (!eos && ogg_stream_packetout(&os, &op) == 1)
            {
                if (op.bytes >= 5 && !memcmp(op.packet, "Speex", 5)) {
                    speex_serialno = os.serialno;
                }
                if (speex_serialno == -1 || os.serialno != speex_serialno)
                    break;
                /*If first packet, process as Speex header*/
                if (packet_count == 0)
                {
                    st = process_header(&op, enh_enabled, &frame_size, &granule_frame_size, &rate, &nframes, forceMode, &channels, &stereo, &extra_headers, quiet);
                    if (!st)
                        exit(1);
                    speex_decoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);
                    if (!nframes)
                        nframes = 1;
                    fout = out_file_open((char *)outFile, rate, &channels);

                }
                else if (packet_count == 1)
                {
                    if (!quiet)
                        print_comments((char*)op.packet, op.bytes);
                }
                else if (packet_count <= 1 + extra_headers)
                {
                    /* Ignore extra headers */
                }
                else {
                    int lost = 0;
                    packet_no++;
                    if (loss_percent>0 && 100 * ((float)rand()) / RAND_MAX<loss_percent)
                        lost = 1;

                    /*End of stream condition*/
                    if (op.e_o_s && os.serialno == speex_serialno) /* don't care for anything except speex eos */
                        eos = 1;

                    /*Copy Ogg packet to Speex bitstream*/
                    speex_bits_read_from(&bits, (char*)op.packet, op.bytes);
                    for (j = 0; j != nframes; j++)
                    {
                        int ret;
                        /*Decode frame*/
                        if (!lost)
                            ret = speex_decode_int(st, &bits, output);
                        else
                            ret = speex_decode_int(st, NULL, output);

                        /*for (i=0;i<frame_size*channels;i++)
                        printf ("%d\n", (int)output[i]);*/

                        if (ret == -1)
                            break;
                        if (ret == -2)
                        {
                            fprintf(stderr, "Decoding error: corrupted stream?\n");
                            break;
                        }
                        if (speex_bits_remaining(&bits)<0)
                        {
                            fprintf(stderr, "Decoding overflow: corrupted stream?\n");
                            break;
                        }
                        if (channels == 2)
                            speex_decode_stereo_int(output, frame_size, &stereo);

                        if (print_bitrate) {
                            spx_int32_t tmp;
                            char ch = 13;
                            speex_decoder_ctl(st, SPEEX_GET_BITRATE, &tmp);
                            fputc(ch, stderr);
                            fprintf(stderr, "Bitrate is use: %d bps     ", tmp);
                        }
                        /*Convert to short and save to output file*/
                        if (strlen(outFile) != 0)
                        {
                            for (i = 0; i<frame_size*channels; i++)
                                out[i] = le_short(output[i]);
                        }
                        else {
                            for (i = 0; i<frame_size*channels; i++)
                                out[i] = output[i];
                        }
                        {
                            int frame_offset = 0;
                            int new_frame_size = frame_size;
                            /*printf ("packet %d %d\n", packet_no, skip_samples);*/
                            /*fprintf (stderr, "packet %d %d %d\n", packet_no, skip_samples, lookahead);*/
                            if (packet_no == 1 && j == 0 && skip_samples > 0)
                            {
                                /*printf ("chopping first packet\n");*/
                                new_frame_size -= skip_samples + lookahead;
                                frame_offset = skip_samples + lookahead;
                            }
                            if (packet_no == page_nb_packets && skip_samples < 0)
                            {
                                int packet_length = nframes*frame_size + skip_samples + lookahead;
                                new_frame_size = packet_length - j*frame_size;
                                if (new_frame_size<0)
                                    new_frame_size = 0;
                                if (new_frame_size>frame_size)
                                    new_frame_size = frame_size;
                                /*printf ("chopping end: %d %d %d\n", new_frame_size, packet_length, packet_no);*/
                            }
                            if (new_frame_size>0)
                            {
#if defined WIN32 || defined _WIN32
                                if (strlen(outFile) == 0)
                                    WIN_Play_Samples(out + frame_offset*channels, sizeof(short) * new_frame_size*channels);
                                else
#endif
                                    fwrite(out + frame_offset*channels, sizeof(short), new_frame_size*channels, fout);

                                audio_size += sizeof(short)*new_frame_size*channels;
                            }
                        }
                    }
                }
                packet_count++;
            }
        }
        if (feof(fin))
            break;

    }

    if (fout && wav_format)
    {
        if (fseek(fout, 4, SEEK_SET) == 0)
        {
            int tmp;
            tmp = le_int(audio_size + 36);
            fwrite(&tmp, 4, 1, fout);
            if (fseek(fout, 32, SEEK_CUR) == 0)
            {
                tmp = le_int(audio_size);
                fwrite(&tmp, 4, 1, fout);
            }
            else
            {
                fprintf(stderr, "First seek worked, second didn't\n");
            }
        }
        else {
            fprintf(stderr, "Cannot seek on wave file, size will be incorrect\n");
        }
    }

    if (st)
        speex_decoder_destroy(st);
    else
    {
        fprintf(stderr, "This doesn't look like a Speex file\n");
    }
    speex_bits_destroy(&bits);
    if (stream_init)
        ogg_stream_clear(&os);
    ogg_sync_clear(&oy);

#if defined WIN32 || defined _WIN32
    if (strlen(outFile) == 0)
        WIN_Audio_close();
#endif

    if (close_in)
        fclose(fin);
    if (fout != NULL)
        fclose(fout);

    return 1;
}

