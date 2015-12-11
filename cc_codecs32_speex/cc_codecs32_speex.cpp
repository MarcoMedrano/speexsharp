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
#include "speexencoder.h";
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

bool  __stdcall EncodeSpeexFromBuffer(const  char *outFilename, int qualityIn, char * buffer, size_t buffer_size)
{
	SpeexEncoder* encoder = new SpeexEncoder();
	encoder->Initialize(outFilename, NULL, 1);
	encoder->SetQuality(qualityIn);
	encoder->EncodeFromBuffer(buffer, buffer_size);
	encoder->Close();
	return 0;
}

bool  __stdcall EncodeSpeexFromFile(const char *inFilename, const char *outFilename, int qualityIn)
{
	SpeexEncoder* encoder = new SpeexEncoder();
	encoder->Initialize(outFilename, NULL, 1);
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

bool  __stdcall EncodeSpeex(const char *inFile, int qualityIn, const   char *outFile)
{
	SpeexEncoder* encoder = new SpeexEncoder();
	encoder->Initialize(outFile, NULL, 1);
	encoder->SetQuality(qualityIn);

	//FILE *fin;
	//char* inputfilename = "gate10.decode.raw";
	//fin = fopen(inputfilename, "rb");
	//if (!fin)
	//{
	//	perror(inputfilename);
	//	exit(1);
	//}

	size_t  buffer_size = 0;
	char  *pChars = ReadAllBytes(inFile, &buffer_size);
	encoder->EncodeFromBuffer(pChars, buffer_size);
	//encoder->EncodeFromFile(fin);
	encoder->Close();
	return 0;
}


extern  "C" __declspec(dllexport) bool  __stdcall DecodeSpeex(const   char *inFile, const   char *outFile)
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

