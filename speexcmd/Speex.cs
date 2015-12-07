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
            int response = Speex.EncodeSpeex(inFile, outFile);
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
            return Speex.DecodeSpeex(inFile, outFile);
        }


        #region Marshalling

            [DllImport("cc_codecs32_speex.dll")]
        public extern static int EncodeSpeex(string inFile, string outFile);

            [DllImport("cc_codecs32_speex.dll")]
            public extern static bool DecodeSpeex(string inFile, string outFile);
        #endregion
    }
}

