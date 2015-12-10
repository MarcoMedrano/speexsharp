#include "stdafx.h"
#include "speexencoder.h"

SpeexEncoder::SpeexEncoder()
{
	total_samples = 0;
	mode = NULL;
	vbr_enabled = 0;
	vbr_max = 0;
	abr_enabled = 0;
	vad_enabled = 0;
	dtx_enabled = 0;
	modeID = -1;
	rate = 0;
	chan = 1;
	fmt = 16;
	quality = 3;
	vbr_quality = 3;
	lsb = 1;
	nframes = 1;
	complexity = 3;
	bytes_written = 0;
	close_in = 0;
	close_out = 0;
	eos = 0;
	bitrate = 0;
	cumul_bits = 0;
	enc_frames = 0;
	wave_input = 0;
	preprocess = NULL;
	denoise_enabled = 0;
	agc_enabled = 0;
	lookahead = 0;
	closed = false;

}


int SpeexEncoder::Initialize(const char* filename, char* modeInput, int channels)
{
	fprintf(stderr, "SpeexEncoder: Initialize\n");
	SpeexHeader header;
	if (modeInput != NULL){
		if (strcmp(modeInput, "narrowband") == 0)
		{
			modeID = SPEEX_MODEID_NB;
		}
		else if (strcmp(modeInput, "wideband") == 0)
		{
			modeID = SPEEX_MODEID_WB;
		}
		else if (strcmp(modeInput, "ultra-wideband") == 0)
		{
			modeID = SPEEX_MODEID_UWB;
		}
	}
	if (channels > 0){
		chan = channels;
	}
	

	speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
	snprintf(vendor_string, sizeof(vendor_string), "Encoded with Speex %s", speex_version);

	comment_init(&comments, &comments_length, vendor_string);
	char outFile[100];
	strcpy(outFile, (char*)filename);
	strcat(outFile, ".spx");
	/*Initialize Ogg stream struct*/
	srand(time(NULL));
	if (ogg_stream_init(&os, rand()) == -1)
	{
		fprintf(stderr, "Error: stream init failed\n");
		exit(1);
	}

	if (strcmp(filename, "-") == 0)
	{
		_setmode(_fileno(stdin), _O_BINARY);
		fin = stdin;
	}
	else
	{
		fin = fopen(filename, "rb");
		if (!fin)
		{
			perror(filename);
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

   if (modeID == -1 )
   {
	   /* By default, use narrowband/8 kHz */
	   modeID = SPEEX_MODEID_NB;
   }
	if (modeID == SPEEX_MODEID_NB)
		rate = 8000;
	else if (modeID == SPEEX_MODEID_WB)
		rate = 16000;
	else if (modeID == SPEEX_MODEID_UWB)
		rate = 32000;

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

		   fprintf(stderr, "Encoding %d Hz audio using %s mode (%s)\n",
		   header.rate, mode->modeName, st_string);
   }

   /*Initialize Speex encoder*/
   st = speex_encoder_init(mode);
   if (strcmp(outFile, "-") == 0)
   {
	   _setmode(_fileno(stdout), _O_BINARY);
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


   return 0;
}


int SpeexEncoder::Encode()
{
	int id = -1;
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
	return 0;
}

SpeexEncoder::~SpeexEncoder()
{
	Close();
}


void SpeexEncoder::Close()
{
	speex_encoder_destroy(st);
	speex_bits_destroy(&bits);
	ogg_stream_clear(&os);

	if (close_in)
		fclose(fin);
	if (close_out)
		fclose(fout);
	closed = true;
}

void SpeexEncoder::SetQuality(int qualityIn){

	fprintf(stderr, "SpeexEncoder: SetQuality %i \n", qualityIn);
	quality = qualityIn;
	vbr_quality = qualityIn;

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
}



/*Write an Ogg page to a file pointer*/
int  SpeexEncoder::oe_write_page(ogg_page *page, FILE *fp)
{
	int written;
	written = fwrite(page->header, 1, page->header_len, fp);
	written += fwrite(page->body, 1, page->body_len, fp);

	return written;
}

/* Convert input audio bits, endians and channels */
int SpeexEncoder::read_samples(FILE *fin, int frame_size, int bits, int channels, int lsb, short * input, char *buff, spx_int32_t *size)
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

void SpeexEncoder::add_fishead_packet(ogg_stream_state *os) {

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
void SpeexEncoder::add_fisbone_packet(ogg_stream_state *os, spx_int32_t serialno, SpeexHeader *header) {

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

void SpeexEncoder::version()
{
	const char* speex_version;
	speex_lib_ctl(SPEEX_LIB_GET_VERSION_STRING, (void*)&speex_version);
	printf("speexenc (Speex encoder) version %s (compiled " __DATE__ ")\n", speex_version);
	printf("Copyright (C) 2002-2006 Jean-Marc Valin\n");
}

void SpeexEncoder::version_short()
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

void SpeexEncoder::comment_init(char **comments, int* length, char *vendor_string)
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
void SpeexEncoder::comment_add(char **comments, int* length, char *tag, char *val)
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