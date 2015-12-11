using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace CallCopy.Media.Audio
{
    class SpeexCommons
    {
        #region Marshalling
        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool EncodeSpeex(string inFile, int qualityIn, string outFile);

        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool EncodeSpeexFromBuffer(string outFile, int qualityIn, byte[] buffer, int buferSize);

        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool EncodeSpeexFromFile(string inFile, string outFile, int qualityIn);

        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool DecodeSpeex(string inFile, string outFile);       
        #endregion Marshalling

    }
}
