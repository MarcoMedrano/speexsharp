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
            bool convert = false;
            int response = SpeexCommons.EncodeSpeex(inFile, qualityIn, outFile);
            convert = response == 0 ? true : false;
            
            return convert;
        }

        /// <summary> 
        /// Decoding audio data will be collected.
        /// </ Summary>         
        /// <param name = "inFile" >  input file </ param> 
        /// <param name = "outFile" >  output file </ param> 
        public IntPtr Decode(string inFile, string outFile)
        {
            IntPtr pointerToBytes;
            int size;
            SpeexCommons.DecodeSpeex(inFile, out pointerToBytes, out size);

            var destination = new byte[size];
            Marshal.Copy(pointerToBytes, destination, 0, size);

            using (var fileStream = new FileStream("test2.raw", FileMode.Create, FileAccess.Write))
            {
                fileStream.Write(destination, 0, destination.Length);
            }

            return pointerToBytes;
        }

    }
}

