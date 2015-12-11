#include <fstream>
#include "stdafx.h"
// This is the main DLL file.
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
#if defined WIN32 || defined _WIN32
/* We need the following two to set stdout to binary */
#include <io.h>
#include <fcntl.h>
#endif

#include "skeleton.h"

#define MAX_FRAME_SIZE 2000
#define MAX_FRAME_BYTES 2000

using namespace std;


class SpeexEncoder
{

private:
	int nb_samples, total_samples, nb_encoded;
	int c;
	FILE *fout;
	short input[MAX_FRAME_SIZE];
	const SpeexMode *mode;
	spx_int32_t frame_size;
	int nbBytes;
	int modeID;
	void *st;
	SpeexBits bits;
	char cbits[MAX_FRAME_BYTES];
	spx_int32_t rate;
	int chan;
	spx_int32_t quality;
	spx_int32_t complexity;
	const char* speex_version;
	char vendor_string[64];
	char *comments;
	int comments_length;
	int eos;
	double cumul_bits, enc_frames;
	SpeexPreprocessState *preprocess;
	spx_int32_t lookahead;

	ogg_stream_state os;
	ogg_stream_state so; /* ogg stream for skeleton bitstream */
	ogg_page 		 og;
	ogg_packet 		 op;
	int bytes_written, ret, result;

	bool closed;
public:
	SpeexEncoder();
	~SpeexEncoder();

	int				Initialize(const char* filename, char* modeInput, int channels);
	int				EncodeFromBuffer(char *buffer, size_t buffer_size);
	int				EncodeFromFile(FILE *fin);
	void			Close();
	void            SetQuality(int qualityIn);
	void            SetTitle(char* title);
	void            SetAuthor(char* author);

	/*Write an Ogg page to a file pointer*/
	int  oe_write_page(ogg_page *page, FILE *fp);
	/* Convert input audio bits, endians and channels */
	int read_samples(FILE *fin, int frame_size, int bits, int channels, int lsb, short * input, spx_int32_t *size);
	void add_fishead_packet(ogg_stream_state *os);
	/*
	* Adds the fishead packets in the skeleton output stream along with the e_o_s packet
	*/
	void add_fisbone_packet(ogg_stream_state *os, spx_int32_t serialno, SpeexHeader *header);
	void version();
	void version_short();
	void comment_init(char **comments, int* length, char *vendor_string);
	void comment_add(char **comments, int* length, char *tag, char *val);
};