// CDCTap.cpp - Read a ".TAP" file containing the image of a 9-track tape,
// and display its contents as if the tape originated on a 60-bit CDC system.
// Mark Riordan   28 June 2003 
//
// Sample call:
// -a pfload \DtCyber\tapes\mrr_up1821_1600bpi.tap -m MRRSEGMENTATIONDIRECTIVES*

#include <stdio.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include "TapeImage.h"
#define MAIN
#include "MRRCDCUtils.h"

enum enum_action {ACTION_NONE, ACTION_DUMPRAW, ACTION_DUMPAF, ACTION_PFLIST, ACTION_PFLOAD}
      action = ACTION_NONE;

//typedef LONGLONG CDCWord;

// Symbols for PFDUMP tape format
#define F_CWEOI 3   // Signifies EOI when in bits 10-9 of data group header word.

/*
   * Written By Douglas A. Lewis <dalewis@cs.Buffalo.EDU>
   *
   * The match procedure is public domain code (from ircII's reg.c)
   *
   *  Entry:   mask  is a DOS-style wildcard mask.  If NULL,
   *                 the match will be true.
 */

BOOL WildcardMatch(const char *mask, const char *string)
{
   const char *m = mask, *n = string, *ma = NULL, *na = NULL, *mp = NULL,
    *np = NULL;
   int just = 0, pcount = 0, acount = 0, count = 0;

   if(!mask) return TRUE;

   for (;;) {
      if (*m == '*') {
         ma = ++m;
         na = n;
         just = 1;
         mp = NULL;
         acount = count;
      }
#if 0
      else if (*m == '%') {
         mp = ++m;
         np = n;
         pcount = count;
      }
#endif
      else if (*m == '?') {
         m++;
         if (!*n++)
            return FALSE;
      } else {
         if (*m == '\\') {
            m++;
            /* Quoting "nothing" is a bad thing */
            if (!*m)
               return FALSE;
         }
         if (!*m) {
            /*
               * If we are out of both strings or we just
               * saw a wildcard, then we can say we have a
               * match
             */
            if (!*n)
               return TRUE;
            if (just)
               return TRUE;
            just = 0;
            goto not_matched;
         }
         /*
            * We could check for *n == NULL at this point, but
            * since it's more common to have a character there,
            * check to see if they match first (m and n) and
            * then if they don't match, THEN we can check for
            * the NULL of n
          */
         just = 0;
         if (tolower(*m) == tolower(*n)) {
            m++;
            if (*n == ' ')
               mp = NULL;
            count++;
            n++;
         } else {

          not_matched:

            /*
               * If there are no more characters in the
               * string, but we still need to find another
               * character (*m != NULL), then it will be
               * impossible to match it
             */
            if (!*n)
               return FALSE;
            if (mp) {
               m = mp;
               if (*np == ' ') {
                  mp = NULL;
                  goto check_percent;
               }
               n = ++np;
               count = pcount;
            } else
             check_percent:

            if (ma) {
               m = ma;
               n = ++na;
               count = acount;
            } else
               return FALSE;
         }
      }
   }
}

int Trim(char *str)
{
   int len=strlen(str);
   while(len) {
      len--;
      if(' '==str[len]) {
         str[len] = '\0';
      } else {
         len++;
         break;
      }
   }
   return len;
}

unsigned char Get6Bits(unsigned char *buf, int ichar)
{
   unsigned char cur6bits;
   static unsigned char mask_of_size[7] = {
      0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f};
   int starting_bit = 6*ichar;
   int byte_idx = starting_bit / 8;
   int bits_left_in_byte = 8 - (starting_bit % 8);
   int bits_from_this_byte = bits_left_in_byte >= 6 ? 6 : bits_left_in_byte;

   if(bits_left_in_byte >= 6) {
      // We can get the entire 6 bits from the current byte.
      cur6bits = (buf[byte_idx] >> (bits_left_in_byte-6)) & 0x3f;
      bits_left_in_byte -= 6;
   } else {
      // Get the top part of the 6 bits from this byte.
      cur6bits = buf[byte_idx] & mask_of_size[bits_left_in_byte];
      cur6bits <<= 6-bits_left_in_byte;
      // Advance to the next byte.
      byte_idx++;
      // Now extract the rest of the 6 bits from this byte.
      bits_from_this_byte = 6-bits_from_this_byte;
      cur6bits |= (buf[byte_idx] >> (8-bits_from_this_byte)) & mask_of_size[bits_from_this_byte];
   }
   return cur6bits;
}

unsigned int Get12Bits(unsigned char *buf, int ichar)
{
   unsigned int first6 = Get6Bits(buf, 2*ichar);
   unsigned int second6 = Get6Bits(buf, 1 + 2*ichar);
   unsigned int both = (first6 << 6) | second6;
   return both;
}

void DisplayOneLineOfCDCChars(unsigned char *pcdcchars, int n_cdc_chars)
{
   int j;
   for(j=0; j<n_cdc_chars; j++) {
      printf("%-2.2o ", pcdcchars[j]);
   }
   putchar(' ');
   for(j=0; j<n_cdc_chars; j++) {
      putchar(cdc2asc_nocolon_table[pcdcchars[j]]);
   }
   putchar('\n');
}

void DisplayBuffer(int irec, unsigned char *buf, int bytes_in_buf)
{
   unsigned char cdcchars[10], cur6bits, n_cdc_chars=0;
   int byte_idx=0, bits_left_in_byte=8, bits_from_this_byte;
   static unsigned char mask_of_size[7] = {
      0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f};

   printf("Record %d contains %d 8-bit bytes (%d 60-bit words):\n", irec, bytes_in_buf,
      8*bytes_in_buf/60);
   while(1) {
      //printf("Top of loop: byte_idx=%d bits_left_in_byte=%d\n", byte_idx, bits_left_in_byte);
      cur6bits = 0;
      if(bits_left_in_byte <= 0) {
         byte_idx++;
         if(byte_idx >= bytes_in_buf) break;
         bits_left_in_byte = 8;
      }
      bits_from_this_byte = bits_left_in_byte >= 6 ? 6 : bits_left_in_byte;
      if(bits_left_in_byte >= 6) {
         // We can get the entire 6 bits from the current byte.
         cur6bits = (buf[byte_idx] >> (bits_left_in_byte-6)) & 0x3f;
         bits_left_in_byte -= 6;
      } else {
         // Get the top part of the 6 bits from this byte.
         cur6bits = buf[byte_idx] & mask_of_size[bits_left_in_byte];
         cur6bits <<= 6-bits_left_in_byte;
         // Advance to the next byte.
         byte_idx++;
         if(byte_idx >= bytes_in_buf) break;
         bits_left_in_byte = 8-(6-bits_from_this_byte);
         // Now extract the rest of the 6 bits from this byte.
         bits_from_this_byte = 6-bits_from_this_byte;
         cur6bits |= (buf[byte_idx] >> (8-bits_from_this_byte)) & mask_of_size[bits_from_this_byte];
      }
      cdcchars[n_cdc_chars++] = cur6bits;
      if(n_cdc_chars >= sizeof(cdcchars)) {
         DisplayOneLineOfCDCChars(cdcchars, n_cdc_chars);
         n_cdc_chars = 0;
      }
   }
   if(n_cdc_chars >= sizeof(cdcchars)) {
      DisplayOneLineOfCDCChars(cdcchars, n_cdc_chars);
   }
   printf("\n");
}

void DisplayBufferAF(int irec, unsigned char *buf, int bytes_in_buf)
{
   //printf("AF Record %d contains %d 8-bit bytes (%d 60-bit words):\n", irec, bytes_in_buf, 8*bytes_in_buf/60);
   int n12 = bytes_in_buf * 8 / 12;
   for(int iAF=0; iAF < n12; iAF++) {
      unsigned int AF = Get12Bits(buf, iAF);
      if(0 == AF && 0==(1 + iAF)%5) {
         putchar('\n');
      } else if(AF > 0) {
         putchar(AF);
      }
   }
}

/*---  function GetCDCCharsAsASCIIFromBuf  -------------------------------
 */
void GetCDCCharsAsASCIIFromBuf(unsigned char *buf, int idxStart, int nChars, char *szOut)
{
   int j;
   for(j=0; j<nChars; j++) {
      unsigned char cdcchar = Get6Bits(buf, idxStart+j);
      szOut[j] = cdc2asc_nocolon_table[cdcchar];
   }
   szOut[j] = '\0';
}

/*---  function ASCIIJulianToYMD  -------------------------------
 *  Convert a Julian date to binary year/month/day.
 *
 *  Entry:  szJulian is an ASCII Julian date, 5 chars of form YYDDD.
 *                   e.g. 81013.  (= 13 January 1981)
 *
 *  Exit:   year     is a year, e.g, 1981.  Julian dates from 
 *                   00xxx through 59xxx are considered to be year
 *                   2000 through 2059.
 *          month    is a month; 1=January
 *          day      is a day, 1=first day of month.
 */
void ASCIIJulianToYMD(const char *szJulian, int &year, int &month, int &day)
{
   year = 10*(szJulian[0]-'0') + szJulian[1]-'0';
   if(year < 60) {
      year += 2000;
   } else {
      year += 1900;
   }
   // Is this a leap year?  It is if it's divisible by 4.
   // The actual rules are more complicated, but they apply only to 
   // years < 1901 or > 2099, which is out of our range.
   BOOL bLeap=FALSE;
   if(0 == (year%4)) bLeap = TRUE;

   int jdate = 100*(szJulian[2]-'0') + 10*(szJulian[3]-'0') + szJulian[4]-'0';

   static int days_in_month[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
   int imon, days_in_cur_month;
   for(imon=0; imon<12; imon++) {
      days_in_cur_month = days_in_month[imon];
      if(1==imon && bLeap) days_in_cur_month++;
      jdate -= days_in_cur_month;
      if(jdate <= 0) {
         month = imon+1;
         day = jdate + days_in_cur_month;
         break;
      }      
   }
}

/*---  function ASCIIJulianToASCIIYMD  -----------------------------
 *  Convert an ASCII Julian date (e.g. 81013) to an ASCII format
 *  like 1981-01-13.
 */
void ASCIIJulianToASCIIYMD(const char *szJulian, char *szDate)
{
   int year, month, day;
   ASCIIJulianToYMD(szJulian, year, month, day);
   sprintf(szDate, "%-4.4d-%-2.2d-%-2.2d", year, month, day);
}

/*---  function DoPFDump  -------------------------------
 */
void DoPFDump(int irec, unsigned char *buf, int bytes_in_buf)
{
   char szASCII[128];
   do {
      unsigned char first = Get6Bits(buf, 0);
      if(024 != first) {
         static int bWarned = 0;
         if(!bWarned) {
            printf("Error: first char of tape rec %5d is %2.2o\n", irec, first);
            bWarned = 1;
         }
         //break;
      }
      if(0==Get6Bits(buf, 2) && 0==Get6Bits(buf, 3) && 0==Get6Bits(buf,4)) {
         // This is the first tape record for a given dumped file.
         GetCDCCharsAsASCIIFromBuf(buf, 14, 6, szASCII);
         if(0!=strcmp("DUMP T", szASCII)) {
            printf("Did not find expected PFDUMP header: %s\n", szASCII);
            break;
         }
         GetCDCCharsAsASCIIFromBuf(buf, 100, 40, szASCII);
         printf("Rec %5d: %s\n", irec, szASCII);
      }
   } while(0);
}


class CCDCWords {
public:
   CDCWord *m_pCDCWords;
   int      m_nwords;
   int      m_nAllocated;
public:
   CCDCWords() {
      m_nwords = 0;
      m_nAllocated = 2048;
      m_pCDCWords = new CDCWord[m_nAllocated];
   }
   ~CCDCWords() {
      delete [] m_pCDCWords;
   }
   int LoadFromRawBlock(unsigned char *rawbuf, int nBytes)
   {
      int retcode = 0;
      int iw;
      m_nwords = nBytes*8/60;
      if(m_nwords > m_nAllocated) {
         return 666;
      }
      for(iw=0; iw<m_nwords; iw++) {
         int ic6, start=10*iw;
         CDCWord word60=0;
         for(ic6=start; ic6<start+10; ic6++) {
            word60 = (word60<<6) | Get6Bits(rawbuf, ic6);
         }
         m_pCDCWords[iw] = word60;
      }
      return retcode;
   }


};

class CCDCTap {
public:
   CTapeImage m_TapeImage;
   unsigned char buf[4096];
public:
   int OpenTape(const char *szFilename) {
      return m_TapeImage.OpenTape(szFilename);
   }

   int RawDump(enum enum_action action, int recs_to_skip, int recs_to_read) {
      int retcode=0;
      int tot_recs = recs_to_skip + recs_to_read, irec, bytes_read;
      for(irec=0; irec<tot_recs; irec++) {
         if(0!=(retcode=m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read))) {
            break;
         }
         if(bytes_read) {
            if(irec >= recs_to_skip) {
               if(ACTION_DUMPRAW == action) {
                  DisplayBuffer(irec+1, buf, bytes_read);
               } else if(ACTION_DUMPAF == action) {
                  DisplayBufferAF(irec+1, buf, bytes_read);
               }
            }
         } else {
            // This was a zero-byte record, so there's no data to dump, and no trailer.
            printf("Record %d is empty\n", irec+1);
         }
      }
      return retcode;
   }
   
   BOOL SkipLabels(char *VRN, char *JulianDateOfCreation) {
      BOOL bOK=FALSE;
      VRN[0] = JulianDateOfCreation[0] = '\0';
      int bytes_read;
      do {
         if(0!=m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read)) {
            // Error reading tape.
            break;
         }
         memcpy(VRN, buf+4, 6);
         VRN[6] = '\0';
         // Skip tape mark;
         m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read);
         // Read HDR1 label, with dates.
         m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read);
         memcpy(JulianDateOfCreation, buf+42, 5);
         JulianDateOfCreation[5] = '\0';
         // Skip tape mark;
         m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read);
         bOK = TRUE;
      } while(false);
      return bOK;
   }

   void ExtractPFNFromFirstDataGroup(CCDCWords &CDCWords, char *PFN) {
      char *pszPFN = PFN;
      for(int iw=10; iw<14; iw++) {
         CDCWord word60 = CDCWords.m_pCDCWords[iw];
         for(int ic=0; ic<10; ic++) {
            *(pszPFN++) = GetCDCCharAsASCII(word60, ic);
         }
      }
      *pszPFN = '\0';
      Trim(PFN);
   }

   int PFList(const char *pszMask) {
      int retcode=0, bytes_read;
      char VRN[8], JulianDateOfCreation[8], date[16], PFN[41];
      char JulianPFDate[8], PFDate[16];
      CCDCWords CDCWords;
      CDCWord word60;
      int nDataGroup;
      do {
         if(!SkipLabels(VRN, JulianDateOfCreation)) break;
         ASCIIJulianToASCIIYMD(JulianDateOfCreation, date);
         printf("Dump of VRN %s, created %s\n", VRN, date);

         do {
            // Read first block of file.
            if(0!=(retcode=m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read))) {
               break;
            }
            if(CDCWords.LoadFromRawBlock(buf, bytes_read)) break;
            word60 = CDCWords.m_pCDCWords[0];
            nDataGroup = GetBitsFromWord(word60, 47, 18);
            if(0==nDataGroup) {
               ExtractPFNFromFirstDataGroup(CDCWords, PFN);
               word60 = CDCWords.m_pCDCWords[31];
               GetCDCCharsAsASCII(word60, 1, 5, JulianPFDate);
               ASCIIJulianToASCIIYMD(JulianPFDate, PFDate);
               if(WildcardMatch(pszMask, PFN)) {
                  printf("%-40s %s\n", PFN, PFDate);
               }
            }

         } while(true);
      } while(false);
      return retcode;
   }

   void ParseDataGroupHeader(CDCWord word60, int &nDataGroup,
      int &nWordsInGroup, int &EOFFlg) {
      nDataGroup = GetBitsFromWord(word60, 47, 18);
      nWordsInGroup = GetBitsFromWord(word60, 8, 9);
      EOFFlg = GetBitsFromWord(word60, 10, 2);
   }

   void MakePCFilename(const char *PFN, char *localfile) {
      strcpy(localfile, PFN);
      Trim(localfile);
      // Set leading spaces to _
      //for(int j=0; ' '==localfile[j]; j++) localfile[j] = "_";
   }

   int PFLoad(const char *pszMask) {
      int retcode=0, bytes_read;
      char VRN[8], JulianDateOfCreation[8], date[16], PFN[41];
      char localfile[256];
      CCDCWords CDCWords;
      CDCWord word60;
      int nDataGroup, nWordsInGroup, EOFFlg, offset;
      do {
         if(!SkipLabels(VRN, JulianDateOfCreation)) break;
         ASCIIJulianToASCIIYMD(JulianDateOfCreation, date);
         printf("Loading from VRN %s, created %s\n", VRN, date);

         do {
            // Read first block of file.
            if(0!=(retcode=m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read))) {
               break;
            }
            if(CDCWords.LoadFromRawBlock(buf, bytes_read)) break;
            offset=0;
            word60 = CDCWords.m_pCDCWords[offset];
            ParseDataGroupHeader(word60, nDataGroup, nWordsInGroup, EOFFlg);
            if(0==nDataGroup) {
               ExtractPFNFromFirstDataGroup(CDCWords, PFN);
               if(WildcardMatch(pszMask, PFN)) {
                  printf("Copying %s\n", PFN);
                  MakePCFilename(PFN, localfile);
                  FILE *fileOut=fopen(localfile, "wb");
                  // Copy data groups until we get an EOI.
                  do {
                     offset += 1 + nWordsInGroup;
                     if(offset >= CDCWords.m_nwords) {
                        //printf("Reading next tape block\n");
                        if(0!=(retcode=m_TapeImage.ReadBlock(buf, sizeof(buf), bytes_read))) {
                           break;
                        }
                        if(CDCWords.LoadFromRawBlock(buf, bytes_read)) break;
                        offset = 0;
                     }
                     word60 = CDCWords.m_pCDCWords[offset];
                     //printf("Scanning header word %I64o at offset %d\n", word60, offset);
                     ParseDataGroupHeader(word60, nDataGroup, nWordsInGroup, EOFFlg);
                     if(F_CWEOI == EOFFlg) {
                        printf("EOI flag encountered.\n");
                        break;
                     } else {
                        // Copy words from offset+1 through offset+1+nWordsInGroup
                     }
                     //printf("Copying %d words from data group %d\n", nWordsInGroup, nDataGroup);
                     fwrite(&CDCWords.m_pCDCWords[offset+1], 8*nWordsInGroup, 1, fileOut);
                   } while(true);
                  fclose(fileOut);
               }
            }

         } while(true);
      } while(false);
      return retcode;

      return retcode;
   }
};

void InternalTest()
{
   do {
      printf("Enter Julian date: ");
      char line[80], date[80];
      gets(line);
      ASCIIJulianToASCIIYMD(line, date);
      printf("%s is %s\n", line, date);
   } while(true);
}

int main(int argc, char* argv[])
{
   char *szInFile = NULL;
   int j, recs_to_skip=0, recs_to_read=999999999;
   int bPFDump=0, errcode;
   bool bCmdErr=false;
   char *pszAction=NULL, *pszMask=NULL;
   CCDCTap CDCTap;

   for(j=1; j<argc; j++) {
      if(0==strcmp(argv[j], "-a")) {
         j++;
         if(0==strcmp("dumpraw", argv[j])) {
            action = ACTION_DUMPRAW;
         } else if(0==strcmp("dumpaf", argv[j])) {
            action = ACTION_DUMPAF;
         } else if(0==strcmp("pflist", argv[j])) {
            action = ACTION_PFLIST;
         } else if(0==strcmp("pfload", argv[j])) {
            action = ACTION_PFLOAD;
         }
      } else if(0==strcmp(argv[j], "-s")) {
         j++;
         recs_to_skip = atoi(argv[j]);
      } else if(0==strcmp(argv[j], "-n")) {
         j++;
         recs_to_read = atoi(argv[j]);
      } else if(0==strcmp(argv[j], "-p")) {
         bPFDump = 1;
      } else if(0==strcmp(argv[j], "-m")) {
         j++;
         pszMask = argv[j];
      } else if('-' != argv[j][0]) {
         szInFile = argv[j];
      }
   }
   if(!szInFile || ACTION_NONE==action) {
      printf("CDCTap: Reads .tap format file and dumps it in 60-bit chunks.\n");
      printf("Usage: CDCTap -a action [-s nrecs_to_skip] [-n nrecs_to_dump] \n");
      printf("   [-m mask] tapfile\n");
      printf("where action is:\n");
      printf("  dumpraw  to display an uninterpreted dump of the .tap file.\n");
      printf("  dumpaf   to display the tape as ASCII Fancy.\n");
      printf("  pfload   to restore to PC disk the PFs matching pfmask,\n");
      printf("           treating the tape as an MSU PFDUMP tape.  Files are\n");
      printf("           restored to the current directory.  Spaces\n");
      printf("           are replaced with _.\n");
      printf("  pflist   to list the contents of the tape to stdout,\n");
      printf("           treating the tape as an MSU PFDUMP tape.\n");
      printf("mask   is a DOS-style filename wildcard.\n");
      return 1;
   }

   if(0!=(errcode=CDCTap.OpenTape(szInFile))) {
      printf("Cannot open %s: error %d\n", szInFile, errcode);
      return 2;
   }

   if(ACTION_DUMPRAW == action) {
      CDCTap.RawDump(action, recs_to_skip, recs_to_read);
   } else if(ACTION_DUMPAF == action) {
      CDCTap.RawDump(action, recs_to_skip, recs_to_read);
   } else if(ACTION_PFLIST == action) {
      CDCTap.PFList(pszMask);
   } else if(ACTION_PFLOAD == action) {
      CDCTap.PFLoad(pszMask);
   }

   //printf("Press Enter to exit: ");
   //getchar();

	return 0;
}

