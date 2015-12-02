using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Threading.Tasks;

namespace speexcmd
{
    class Program
    {
        static void Main(string[] args)
        {
            byte[] buffer = null;

            using (var fileStream = new FileStream("files/gate10.decode.raw", FileMode.Open, FileAccess.Read))
            {
                buffer = new byte[fileStream.Length];     
                int count;                            
                int offset = 0;                       

                // read until Read method returns 0 (end of the stream has been reached)  
                while ((count = fileStream.Read(buffer, offset, offset + 1024)) > 0)
                    offset += 1024;  // sum is a buffer offset for next reading
            }

            Speex speex = new Speex(10);
            byte[] encodedBuffer = speex.Encode(buffer);

            using (var fileStream = new FileStream("files/gate10.encoded.spx", FileMode.Create, FileAccess.Write))
            {
                fileStream.Write(encodedBuffer, 0, encodedBuffer.Length);
            }

            Console.WriteLine("Finished!");
            Console.ReadKey();
        }
    }
}
