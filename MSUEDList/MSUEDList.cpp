// MSUEDList.cpp - Program to list the contents of an MSU EDITOR workfile.
// The input file is assumed to be a binary copy of the original 
// SCOPE/Hustler EWFILE, except that each 60-bit word is stored
// in a 64-bit word in Intel format (least significant byte first).
//
// Mark Riordan  2 August 2003

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wformat-security"
#endif

#include <stdio.h>
//#include <io.h>
#include <string.h>
#include <stdlib.h>
#include "../MRRCDCUtils.h"

#define MAIN
#include "../cset.h"

int Debug=0;
FILE *fileIn=NULL;

// ShiftTables is used to convert the 6-bit characters in text blocks to ASCII.
// It is a table of tables.  At the first level, it is 
// indexed by enum_case (upper, lower, control case).
// Each one of these tables is then indexed by 6-bit "Old Mistic" characters 
// found in EWFILEs.
// ShiftTables was mechnically generated from EDITOR COMPASS source code.

struct struct_shift_table {
   unsigned char to_ascii[64];
} ShiftTables[3] = {
   {0x00,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2B,0x2D,0x2A,0x2F,0x28,0x29,0x24,0x3D,0x20,0x2C,0x2E,0x23,0x5B,0x5D,0x3A,0x22,0x5F,0x21,0x26,0x27,0x3F,0x3C,0x3E,0x40,0x5C,0x5E,0x3B,},
   {0x00,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2B,0x2D,0x2A,0x2F,0x28,0x29,0x24,0x3D,0x20,0x2C,0x2E,0x27,0x3F,0x3C,0x3E,0x40,0x5C,0x5E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,},
   {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x25,0x60,0x7B,0x7C,0x7D,0x7E,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,},
};

#pragma pack(push, 1)

typedef struct struct_index_entry {
#if 0
   unsigned char  unused:4;
   unsigned char  format:3;
   unsigned char  unused2:7;
   unsigned short nlines:10;
   unsigned int   linenum_first_int:20;
   unsigned int   linenum_first_frac:20;  // end of first word
   unsigned char  unused3:4;
   unsigned int   pruord:20;
   unsigned int linenum_last_int:20;
   unsigned int linenum_last_frac:20;
#else
   CDCWord  words[2];
#endif
} TypIndexEntry;
#pragma pack(pop)

typedef enum enum_seq_type {
   SEQTYPE_NONE, SEQTYPE_NORMAL, SEQTYPE_FULL}
TypSeqType;

class CIndexEntry {
protected:
   CDCWord  *m_pEnt;
public:
   void LoadPtr(CDCWord *pEnt) {
      m_pEnt = pEnt;
   }
   int GetFormat() {
      return GetBitsFromWord(m_pEnt[0], 59, 3);
   }
   int GetNLines() {
      return GetBitsFromWord(m_pEnt[0], 49, 10);
   }
   int GetPRUOrd() {
      return GetBitsFromWord(m_pEnt[1], 59, 20);
   }
   int GetLastLineInt() {
      return GetBitsFromWord(m_pEnt[1], 39, 20);
   }
   void DumpIndexEntry() {
      printf("  format=%d  nlines=%d  pru=%d  *L=%d\n",
         GetFormat(), GetNLines(), GetPRUOrd(), GetLastLineInt());
   }
   static void DumpEntireIndex(TypIndexEntry *pIndex, int beg, const char *szmsg) {
      printf("%s\n", szmsg);
      int j;
      for(j=beg; j<95; j++) {
         CIndexEntry Entry;
         Entry.LoadPtr(&pIndex[j].words[0]);
         if(0==Entry.GetLastLineInt()) break;
         Entry.DumpIndexEntry();
      }
   }
};

class CLineHeader {
protected:
   CDCWord m_headerword;
public:
   CLineHeader(CDCWord word60) {
      m_headerword = word60;
   }
   BOOL GetIsFormatDummy() {
      return GetBitsFromWord(m_headerword, 57, 1);
   }
   BOOL GetIsString() {
      return GetBitsFromWord(m_headerword, 53, 1);
   }
   // GetNWords returns total # of words in line, including header word.
   int GetNWords() {
      return GetBitsFromWord(m_headerword, 45, 6);
   }
   int GetLineNumInt() {
      return GetBitsFromWord(m_headerword, 39, 20);
   }
   int GetLineNumFrac() {
      return GetBitsFromWord(m_headerword, 19, 20);
   }
};

class CEWFILE {
protected:
   FILE *m_fileIn;
   CDCWord m_Variables[WORDS_IN_PRU];
   TypIndexEntry m_MasterIndex[WORDS_IN_PRU/2];
   TypIndexEntry m_CurSubIndex[96];
   TypSeqType    m_seqtype;
#define PRUS_IN_TEXTBLOCK  16
#define WORDS_IN_TEXTBLOCK PRUS_IN_TEXTBLOCK*WORDS_IN_PRU
   CDCWord m_CurTextBlock[WORDS_IN_TEXTBLOCK];
public:
   CEWFILE() {
      m_fileIn = NULL;
   }

   BOOL Open(const char *szInFile) {
      BOOL bOK=FALSE;
      do {
         m_fileIn=fopen(szInFile, "rb");
         if(!m_fileIn) {
            break;
         }
         int bytes_read;
         char szFirstWord[16];
         bytes_read = fread(m_Variables, 1, WORDS_IN_PRU*PC_BYTES_PER_64_BIT_WORD, m_fileIn);
         GetCDCCharsAsASCII(m_Variables[0], 0, 10, szFirstWord);
         if(0!=strcmp(szFirstWord, "MSUEDITOR2")) {
            printf("First word is not MSUEDITOR2\n");
            break;
         }
         bytes_read = fread(m_MasterIndex, 1, WORDS_IN_PRU*PC_BYTES_PER_64_BIT_WORD, m_fileIn);
         if(WORDS_IN_PRU*PC_BYTES_PER_64_BIT_WORD == bytes_read) bOK = TRUE;
      } while(false);
      return bOK;
   }

   void Close() {
      if(m_fileIn) fclose(m_fileIn);
   }

   void SetSeqType(TypSeqType seqtype) {
      m_seqtype = seqtype;
   }

   /*---  function LoadSubIndex  -------------------------
    *  Load a subindex into m_CurSubindex.
    *
    *  Entry:  pruord   is the PRU ordinal (1=first PRU of file) to load from.
    *                   The smallest legal value is 3.
    */
   BOOL LoadSubIndex(int pruord) {
      BOOL bOK=TRUE;
      long byte_offset = (pruord-1) * WORDS_IN_PRU * PC_BYTES_PER_64_BIT_WORD;
      fseek(m_fileIn, byte_offset, SEEK_SET);
      fread(m_CurSubIndex, 1, 3 * WORDS_IN_PRU * PC_BYTES_PER_64_BIT_WORD, m_fileIn);
      char szmsg[128];
      if(Debug>0) {
         sprintf(szmsg, "Dumping subindex at PRU %d", pruord);
         CIndexEntry::DumpEntireIndex(m_CurSubIndex, 1, szmsg);
      }
      return bOK;
   }

   /*---  function LoadTextBlock  ---------------------------
    *  Read a textblock into the textblock buffer.
    *  Entry:  pruord   is the file PRU ordinal (1=first).
    *
    *  Exit:   m_CurTextBlock    is the textblock.
    */
   BOOL LoadTextBlock(int pruord) {
      BOOL bOK=TRUE;
      long byte_offset = (pruord-1) * WORDS_IN_PRU * PC_BYTES_PER_64_BIT_WORD;
      int textblock_num = pruord / PRUS_IN_TEXTBLOCK;
      byte_offset -= 2*textblock_num*PC_BYTES_PER_64_BIT_WORD;
      fseek(m_fileIn, byte_offset, SEEK_SET);
      fread(m_CurTextBlock, 1, WORDS_IN_TEXTBLOCK * PC_BYTES_PER_64_BIT_WORD, m_fileIn);
      if(Debug>2) {
         printf("Dumping textblock at PRU %d\n", pruord);
         for(int iw=0; true; iw++) {
            DumpCDCWord(m_CurTextBlock[iw]);
            if(iw>1 && 0==m_CurTextBlock[iw-1] && 0==m_CurTextBlock[iw]) break;
         }
      }
      return bOK;
   }

   /*---  function FormatLineNum  --------------------------
    */
   int FormatLineNum(int intpart, int fracpart, char *pszOut) {
      int nchars=0;
      switch(m_seqtype) {
      case SEQTYPE_NONE:
         break;
      case SEQTYPE_NORMAL:
         nchars = sprintf(pszOut, "%d", intpart);
         if(fracpart) {
            pszOut[nchars++] = '.';
            nchars += sprintf(pszOut+nchars, "%-6.6d", fracpart);
            while('0' == pszOut[nchars-1]) {
               nchars--;
            }
         }
         break;
      case SEQTYPE_FULL:
         nchars = sprintf(pszOut, "%-6.6d.%-6.6d", intpart, fracpart);
         break;
      }
      pszOut[nchars] = '\0';
      return nchars;
   }

   /*---  function ListLine  ---------------------------
    *  List a given line.
    *
    *  Entry:  iw       is an offset into m_CurTextBlock for the
    *                   first word (header) of the line.
    *
    *  Exit:   Returns the number of CPU words in the line, so
    *             we can figure out the address of the next line.
    */
   int ListLine(int iw) {
      char szLine[2000], *pszLine=szLine;
      CLineHeader header(m_CurTextBlock[iw]);
      int NWords = header.GetNWords();
      do {
         if(header.GetIsFormatDummy()) break;
         if(header.GetIsString()) break;
         int intpart=header.GetLineNumInt();
         if(13050==intpart) {
            int jj=0;
         }
         pszLine += FormatLineNum(header.GetLineNumInt(), header.GetLineNumFrac(), szLine);
         if(m_seqtype != SEQTYPE_NONE) *(pszLine++) = '=';
#define SHIFT_UPPERCASE 070
#define SHIFT_LOWERCASE 071
#define SHIFT_CTLCASE   072
         enum enum_case {CASE_UPPER, CASE_LOWER, CASE_CTL} curcase=CASE_UPPER;
         BOOL bCompressed=FALSE;
         for(int ilw=1; ilw<NWords; ilw++) {
            CDCWord word60 = m_CurTextBlock[ilw+iw];
            for(int ic=0; ic<10; ic++) {
               unsigned char ch6 = GetCDCChar(word60, ic);
               if(bCompressed) {
                  while(ch6--) *(pszLine++) = ' ';
                  bCompressed = FALSE;
               } else if(0==ch6) {
                  // Two consecutive zeros mean end of line.
                  if(bCompressed) break;
                  bCompressed = TRUE;
               } else if(SHIFT_UPPERCASE==ch6) {
                  curcase = CASE_UPPER;
               } else if(SHIFT_LOWERCASE==ch6) {
                  curcase = CASE_LOWER;
               } else if(SHIFT_CTLCASE==ch6) {
                  curcase = CASE_CTL;
               } else {
                  char asciich = ShiftTables[curcase].to_ascii[ch6];
                  *(pszLine++) = asciich;
               }
            }
         }
         *pszLine = '\0';
         puts(szLine);
      } while(false);
      return NWords;
   }

   /*---  function ListLinesInBlock  ----------------------------
    *  List the lines in the current text block.
    *  I just skip the first two words, because I'm not sure what
    *  they mean.
    */
   void ListLinesInBlock() {
      int iw=2;
      int n_active_words = (int)m_CurTextBlock[0];
      do {
         if(!m_CurTextBlock[iw]) break;
         int offset = ListLine(iw);
         iw += offset;
         if(iw >= n_active_words) break;
      } while(true);
   }

   void List() {
      if(Debug>0) {
         CIndexEntry::DumpEntireIndex(m_MasterIndex, 0, "Dumping master index");
      }
      for(int imast=0; true; imast++) {
         CIndexEntry MasterEntry;
         MasterEntry.LoadPtr(&m_MasterIndex[imast].words[0]);
         if(0==MasterEntry.GetLastLineInt()) break;
         int subidx_pruord = MasterEntry.GetPRUOrd();
         LoadSubIndex(subidx_pruord);
         for(int isub=1; true; isub++) {
            CIndexEntry SubEntry;
            SubEntry.LoadPtr(&m_CurSubIndex[isub].words[0]);
            if(0==SubEntry.GetLastLineInt()) break;
            int text_pruord = SubEntry.GetPRUOrd();
            LoadTextBlock(text_pruord);
            ListLinesInBlock();
         }
      }
   }
};

int main(int argc, char* argv[])
{
   char *szInFile="EWFILE";
   BOOL bPrintUsage=FALSE;
   TypSeqType seqtype=SEQTYPE_NORMAL;
   int j;
   for(j=1; j<argc; j++) {
      if(0==strcmp(argv[j], "-help")) {
         bPrintUsage = TRUE;
      } else if(0==strcmp(argv[j], "-d")) {
         j++;
         Debug = atoi(argv[j]);
      } else if(0==strcmp(argv[j], "-s")) {
         j++;
         char *szSeqType = argv[j];
         if(0==strcmp("off", szSeqType)) {
            seqtype = SEQTYPE_NONE;
         } else if(0==strcmp("normal", szSeqType)) {
            seqtype = SEQTYPE_NORMAL;
         } else if(0==strcmp("full", szSeqType)) {
            seqtype = SEQTYPE_FULL;
         } else {
            bPrintUsage = TRUE;
         }
      } else if('-' == argv[j][0]) {
         bPrintUsage = TRUE;
      } else if('-' != argv[j][0]) {
         szInFile = argv[j];
      }
   }
   if(bPrintUsage) {
      printf("MSUEDList: Displays the contents of an MSU EDITOR workfile.\n");
      printf("Usage: MSUEDList [-d debug] [-s seqtype] [ewfile] \n");
      printf("where:\n");
      printf("debug     is the debug level, 0-3\n");
      printf("seqtype   selects the type of sequence numbers of beg of lines:\n");
      printf("          off      means none\n");
      printf("          normal   means no extra 0's; e.g. 123.05 (default).\n");
      printf("          full     means sequence numbers like 000123.050000.\n");
      printf("ewfile    is the name of an EDITOR workfile.  Each 60-bit CDC word\n");
      printf("          should be stored in 64-bit words, in Intel byte order.\n");
      printf("          Default: EWFILE (of course).\n");
      return 1;
   }

   CEWFILE ewfile;
   if(!ewfile.Open(szInFile)) {
      printf("Unable to open %s\n", szInFile);
      return 2;
   }
   ewfile.SetSeqType(seqtype);
   ewfile.List();
   ewfile.Close();

   //printf("Press Enter to exit: ");
   //getchar();

	return 0;
}
