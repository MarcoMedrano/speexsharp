using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace speexcmd
{
    public class Speex 
    {
        private const int FrameSize = 160;

        /// <summary> 
        /// Initialize.
        /// </ Summary> 
        /// <param name = "quality" > encoding quality, value 0 ~ 10 </ param> 
        public Speex(int quality)
        {
            if (quality < 0 || quality > 10)
            {
                throw new Exception("quality value must be between 0 and 10.");
            }

            int var  = Speex.TestPlusPlus(1);
            //Speex.encoder_init(quality);
            //Speex.decoder_init();
        }

        public void Dispose()
        {

        }

        /// <summary> 
        /// Encoding audio data will be collected.
        /// </ Summary>         
        public bool Encode(string inFile, string outFile)
        {
            bool convert = false;
            int response = Speex.encoder_encode(inFile, outFile);
            convert = response == 0 ? true : false;
            
            return convert;
        }

        public bool Decode(string inFile, string outFile)
        {
            return Speex.decoder_decode(inFile, outFile);
        }

        /// <summary> 
        /// The encoded data is decoded to obtain the original audio data.
        /// </ Summary>        
        public byte[] Decode(byte[] data)
        {
            //if (this.isDisposed)
            //{
            //    return null;
            //}

            //int nbBytes, index = 0;
            //byte[] input;
            //short[] buffer = new short[FrameSize];
            byte[] output = new byte[0];
            //while (index < data.Length)
            //{
            //    nbBytes = 0;
            //    index += sizeof(int);
            //    for (int i = 1; i <= sizeof(int); i++)
            //        nbBytes = nbBytes * 0x100 + data[index - i];
            //    input = new byte[nbBytes];
            //    Array.Copy(data, index, input, 0, input.Length);
            //    index += input.Length;
            //    Speex.decoder_decode(nbBytes, input, buffer);
            //    Array.Resize<byte>(ref output, output.Length + FrameSize * 2);

            //    for (int i = 0; i < FrameSize; i++)
            //    {
            //        output[output.Length - FrameSize * 2 + i * 2] = (byte)(buffer[i] % 0x100);
            //        output[output.Length - FrameSize * 2 + i * 2 + 1] = (byte)(buffer[i] / 0x100);
            //    }
            //}
            return output;
        }

        #region Pinvoke 

        [DllImport("SpeexWrapper.dll", EntryPoint = "test_plus_plus")]
        internal extern static int TestPlusPlus(int number);
        
        [DllImport("SpeexWrapper.dll", EntryPoint = "encoder_encode")]
        internal extern static int encoder_encode(string inFile, string outFile);

        [DllImport("SpeexWrapper.dll", EntryPoint = "decoder_decode")]
        internal extern static bool decoder_decode(string inFile, string outFile);
        #endregion
    }
}

