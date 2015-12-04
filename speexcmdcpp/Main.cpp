#define _CRT_SECURE_NO_WARNINGS
#include <speex/speex.h>
#include <stdio.h>

extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }

/*The frame size in hardcoded for this sample code but it doesn't have to be*/
/*Sample gotten from 
http://www.speex.org/docs/manual/speex-manual/node13.html
*/

int encode(char **argv);

#define FRAME_SIZE 160
int main(int argc, char **argv)
{
    return encode(argv);
}

int decode(char **argv)
{
    printf("DECODING!!\n");
    char *outFile;
    FILE *fout;
    /*Holds the audio that will be written to file (16 bits per sample)*/
    short out[FRAME_SIZE];
    /*Speex handle samples as float, so we need an array of floats*/
    float output[FRAME_SIZE];
    char cbits[200];
    int nbBytes;
    /*Holds the state of the decoder*/
    void *state;
    /*Holds bits so they can be read and written to by the Speex routines*/
    SpeexBits bits;
    int i, tmp;

    /*Create a new decoder state in narrowband mode*/
    state = speex_decoder_init(&speex_nb_mode);

    /*Set the perceptual enhancement on*/
    tmp = 1;
    speex_decoder_ctl(state, SPEEX_SET_ENH, &tmp);

    //outFile = argv[1];
    outFile = "speex.sample1.decoded.raw";
    fout = fopen(outFile, "w");

    /*Initialization of the structure that holds the bits*/
    speex_bits_init(&bits);
    while (1)
    {
        /*Read the size encoded by sampleenc, this part will likely be
        different in your application*/
        fread(&nbBytes, sizeof(int), 1, stdin);
        fprintf(stderr, "nbBytes: %d\n", nbBytes);
        if (feof(stdin))
            break;

        /*Read the "packet" encoded by sampleenc*/
        fread(cbits, 1, nbBytes, stdin);
        /*Copy the data into the bit-stream struct*/
        speex_bits_read_from(&bits, cbits, nbBytes);

        /*Decode the data*/
        speex_decode(state, &bits, output);

        /*Copy from float to short (16 bits) for output*/
        for (i = 0; i<FRAME_SIZE; i++)
            out[i] = output[i];

        /*Write the decoded audio to file*/
        fwrite(out, sizeof(short), FRAME_SIZE, fout);
    }

    /*Destroy the decoder state*/
    speex_decoder_destroy(state);
    /*Destroy the bit-stream truct*/
    speex_bits_destroy(&bits);
    fclose(fout);
    return 0;
}

int encode(char **argv)
{
    printf("ENCODING!!\n");

    char *inFile;
    FILE *fin;
    short in[FRAME_SIZE];
    float input[FRAME_SIZE];
    char cbits[200];
    int nbBytes;
    /*Holds the state of the encoder*/
    void *state;
    /*Holds bits so they can be read and written to by the Speex routines*/
    SpeexBits bits;
    int i, tmp;

    /*Create a new encoder state in narrowband mode*/
    state = speex_encoder_init(&speex_nb_mode);

    /*Set the quality to 8 (15 kbps)*/
    tmp = 8;
    speex_encoder_ctl(state, SPEEX_SET_QUALITY, &tmp);

    inFile = "gate10.decode.raw";//argv[1];
    fin = fopen(inFile, "r");

    /*Initialization of the structure that holds the bits*/
    speex_bits_init(&bits);
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
        speex_encode(state, input, &bits);
        /*Copy the bits to an array of char that can be written*/
        nbBytes = speex_bits_write(&bits, cbits, 200);

        /*Write the size of the frame first. This is what sampledec expects but
        it's likely to be different in your own application*/
        fwrite(&nbBytes, sizeof(int), 1, stdout);
        /*Write the compressed data*/
        fwrite(cbits, 1, nbBytes, stdout);

    }

    /*Destroy the encoder state*/
    speex_encoder_destroy(state);
    /*Destroy the bit-packing struct*/
    speex_bits_destroy(&bits);
    fclose(fin);
    return 0;
}