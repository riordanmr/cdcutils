// TapeImage.h - Class to represent a ".tap" file.
#ifndef TAPEIMAGE_INCLUDED
#define TAPEIMAGE_INCLUDED

#include <stdio.h>
#include <errno.h>

class CTapeImage {
protected:
   FILE *m_fileIn;
   int   m_nBlockNumber;   // starts at 1.
public:
   int OpenTape(const char *pszFilename);
   int ReadBlock(unsigned char *rawbuf, int maxlen, int &nBytesRead);  // Returns 0 if all OK.
   int CloseTape();
};

#endif