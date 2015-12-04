using NSpeex;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nspeexcmd
{
    class Program
    {
        static void Main(string[] args)
        {
            Decode();
        }

        private static void Decode()
        {
            byte[] encodedData = File.ReadAllBytes("speex.sample1.encoded.spx");
            SpeexDecoder decoder = new SpeexDecoder(BandMode.Wide);

            short[] decodedFrame = new short[1024]; // should be the same number of samples as on the capturing side
            decoder.Decode(encodedData, 0, encodedData.Length, decodedFrame, 0, false);

            // todo: do something with the decoded data
        }
    }
}
