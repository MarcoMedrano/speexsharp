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
        public extern static bool EncodeSpeexFromBuffer(string outFile, int qualityIn, int bandMode, int channels, byte[] buffer, int buferSize, int pcmRate);

        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool EncodeSpeexFromFile(string inFile, string outFile, int qualityIn, int bandMode, int channels, int pcmRate);

        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool DecodeSpeex(string inFile, out IntPtr outBytes, out int size);       
        #endregion Marshalling

    }
}
