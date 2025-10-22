// MRRCDCUtils.cpp - Utilities for manipulating CDC files.
// Mark Riordan  2 August 2003

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wformat-security"
#endif

#include <stdio.h>
#include <ctype.h>
#include <inttypes.h> // LLVM requires this for PRIo64
#include "MRRCDCUtils.h"

/*---  function GetBitsFromWord  -------------------------------
 *  Extract bits from a word.
 *  Entry:  word60      is a 60-bit word.
 *          bit_start   is the bit number to start extracting.
 *                      0 = least significant bit.
 *          nbits       is the # of bits to extract.
 */
int GetBitsFromWord(CDCWord word60, int bit_start, int nbits) {
   static int Masks[] = {
      0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383,
         32767, 65535, 131071, 262143, 524287, 1048575, 2097151,
         4194303, 8388607, 16777215};
   int result = (int) ((word60>>(bit_start+1-nbits)) & Masks[nbits]);
   return result;
}

/*---  function GetCDCChar  -------------------------------
 *  Extract a 6-bit character from a word.
 *
 *  Entry:  word60      is a 60-bit word.
 *          idx         is a char index; 0=leftmost char.
 *  Exit:   Returns the 6-bit char.
 */
int GetCDCChar(CDCWord word60, int idx) {
   return (int) (0x3f & (word60 >> (54-(6*idx))));
}

/*---  function GetCDCCharAsASCII  -------------------------
 *  Extract a Display Code character from a word, and 
 *  convert it to ASCII.
 *
 *  Entry:  word60   is a 60-bit word.
 *          idx      is a character index; 0=leftmost char;
 *
 *  Exit:   Returns the ASCII value of that character.
 */
char GetCDCCharAsASCII(CDCWord word60, int idx) {
   return cdc2asc_nocolon_table[GetCDCChar(word60, idx)];
}

/*---  function GetCDCCharsAsASCII  ---------------------------
 *  Extract Display Code chars from a word and return as ASCII.
 *
 *  Entry:  word60   is a 60-bit word.
 *          first    is the index of the first char to extract; 0=first.
 *          nchars   is the # of chars to extract.
 *
 *  Exit:   szOut    contains nchars ASCII chars, terminated by 0.
 */
void GetCDCCharsAsASCII(CDCWord word60, int first, int nchars, char *szOut) {
   int iout=0;
   for(int ich=first; ich<first+nchars; ich++) {
      szOut[iout++] = GetCDCCharAsASCII(word60, ich);
   }
   szOut[iout] = '\0';
}

/*---  function WordTo6BitBytes  -------------------------
 *  Break down a 60-bit word into 10 6-bit characters.
 */
void WordTo6BitBytes(CDCWord word60, unsigned char *chars)
{
   for(int j=0; j<10; j++) {
      chars[j] = (unsigned char) ((word60 >> (54-6*j)) & 0x3f);
   }
}

/*---  function DumpCDCWord  ----------------------------
 *  Display a single 60-bit word in octal and display code (as ASCII).
 *
 *  Entry:  word60   is a 60-bit word.
 */
void DumpCDCWord(CDCWord word60, FormatType format)
{
   char line[80];
   char *pch;
   pch = line + sprintf(line, "%-20.20" PRIo64 " ", word60);
   if(FORMAT_DISPLAYCODE==format) {
      unsigned char cdcchars[10];
      WordTo6BitBytes(word60, cdcchars);
      for(int j=0; j<10; j++) {
         *(pch++) = cdc2asc_nocolon_table[cdcchars[j]];
      }
   } else if(FORMAT_ASCII==format) {
      for(int j=59; j>0; j-=12) {
         int ch = GetBitsFromWord(word60, j, 12);
         if(!isprint(ch)) {
            *(pch++) = '.';
         } else {
            *(pch++) = (char) ch;
         } 
      }
   }
   *pch = '\0';

   puts(line);
}
