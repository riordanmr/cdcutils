// CDCFileDump.cpp - Simple program to create an octal
// dump of the PC version of a CDC file.
// In the input file, each 60 bits is represented by a 
// 64-bit word, in local machine byte order.
// Mark Riordan   29 July 2003

#include <stdio.h>
#define MAIN
#include "../cset.h"
#include "../MRRCDCUtils.h"

int main(int argc, char *argv[])
{
   int retcode = 1;
   do {
      if(2!=argc) {
         printf("CDCFileDump - Dump a PC file containing 60-bit CDC words.\n");
         printf("Usage:  CDCFileDump filename\n");
         retcode = 1;
         break;
      }

      FILE *fileIn = fopen(argv[1], "rb");
      if(!fileIn) {
         printf("Cannot open input file.\n");
         retcode = 2;
         break;
      }

      do {
         __int64 word60;
         unsigned char cdcchars[10];
         int nitems = fread(&word60, 8, 1, fileIn);
         if(0==nitems) break;
         DumpCDCWord(word60);
      } while(true);
   } while(false);
   return retcode;
}