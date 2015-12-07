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
        public extern static int EncodeSpeex(string inFile, int qualityIn, string outFile);

        [DllImport("cc_codecs32_speex.dll")]
        public extern static bool DecodeSpeex(string inFile, string outFile);       
        #endregion Marshalling

    }
}
