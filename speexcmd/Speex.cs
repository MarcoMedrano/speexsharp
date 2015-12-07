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
        /// <param name = "inFile" >  input file </ param> 
        /// <param name = "outFile" >  output file </ param> 
        public bool Encode(string inFile, string outFile)
        {
            bool convert = false;
            int response = Speex.encoder_encode(inFile, outFile);
            convert = response == 0 ? true : false;
            
            return convert;
        }

        /// <summary> 
        /// Decoding audio data will be collected.
        /// </ Summary>         
        /// <param name = "inFile" >  input file </ param> 
        /// <param name = "outFile" >  output file </ param> 
        public bool Decode(string inFile, string outFile)
        {
            return Speex.decoder_decode(inFile, outFile);
        }
           

        #region Pinvoke 

        [DllImport("cc_codecs32_speex.dll", EntryPoint = "test_plus_plus")]
        internal extern static int TestPlusPlus(int number);

        [DllImport("cc_codecs32_speex.dll", EntryPoint = "encoder_encode")]
        internal extern static int encoder_encode(string inFile, string outFile);

        [DllImport("cc_codecs32_speex.dll", EntryPoint = "decoder_decode")]
        internal extern static bool decoder_decode(string inFile, string outFile);
        #endregion
    }
}

