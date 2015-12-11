using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CallCopy.Media.Audio;

namespace speexcmd
{
    public class Speex 
    {
        /// <summary> 
        /// Initialize.
        /// </ Summary> 
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
        public bool Encode(string inFile, int qualityIn, string outFile)
        {
            return SpeexCommons.EncodeSpeex(inFile, qualityIn, outFile);
        }


        /// <summary> 
        /// Encoding audio data will be collected.
        /// </ Summary>         
        /// <param name = "inFile" >  input file </ param> 
        /// <param name = "outFile" >  output file </ param> 
        public bool EncodeFromFile(string inFile,  string outFile,int qualityIn)
        {
            return SpeexCommons.EncodeSpeexFromFile(inFile, outFile, qualityIn);
        }

        /// <summary> 
        /// Encoding audio data will be collected.
        /// </ Summary>         
        /// <param name = "inFile" >  input file </ param> 
        /// <param name = "outFile" >  output file </ param> 
        public bool EncodeFromBuffer(string outFile, int qualityIn, byte[] buffer, int buferSize)
        {
            return SpeexCommons.EncodeSpeexFromBuffer(outFile, qualityIn, buffer, buferSize);

        }

        /// <summary> 
        /// Decoding audio data will be collected.
        /// </ Summary>         
        /// <param name = "inFile" >  input file </ param> 
        /// <param name = "outFile" >  output file </ param> 
        public bool Decode(string inFile, string outFile)
        {
            return SpeexCommons.DecodeSpeex(inFile, outFile);
        }

    }
}

