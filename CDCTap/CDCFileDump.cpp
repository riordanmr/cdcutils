// CDCFileDump.cpp - Simple program to create an octal
// dump of the PC version of a CDC file.
// In the input file, each 60 bits is represented by a 
// 64-bit word, in local machine byte order.
// Mark Riordan   29 July 2003

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAIN
#include "../cset.h"
#include "../MRRCDCUtils.h"

void usage()
{
   printf("CDCFileDump - Dump a PC file containing 60-bit CDC words.\n");
   printf("Usage:  CDCFileDump [-f a|d] filename\n");
   printf("where:\n");
   printf("f    is the format: a=ASCII, d=display code (default=d)\n");
}

int main(int argc, char *argv[])
{
   int retcode = 1;
   do {
      char *szInFile = NULL;
      FormatType format = FORMAT_NONE;
      for(int j=1; j<argc; j++) {
         printf("arg %d: %s\n", j, argv[j]);
         if(0==strcmp("-f", argv[j])) {
            j++;
            if(0==strcmp("a", argv[j])) {
               format = FORMAT_ASCII;
            } else if(0==strcmp("d", argv[j])) {
               format = FORMAT_DISPLAYCODE;
            } else {
               usage();
               retcode = 3;
               break;
            }
         } else if('-' != argv[j][0]) {
            szInFile = argv[j];
         }
      }
      if(FORMAT_NONE==format || !szInFile) {
         usage();
         retcode = 1;
         break;
      }

      FILE *fileIn = fopen(szInFile, "rb");
      if(!fileIn) {
         printf("Cannot open input file.\n");
         retcode = 2;
         break;
      }

      do {
         __int64_t word60;
         unsigned char cdcchars[10];
         int nitems = fread(&word60, 8, 1, fileIn);
         if(0==nitems) break;
         DumpCDCWord(word60, format);
      } while(true);
   } while(false);
   return retcode;
}
