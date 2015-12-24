#include "stdafx.h"
#include "speexencoder.h"
#include <stdio.h>
#include <string.h>
#include <vector>
void comment_init(char **comments, int* length, char *vendor_string);
void comment_add(char **comments, int* length, char *tag, char *val);

SpeexEncoder::SpeexEncoder()
{
	total_samples = 0;
	mode = NULL;
	modeID = -1;
	rate = 0;
	chan = 1;
	quality = 3;
	complexity = 3;
	bytes_written = 0;
	eos = 0;
	preprocess = NULL;
	lookahead = 0;
	closed = false;

}
int SpeexEncoder::Initialize(const char* filename, char* modeInput, int channels)
{

	return Initialize(filename, modeInput, channels, -1);
}

int SpeexEncoder::Initialize(const char* filename, char* modeInput, int channels, int pcmRate)
{
	char *comments;
	int comments_length;
	fprintf(stderr, "SpeexEncoder: Initialize\n");
	SpeexHeader header;
	fprintf(stderr, "modeInput: %s\n", modeInput);
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
	comment_add(&comments, &comments_length, "TITLE=", "ssss");
	comment_add(&comments, &comments_length, "AUTHOR=", "uptivity");
	comment_add(&comments, &comments_length, "FORMATTYPE=", (char*)speex_version);
	/*Initialize Ogg stream struct*/
	srand(time(NULL));
	if (ogg_stream_init(&os, rand()) == -1)
	{
		fprintf(stderr, "Error: stream init failed\n");
		exit(1);
	}
	rate = pcmRate;
   if (modeID == -1 )
   {
	   /* By default, use narrowband/8 kHz */
	   modeID = SPEEX_MODEID_NB;
	   if (pcmRate < 0){
		   rate = 8000;
	   }
   }
   if (pcmRate < 0){
	   if (modeID == SPEEX_MODEID_NB)
	   	rate = 8000;
	   else if (modeID == SPEEX_MODEID_WB)
	   	rate = 16000;
	   else if (modeID == SPEEX_MODEID_UWB)
	   	rate = 32000;
   }

   if (!mode)
	   mode = speex_lib_get_mode(modeID);

   speex_init_header(&header, rate, 1, mode);
   header.frames_per_packet = 1;
   header.vbr = 0;
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
   fout = fopen(filename, "wb");
	if (!fout)
	{
		perror(filename);
		exit(1);
	}

   speex_encoder_ctl(st, SPEEX_GET_FRAME_SIZE, &frame_size);
   speex_encoder_ctl(st, SPEEX_SET_COMPLEXITY, &complexity);
   speex_encoder_ctl(st, SPEEX_SET_SAMPLING_RATE, &rate);
   if (quality >= 0)
   {
	   speex_encoder_ctl(st, SPEEX_SET_QUALITY, &quality);
   }

   speex_encoder_ctl(st, SPEEX_GET_LOOKAHEAD, &lookahead);

   /*Write header*/
   {
	   int packet_size;
	   op.packet = (unsigned char *)speex_header_to_packet(&header, &packet_size);
	   op.bytes = packet_size;
	   op.b_o_s = 1;
	   op.e_o_s = 0;
	   op.granulepos = 0;
	   op.packetno = 0;

	   // submit the packet to the ogg streaming layer
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

   return 0;
}


int SpeexEncoder::EncodeFromBuffer(char * buffer, size_t buffer_size)
{
	//fprintf(stderr, "buffer size aa---- %i", buffer_size);
	FILE *f = tmpfile();//fopen("out.raw", "wb"); 
	fwrite(buffer, 1, buffer_size, f);
	fflush(f);
	rewind(f);
	return EncodeFromFile(f);
}



int SpeexEncoder::EncodeFromFile(FILE *fin)
{
	int id = -1;
	int nframes = 1;
	int lsb = 1;
	int fmt = 16;
	spx_int32_t size;
	
	nb_samples = read_samples(fin, frame_size, fmt, chan, lsb, input, &size);

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
		nb_samples = read_samples(fin, frame_size, fmt, chan, lsb, input, NULL);

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
		//printf("granulepos: %d %d %d %d %d %d\n", (int)op.granulepos, id, 2 + id / nframes, lookahead, 5, 6);
		op.packetno = 2 + id / nframes;
		ogg_stream_packetin(&os, &op);

		/*Write all new pages (most likely 0 or 1)*/
		while (ogg_stream_pageout(&os, &og))
		{
			ret = oe_write_page(&og, fout);
			if (ret != og.header_len + og.body_len)
			{
				fprintf(stderr, "Error: failed writing header to output stream\n");
				fclose(fin);
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
			fclose(fin);
			exit(1);
		}
		else
			bytes_written += ret;
	}

	fprintf(stderr, "Duration: %d\n", frame_size *id / rate);
	int minutes = (int)(frame_size *id / rate)/60;
	int seconds = (frame_size *id / rate) - (minutes * 60);
	fprintf(stderr, "Duration Minutes: %d\n", minutes);
	fprintf(stderr, "Duration Seconds: %d\n", seconds);
	
	fclose(fin);

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
	fclose(fout);
	closed = true;
}

void SpeexEncoder::SetQuality(int qualityIn)
{
	fprintf(stderr, "SpeexEncoder: SetQuality %i \n", qualityIn);
	quality = qualityIn;
	if (quality >= 0)
	{
		speex_encoder_ctl(st, SPEEX_SET_QUALITY, &quality);
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
int SpeexEncoder::read_samples(FILE *fin, int frame_size, int bits, int channels, int lsb, short * input, spx_int32_t *size)
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
		
	nb_read = fread(in, 1, bits / 8 * channels* frame_size, fin);

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



void SpeexEncoder::encoder_init()
{
	int quality = 5;
	modeID = SPEEX_MODEID_NB;
	mode = speex_lib_get_mode(modeID);
	encoder_state = speex_encoder_init(mode);
	speex_encoder_ctl(encoder_state, SPEEX_SET_QUALITY, &quality);
	speex_bits_init(&encoder_bits);
}



void SpeexEncoder::encoder_dispose()
{
	speex_encoder_destroy(encoder_state);
	speex_bits_destroy(&encoder_bits);
}

int SpeexEncoder::encoder_encode(const   short   *data, char   **output)
{
	////fprintf(stderr, "encoder_encode1\n");
	//for (int i = 0; i < FRAME_SIZE; i++){
	//	encoder_input[i] = data[i];
	//}


	//speex_encode_int(encoder_state,(short*) encoder_input, &encoder_bits);

	//speex_bits_insert_terminator(&encoder_bits);
	////fprintf(stderr, "encoder_encode2\n");
	//speex_bits_reset(&encoder_bits);
	////fprintf(stderr, "encoder_encode3\n");
	//speex_encode(encoder_state, encoder_input, &encoder_bits);
	////fprintf(stderr, "encoder_encode4\n");
	//char  outputBuffer[200];
	//int size;
	////fprintf(stderr, "encoder_encode4.5\n");
	//size = speex_bits_write(&encoder_bits, outputBuffer, 200);
	char *inFile;
	FILE *fin;
	short in[FRAME_SIZE];
	inFile = "gate10.decode.raw";//argv[1];
	fin = fopen(inFile, "r");
	float input[FRAME_SIZE];
	int i;
	/*Initialization of the structure that holds the bits*/
	speex_bits_init(&bits);
	typedef std::vector<char> buffer_type;
	buffer_type myData;
	while (1)
	{
		/*Read a 16 bits/sample audio frame*/
		fread(in, sizeof(short), FRAME_SIZE, fin);
		if (feof(fin))
			break;
		/*Copy the 16 bits values to float so Speex can work on them*/
		for (i = 0; i<FRAME_SIZE; i++)
			input[i] = in[i];

		/*Flush all the bits in the struct so we can encode a new frame*/
		speex_bits_reset(&bits);

		/*Encode the frame*/
		speex_encode(encoder_state, input, &bits);
		/*Copy the bits to an array of char that can be written*/
		nbBytes = speex_bits_write(&bits, cbits, MAX_FRAME_BYTES);

		/*Write the size of the frame first. This is what sampledec expects but
		it's likely to be different in your own application*/
		fwrite(&nbBytes, sizeof(int), 1, stdout);
		/*Write the compressed data*/
		fwrite(cbits, 1, nbBytes, stdout);
		fprintf(stderr, "EncodeTest2 %i\n" , myData.size());
		myData.insert(myData.end(), cbits, cbits + nbBytes);
		
	}

	char *buffer = new char[myData.size()];
	std::copy(myData.begin(), myData.end(), buffer);
	fprintf(stderr, "EncodeTest2 %i\n", sizeof(buffer));
	//fprintf(stderr, "encoder_encode5\n");
	*output = buffer;
	//fprintf(stderr, "encoder_encode6 %i\n", size);
	return myData.size();
}