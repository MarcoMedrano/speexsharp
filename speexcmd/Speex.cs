using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace speexcmd
{
    public class Speex /*: IAudioCodec*/
    {
        private const int FrameSize = 160;

        private volatile bool isDisposed = false;
        public bool IsDisposed
        {
            get { return isDisposed; }
        }

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
            Speex.encoder_init(quality);
            Speex.decoder_init();
        }

        public void Dispose()
        {
            this.isDisposed = true;
            System.Threading.Thread.Sleep(100);
            Speex.decoder_dispose();
            Speex.encoder_dispose();
        }

        /// <summary> 
        /// Encoding audio data will be collected.
        /// </ Summary>         
        public byte[] Encode(byte[] data)
        {
            if (this.IsDisposed)
            {
                return null;
            }

            if (data.Length % (FrameSize * 2) != 0)
            {
                throw new ArgumentException("Invalid Data Length.");
            }

            int nbBytes;
            short[] input = new short[FrameSize];
            byte[] buffer = new byte[200];
            byte[] output = new byte[0];

            for (int i = 0; i < data.Length / (FrameSize * 2); i++)
            {
                for (int j = 0; j < input.Length; j++)
                {
                    input[j] = (short)(data[i * FrameSize * 2 + j * 2] + data[i * FrameSize * 2 + j * 2 + 1] * 0x100);
                }

                nbBytes = Speex.encoder_encode(input, buffer);
                Array.Resize<byte>(ref output, output.Length + nbBytes + sizeof(int));
                Array.Copy(buffer, 0, output, output.Length - nbBytes, nbBytes);

                for (int j = 0; j < sizeof(int); j ++)
                {
                    output[output.Length - nbBytes - sizeof(int) + j] = (byte)(nbBytes % 0x100);
                    nbBytes /= 0x100;
                }
            }
            return output;
        }

        /// <summary> 
        /// The encoded data is decoded to obtain the original audio data.
        /// </ Summary>        
        public byte[] Decode(byte[] data)
        {
            if (this.isDisposed)
            {
                return null;
            }

            int nbBytes, index = 0;
            byte[] input;
            short[] buffer = new short[FrameSize];
            byte[] output = new byte[0];
            while (index < data.Length)
            {
                nbBytes = 0;
                index += sizeof(int);
                for (int i = 1; i <= sizeof(int); i++)
                    nbBytes = nbBytes * 0x100 + data[index - i];
                input = new byte[nbBytes];
                Array.Copy(data, index, input, 0, input.Length);
                index += input.Length;
                Speex.decoder_decode(nbBytes, input, buffer);
                Array.Resize<byte>(ref output, output.Length + FrameSize * 2);

                for (int i = 0; i < FrameSize; i++)
                {
                    output[output.Length - FrameSize * 2 + i * 2] = (byte)(buffer[i] % 0x100);
                    output[output.Length - FrameSize * 2 + i * 2 + 1] = (byte)(buffer[i] / 0x100);
                }
            }
            return output;
        }

        #region Pinvoke 

        [DllImport("SpeexWrapper.dll", EntryPoint = "test_plus_plus")]
        internal extern static int TestPlusPlus(int number);

        [DllImport("SpeexWrapper.dll", EntryPoint = "encoder_init")]
        internal extern static void encoder_init(int quality);

        [DllImport("SpeexWrapper.dll", EntryPoint = "encoder_dispose")]
        internal extern static void encoder_dispose();

        [DllImport("SpeexWrapper.dll", EntryPoint = "encoder_encode")]
        internal extern static int encoder_encode(short[] data, byte[] output);

        [DllImport("SpeexWrapper.dll", EntryPoint = "decoder_init")]
        internal extern static void decoder_init();

        [DllImport("SpeexWrapper.dll", EntryPoint = "decoder_dispose")]
        internal extern static void decoder_dispose();

        [DllImport("SpeexWrapper.dll", EntryPoint = "decoder_decode")]
        internal extern static void decoder_decode(int nbBytes, byte[] data, short[] output);
        #endregion
    }
}

