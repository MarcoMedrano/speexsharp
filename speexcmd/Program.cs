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
            Encode();
            Decode();

            Console.WriteLine("Finished!");
            Console.ReadKey();
        }

        private static void Encode()
        {
            Speex speex = new Speex();
            bool response = speex.Encode("gate10.decode.raw", 5, "agmu1.spx");
            Console.WriteLine("Encoded {0}", response ? "Success" : "Failed");
        }

        private static void Decode()
        {
            Speex speex = new Speex();
            bool response = speex.Decode("agmu1.spx", "amug1.raw");
            Console.WriteLine("Decoded {0}", response ? "Success" : "Failed");

        }
    }
}
