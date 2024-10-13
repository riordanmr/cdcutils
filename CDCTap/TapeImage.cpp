// TapeImage.cpp - Class to represent a ".tap" file, a file created
// from a nine-track tape.
// Mark Riordan   20 July 2003

#include "TapeImage.h"

int CTapeImage::OpenTape(const char *pszFilename)
{
   int retcode=0;
   m_fileIn = fopen(pszFilename, "rb");
   if(!m_fileIn) {
      retcode = errno;
   }
   return retcode;
}

/*---  function ReadBlock  -------------------------------
 *  Exit:   Returns 0 if OK, -1 if EOF, else positive error number.
 */
int CTapeImage::ReadBlock(unsigned char *rawbuf, int maxlen, int &nBytesRead)
{
   int retcode=666, bytes_read, reclen, reclen2;
   nBytesRead = 0;
   do {
      // Read a record header, containing the number of bytes m_fileIn the header.
      bytes_read = fread(&reclen, 1, 4, m_fileIn);
      if(0 == bytes_read) {
         // End-of-file.
         retcode = -1;
         break;
      }
      if(reclen > maxlen) {
         // Error--insufficient input buffer size.
         retcode = 5;
         break;
      }
      if(reclen) {
         nBytesRead = fread(rawbuf, 1, reclen, m_fileIn);
         if(0 == nBytesRead) {
            // Error--EOF during data read.
            retcode = 2;
            break;
         }
         bytes_read = fread(&reclen2, 1, 4, m_fileIn);
         if(0 == bytes_read) {
            // Error: EOF during trailer read.
            retcode = 3;
            break;
         }
         if(reclen != reclen2) {
            // Header byte count does not match trailer byte count.
            // I used to always treat this as an error, but I saw on
            // UPMRR1 read by Jeff Woolsey that the byte count was low
            // by 1.
            int reclen3=0;
            fread(&reclen3, 1, 1, m_fileIn);
            reclen3 = (reclen2 >> 8) | (reclen3<<24);
            if(reclen != reclen3) {
               printf("*** Error: header len=%d; trailer byte len=%d\n", reclen, reclen2);
               retcode = 4;
               break;
            }
         }
      }
      retcode =0;
   } while(false);
   return retcode;
}