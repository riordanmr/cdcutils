// MRRCDCUtils.h
#ifndef MRRDEFS_INCLUDED
#define MRRDEFS_INCLUDED
typedef __int64_t CDCWord;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define WORDS_IN_PRU  64
#define PC_BYTES_PER_64_BIT_WORD    8

#include "cset.h"

int GetBitsFromWord(CDCWord word60, int bit_start, int nbits);
int GetCDCChar(CDCWord word60, int idx);
char GetCDCCharAsASCII(CDCWord word60, int idx);
void GetCDCCharsAsASCII(CDCWord word60, int first, int nchars, char *szOut);
void WordTo6BitBytes(CDCWord word60, unsigned char *chars);
void DumpCDCWord(CDCWord word60);

#endif