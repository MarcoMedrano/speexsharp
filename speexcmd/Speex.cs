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
        /// <summary> 
        /// Initialize.
        /// </ Summary> 
        /// <param name = "quality" > encoding quality, value 0 ~ 10 </ param> 
        public Speex()
        {
            int var  = Speex.TestPlusPlus(1);
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

