using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using CallCopy.Media.Audio;
using System.IO;

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
        /// <param name = "inFileName" >  input file </ param> 
        /// <param name = "outFileName" >  output file </ param> 
        public bool Decode(string inFileName, string outFileName)
        {
            IntPtr pointerToBytes;
            int size;
            bool isSuccess = SpeexCommons.DecodeSpeex(inFileName, out pointerToBytes, out size);

            var destination = new byte[size];
            Marshal.Copy(pointerToBytes, destination, 0, size);

            using (var fileStream = new FileStream(outFileName, FileMode.Create, FileAccess.Write))
            {
                fileStream.Write(destination, 0, destination.Length);
            }

            return isSuccess;
        }

    }
}

