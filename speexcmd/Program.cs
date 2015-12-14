using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Threading.Tasks;

namespace speexcmd
{
    //testing credentials master branch
    class Program
    {
        static void Main(string[] args)
        {
            //EncodeFromFile();
            Decode();

            Console.WriteLine("Finished!");
            Console.ReadKey();
        }

        private static void Encode()
        {
            Speex speex = new Speex();
            bool response = speex.Encode("gate10.decode.raw", 10, "agmu1.spx");
            Console.WriteLine("Encoded {0}", response ? "Success" : "Failed");
        }

        private static void EncodeFromFile()
        {
            Speex speex = new Speex();
            bool response = speex.EncodeFromFile("gate10.decode.raw", "agmu1.spx", 5 );
            Console.WriteLine("Encoded {0}", response ? "Success" : "Failed");
        }

        private static void EncodeFromBuffer()
        {
            byte[] tempBuffer = new byte[3];
            Speex speex = new Speex();
            bool response = speex.EncodeFromBuffer("agmu1.spx", 5, tempBuffer, 0);
            Console.WriteLine("Encoded {0}", response ? "Success" : "Failed");
        }

        private static void Decode()
        {
            Speex speex = new Speex();
            bool isSuccess = speex.Decode("agmu1.spx", "amug1.raw");
            Console.WriteLine("Decoded {0}", isSuccess ? "Success" : "Failed");
        }
    }
}
