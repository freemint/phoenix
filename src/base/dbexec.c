/*****************************************************************************
 *
 * Module : DBEXEC.C
 * Author : Purex & JÅrgen Geiû
 *
 * Creation date    : 01.04.91
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database execute routines.
 *
 * History:
 * 25.04.96: Warning removed
 * 27.12.95: ReadCard not available for WIN32
 * 25.10.95: MAX_STR & SLOT_SIZE increased
 * 04.07.95: O_RETURN added
 * 02.11.94: O_KBVGET added
 * 18.10.94: O_KBVREAD added
 * 25.06.94: Special handling of Metafiles added in DoBlobimport
 * 24.06.94: Error in ParseParam fixed with systables (they don't have an index name)
 * 15.06.94: Functions DoBlobImport/Export completet for all blob data types
 * 07.06.94: O_IMPORT, O_EXPORT added in Interpret, error fixed in MarkFreeSlots using HASWILD instead of TYPE_CHAR
 * 01.06.94: Function ParseParam works now with index instead of column
 * 02.05.94: NULL Test in LoadDateTime corrected
 * 14.12.93: Error corrected in LoadDateTime (TIMESTAMP-Test)
 * 16.11.93: Parameters base, table removed in StoreDateTime
 * 28.09.93: DAY, MONTH, YEAR, ..., MICROSECOND no longer needs field names as operands
 * 27.06.93: USE_MANAGER renamed to USE_ALERT
 * 26.06.93: Modifications for Borland C++ added
 * 15.06.93: O_KEYCOUNT now allowed with type HASWILD in CalcSumAvg
 * 14.06.93: Position return FAILURE on error
 * 14.05.93: Error in CalcSumAvg corrected db_initcursor must use index #0
 * 12.05.93: Error in CalcMinMax and CalcSumAvg corrected db_fieldinfo before do loop
 * 10.05.93: CalcMinMax is working now with every data type (except strings), using index for better performance
 * 06.05.93: O_ERRNO added, SUM, MIN, MAX params changed
 * 03.05.93: Convert converts from CHAR to FLOAT now
 * 29.04.93: Warnings at level 4 removed
 * 28.04.93: O_RAND, O_POW, O_ROUND, O_TRUNCATE, O_SIGN, O_KEYCOUNT, O_STRLOWER, O_STRUPPER, O_CHAR, O_WORD, O_LONG, O_FLOAT added
 * 27.04.93: Math with SIN, COS, TAN, ASIN, ACOS, ATAN, ABS, EXP, LN, LOG, SQRT added
 * 26.04.93: O_LENGTH, O_POSITION added
 * 18.01.93: O_USER added
 * 16.12.92: Keyboard interrup for endless loops for Windows added
 * 20.11.92: Modifications for DOS added
 * 18.11.92: Packing pragma added
 * 14.11.92: HPVOID used as data type of parameter buffer in db_excute
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 28.10.92: Replaced strcmp with lstrcmp
 * 26.10.92: Catch and Throw used for windows environment
 * 25.10.92: Module header added
 * 01.04.89: Creation of body
 *****************************************************************************/

#include        <ctype.h>
#include        <math.h>
#include        <setjmp.h>
#include        <stdlib.h>
#include        <string.h>

#include        "import.h"

#include        "conv.h"
#include        "files.h"
#include        "utility.h"

#include        "dbroot.h"
#include        "dbdata.h"
#include        "dbtree.h"
#include        "dbcall.h"

#include        "export.h"
#include        "dbexec.h"

#ifndef USE_ALERT
#define USE_ALERT       1       /* set this to 0, if you dont't want link to phoenix manager modules */
#endif

#ifdef _WINDOWS
#define strcmp          lstrcmp
#define strcmpi         lstrcmpi

/* KBV special part */

#define KBV_MAX			256						/* size of KBV buffer */
#define KBV_FIELDS		 17						/* number of fields on smart card */

EXTERN BOOL WINAPI ReadCard (UCHAR *pChipIn);	/* read KBV smart card (in special KEYBOARD.DRV) */

typedef struct
{
  CHAR  *pFieldName;
  UCHAR *pData;
} KBVINFO;

typedef struct
{
  UCHAR KrankenKassenName [29];
  UCHAR KrankenKassenNummer [8];
  UCHAR VKNR [6];
  UCHAR VersichertenNummer [13];
  UCHAR VersichertenStatus [5];
  UCHAR StatusErgaenzung [4];
  UCHAR Titel [16];
  UCHAR VorName [29];
  UCHAR Namenszusatz [16];
  UCHAR FamilienName [29];
  UCHAR Geburtsdatum [9];
  UCHAR StrassenName [29];
  UCHAR WohnsitzLaenderCode [4];
  UCHAR Postleitzahl [8];
  UCHAR OrtsName [24];
  UCHAR GueltigkeitsDatum [5];
  UCHAR PruefSumme [4];
} KBVCARD;

LOCAL KBVCARD KbvCard;
LOCAL SHORT   KbvFailure;		/* error code on Kbv... functions */

LOCAL KBVINFO KbvInfo [KBV_FIELDS] =
{
  "KrankenKassenName",   KbvCard.KrankenKassenName,
  "KrankenKassenNummer", KbvCard.KrankenKassenNummer,
  "VKNR",                KbvCard.VKNR,
  "VersichertenNummer",  KbvCard.VersichertenNummer,
  "VersichertenStatus",  KbvCard.VersichertenStatus,
  "StatusErg‰nzung",     KbvCard.StatusErgaenzung,
  "Titel",               KbvCard.Titel,
  "VorName",             KbvCard.VorName,
  "Namenszusatz",        KbvCard.Namenszusatz,
  "FamilienName",        KbvCard.FamilienName,
  "Geburtsdatum",        KbvCard.Geburtsdatum,
  "StrassenName",        KbvCard.StrassenName,
  "WohnsitzL‰nderCode",  KbvCard.WohnsitzLaenderCode,
  "Postleitzahl",        KbvCard.Postleitzahl,
  "OrtsName",            KbvCard.OrtsName,
  "G¸ltigkeitsDatum",    KbvCard.GueltigkeitsDatum,
  "Pr¸fSumme",           KbvCard.PruefSumme
}; /* KbvFields */

LOCAL SHORT	KbvReadCard       (VOID);
LOCAL VOID	KbvGet            (LPSTR pFieldName);
LOCAL UCHAR KbvCalcChecksum   (UCHAR *chip_in);
LOCAL VOID  KbvTransform      (UCHAR *chip_in);
LOCAL VOID  KbvSortChipData   (UCHAR *chip_in);
LOCAL SHORT KbvGetDataOrError (UCHAR *chip_in, SHORT cinx, UCHAR *dest, UCHAR header);
LOCAL SHORT KbvGetData        (UCHAR *chip_in, SHORT cinx, UCHAR *dest, UCHAR header);

#else
typedef struct tagMETAHEADER
{
    UINT    mtType;
    UINT    mtHeaderSize;
    UINT    mtVersion;
    ULONG   mtSize;
    UINT    mtNoObjects;
    ULONG   mtMaxRecord;
    UINT    mtNoParameters;
} METAHEADER;
#endif

#pragma pack(2)

typedef struct
{
  ULONG  dwKey;						/* the placeable metafile key */
  USHORT hmf;						/* the metafile handle (not used), using USHORT for Win32 compatibity */
  SHORT  left;						/* using short because of Win32 compatibility */
  SHORT  top;						/* using short because of Win32 compatibility */
  SHORT  right;						/* using short because of Win32 compatibility */
  SHORT  bottom;					/* using short because of Win32 compatibility */
  USHORT wInch;						/* metafile units per inch */
  ULONG  dwReserved;					/* just reserved */
  USHORT wChecksum;					/* checksum of above */
} WMFHEADER;

#pragma pack()

#if USE_ALERT
#ifdef _WINDOWS
EXTERN INT Alert (CHAR *pMessage);
#define ALERT(s)        Alert (s)
#else
EXTERN SHORT open_alert (CHAR *alertmsg);
#define ALERT(s)        open_alert (s)
#endif /* _WINDOWS */
#else
#if GEMDOS
#define ALERT(s)        form_alert (1, s)
#else
#define ALERT(s)        0
#endif /* GEMDOS */
#endif /* USE_ALERT */

#if MSDOS | NT
#pragma pack(1)
#endif

typedef struct
{
	CHAR            opcode;
	CHAR            varNum;
}       OPERINST;


typedef struct
{
	CHAR            opcode;
	CHAR            type;
	SHORT           field;
	LONG            address;
}       FIELDINST;


typedef struct
{
	CHAR            opcode;
	SHORT           jumpDist;
}       JMPINST;


typedef struct
{
	CHAR            opcode;
	VALUEDESC       value;
}       CONSTINST;


typedef union
{
	OPERINST        op;
	FIELDINST       fld;
	CONSTINST       con;
	JMPINST         jmp;
}       INSTRUCTION;


#if MSDOS | NT
#pragma pack()
#endif

#define MAX_STACK                       100                     /* size of stack */
#define MAX_STRING						61440L                  /* size for string constants */
#define SLOT_SIZE                       4096                    /* chars per stack entry for strings */
#define MAX_SLOT        (MAX_STRING/SLOT_SIZE)					/* # of string slots, should be not larger than 15 */
#define MAX_LONGSTRSIZE					255

#define IE_FILEEXIST					  1                     /* some error codes for functions IMPORT/EXPORT */
#define IE_FILEOPEN						  2
#define IE_FILEREAD						  3
#define IE_FILEWRITE					  4
#define IE_FILECREATE					  5
#define IE_WRONGFORMAT					  6
#define IE_OUTOFMEMORY						  7
#define IE_BLOBINSERT					  8
#define IE_BLOBREAD						  9
#define IE_FIELD2SMALL					 10

#define BLOB_OBJECT                       0                     /* blob is OLE object */
#define BLOB_BMP                          1                     /* blob interpreted as bitmap */
#define BLOB_DIB                          2                     /* blob interpreted as dib */
#define BLOB_RLE                          3                     /* blob interpreted as rle bitmap */
#define BLOB_WMF                          4                     /* blob interpreted as metafile */
#define BLOB_WAV                          5                     /* blob interpreted as wave sound file */
#define MAX_BLOBTYPE					  6						/* max number of blobs actually interpreted */

#define BLOB_UNKNOWN					  0						/* unknown, used on ATARI ST only */
#define BLOB_SOUND						  1						/* sampled sound, used on ATARI ST only */
#define BLOB_MIDI						  2						/* midi data, used on ATARI ST only */

#define IS_BLOB(type) ((type == TYPE_VARBYTE) || (type == TYPE_VARWORD) || (type == TYPE_VARLONG) || (type == TYPE_PICTURE) || (type == TYPE_BLOB))

typedef struct
{
  BLOBEXT  szExt;                                               /* suffix ob blob (BMP, WMF, WAV etc.) */
  BLOBNAME szName;                                              /* name of object for object packager (PBrush, MSDraw, SoundRec etc.) */
} BLOBSPEC;

typedef CHAR            STRSLOT[SLOT_SIZE];             /* an entry on string heap */
typedef CHAR            LONGSTR[81];                    /* a typically string */
typedef CHAR            STRING[256];                    /* a long string */

#define IS_NULL                    (-1)

#define LEAP_YEAR(year)         ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))

#define MIN_DAY                    1
#define MIN_MONTH                  1
#define MAX_MONTH                 12
#define MIN_YEAR                   1
#define MAX_YEAR                9999

#define MIN_HOUR                   0
#define MAX_HOUR                  23
#define MIN_MINUTE                 1
#define MAX_MINUTE                99
#define MIN_SECOND                 0
#define MAX_SECOND                59
#define MIN_MICRO                  0
#define MAX_MICRO         999999L

#define CHR_FIELDSEP    ';'
#define CHR_TBLSEP              '.'
#define CHR_JOIN                '>'

GLOBAL  jmp_buf         errJmp;                                 /* to abort execution/compilation */

LOCAL   VALUEDESC       stack[MAX_STACK];               /* stack for numeric values */
LOCAL   STRSLOT         strHeap[MAX_SLOT];              /* heap for strings */
LOCAL   USHORT          slotSet;                                /* set of available slots */
LOCAL   CHAR            *codeStart, *codePtr, *codeEnd;
LOCAL   LONG            calcCount;                              /* count for calculation */
LOCAL   FORMAT          *format;

LOCAL SHORT dayTable [2] [13] =
{
  {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
}; /* dayTable */

LOCAL   SHORT sumDays[2][13] =                          /* cumulated sum of days of month */
{
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
}; /* sumDays */

LOCAL BLOBSPEC BlobSpec [MAX_BLOBTYPE] =
{
  "OLE", "OLE",                                 /* could be every OLE object */
  "BMP", "PBrush",
  "DIB", "PBrush",
  "RLE", "PBrush",
  "WMF", "MSDraw",
  "WAV", "SoundRec"
}; /* BlobSpec */

/****** FUNCTIONS ************************************************************/

LOCAL   VOID    Interpret               _((BASE *base, SHORT table,
										CALCCODE *calccode, CHAR *buffer, LPVALUEDESC retval));
LOCAL   VOID    Convert                 _((VALUEDESC *val, SHORT toType));
LOCAL   VOID    DoAddSub                _((VALUEDESC *sp, SHORT sign));
LOCAL   VOID    DoMul                   _((VALUEDESC *sp));
LOCAL   VOID    DoDiv                   _((VALUEDESC *sp));
LOCAL   SHORT   DoCmp                   _((VALUEDESC *sp));
LOCAL   VOID    LoadDateTime    _((INSTRUCTION *ip, VALUEDESC *sp));
LOCAL   VOID    StoreDateTime   _((INSTRUCTION *ip, VALUEDESC *sp, CHAR *buffer));
LOCAL   SHORT   CalcType                _((SHORT type1, SHORT type2));
LOCAL   SHORT   ResultType              _((SHORT type1, SHORT type2));
LOCAL   VOID    LoadField               _((BASE *base, SHORT table, CHAR *buffer,
								   FIELDDESC *fd, VALUEDESC *sp));
LOCAL   VOID    StoreField              _((BASE *base, SHORT table, CHAR *buffer,
								   FIELDDESC *fd, VALUEDESC *sp));
LOCAL   VOID    FirstLast               _((BASE *base, SHORT table,
								   FIELDDESC *fd, VALUEDESC *sp, SHORT sign));
LOCAL   SHORT   FindIndex               _((BASE *base, SHORT table, SHORT       col));
LOCAL   LONG    Date2days               _((DATE *date));
LOCAL   DATE    Days2date               _((LONG days));
LOCAL   LONG    Time2secs               _((TIME *time));
LOCAL   TIME    Secs2time               _((LONG seconds, LONG *days));
LOCAL   DATE    DateAddSub              _((SHORT sign, DATE *date1, DATE *date2));
LOCAL   TIME    TimeAddSub              _((SHORT sign, TIME *time1, TIME *time2));
LOCAL   TIMESTAMP TStampAddSub  _((SHORT sign, TIMESTAMP *tstamp1, TIMESTAMP *tstamp2));
LOCAL   SHORT   CheckDate               _((DATE *date));
LOCAL   SHORT   ParseParam              _((BASE *base, SHORT table, CHAR *param, SHORT *field,
								   SHORT *src_col, SHORT *dst_tbl, SHORT *dst_col, SHORT *dst_inx));
LOCAL   VALUEDESC CalcSumAvg    _((BASE *base, SHORT table, CHAR *buffer, CHAR *param, CHAR opcode));
LOCAL   VALUEDESC CalcMinMax    _((BASE *base, SHORT table, CHAR *buffer, CHAR *param, BOOL calc_max));
LOCAL   VOID    DoGetPut                _((VALUEDESC *sp, BASE *base, SHORT table, CHAR *buffer, BOOL put));
LOCAL   VOID    AdjustDate              _((DATE *date));
LOCAL   VOID    Append                  _((CHAR *d, CHAR *s, LONG len));
LOCAL   VOID    Replace                 _((VALUEDESC *sp));
LOCAL   VOID    Chr                             _((VALUEDESC *sp));
LOCAL   VOID    Length                  _((VALUEDESC *sp));
LOCAL   VOID    Position                _((VALUEDESC *sp));
LOCAL   CHAR    *AllocStrSlot   _((VOID));
LOCAL   VOID    MarkFreeSlots   _((VALUEDESC *top));
LOCAL   VOID    FillBlanks              _((VALUEDESC *sp));
LOCAL   VOID    SubString               _((VALUEDESC *sp));
LOCAL   VOID    Math                    _((VALUEDESC *sp, CHAR opcode));
LOCAL   SHORT   DoBlobImport	_((BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp));
LOCAL   SHORT   DoBlobExport	_((BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp));
LOCAL   SHORT   DoTextImport	_((BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp));
LOCAL   SHORT   DoTextExport	_((BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp));
LOCAL   SHORT   BlobFromExt		_((CHAR *pExt));
LOCAL	VOID	ExtFromPicType	_((SHORT sType, CHAR *pExt));
LOCAL	VOID	ExtFromVarType	_((SHORT sType, CHAR *pExt));
LOCAL   LPSTR   RmLeadingBlanks _((LPSTR s));


/*****  Abort the compilation, returning error number   ************/

GLOBAL  VOID    CalcError       (error)
INT     error;
{
	longjmp(errJmp, error);
}


/*****  Initialize the code generator   ****************************/

GLOBAL  VOID    StartGen        (codeBuf, len)
CHAR    *codeBuf;
LONG    len;
{
	codeStart = codePtr = codeBuf;
	codeEnd = codeBuf + len;
}


/*****  Generate an operator instruction        ************************/

GLOBAL  VOID    GenInst (op, varNum)
OPCODE  op;
SHORT   varNum;
{
	OPERINST        *iPtr;

	iPtr = (OPERINST *)codePtr;
	codePtr += sizeof(OPERINST);
	if      (codePtr > codeEnd)
		CalcError(PC_OVERFLOW);
	iPtr->opcode = (CHAR)op;
	iPtr->varNum = (CHAR)varNum;
}


/*****  Check whether name is a field name      ************************/

GLOBAL  VOID    LookupField     (base, table, name, fieldDesc)
BASE            *base;
SHORT           table;
CHAR            *name;
FIELDDESC       *fieldDesc;
{
	FIELD_INFO  fieldInfo;
	SHORT           field;

	strncpy(fieldInfo.name, name, MAX_FIELDNAME-1);
	fieldInfo.name[MAX_FIELDNAME - 1] = EOS;

	fieldDesc->field = FAILURE;
	field = db_fieldinfo (base, table, FAILURE, &fieldInfo);
	if ((field != FAILURE) &&
		(db_acc_column (base, table, field) & (GRANT_UPDATE | GRANT_INSERT)))
	{
		fieldDesc->field = field;
		fieldDesc->type  = fieldInfo.type;
		fieldDesc->addr  = fieldInfo.addr;
		fieldDesc->flags = fieldInfo.flags;
	}
}


/*****  Generate a field access instruction     ************************/

GLOBAL  VOID    GenFieldInst    (op, fieldDesc)
OPCODE          op;
FIELDDESC       *fieldDesc;
{
	FIELDINST       *iPtr;

	iPtr = (FIELDINST *)codePtr;
	codePtr += sizeof(FIELDINST);
	if      (codePtr > codeEnd)
		CalcError(PC_OVERFLOW);
	iPtr->opcode  = (CHAR)op;
	iPtr->type    = (CHAR)fieldDesc->type;
	iPtr->field   = fieldDesc->field;
	iPtr->address = fieldDesc->addr;
}


/*****  Generate a constant load instruction    ********************/

GLOBAL  VOID    GenConstLoad    (val)
VALUEDESC       *val;
{
	CONSTINST       *iPtr;
	long            len;

	iPtr = (CONSTINST *)codePtr;
	codePtr += sizeof(CONSTINST);
	if      (codePtr > codeEnd)
		CalcError(PC_OVERFLOW);
	iPtr->opcode  = O_LCONST;
	iPtr->value   = *val;
	if      (val->type == TYPE_CHAR)
	{
		len = strlen(val->val.stringVal) + 1;
		len += len & 1;
		codePtr += len;
		if      (codePtr > codeEnd)
			CalcError(PC_OVERFLOW);
		strcpy((char *)(iPtr+1), val->val.stringVal);
	}
}


/*****  Get current code pointer                ****************************/

GLOBAL  CODELOC GetCurLoc()
{
	return(codePtr);
}


/*****  Generate a jump instruction             ****************************/

GLOBAL  VOID    GenJmp(targetLoc)
CODELOC targetLoc;
{
	JMPINST *iPtr;

	iPtr = (JMPINST *)codePtr;
	codePtr += sizeof(JMPINST);
	if      (codePtr > codeEnd)
		CalcError(PC_OVERFLOW);
	iPtr->opcode   = O_JMP;
	iPtr->jumpDist = (SHORT)((char *)targetLoc - (char *)iPtr);
}


/*****  Generate a conditional jump instruction ********************/

GLOBAL  VOID    GenFalseJmp(targetLoc)
CODELOC targetLoc;
{
	JMPINST *iPtr;

	iPtr = (JMPINST *)codePtr;
	codePtr += sizeof(JMPINST);
	if      (codePtr > codeEnd)
		CalcError(PC_OVERFLOW);
	iPtr->opcode   = O_FJMP;
	iPtr->jumpDist = (SHORT)((char *)targetLoc - (char *)iPtr);
}


/*****  Insert final distance into a (forward) jump     ****************/

GLOBAL  VOID    PatchJmp(jmpLoc)
CODELOC jmpLoc;
{
	JMPINST *iPtr;

	iPtr = (JMPINST *)jmpLoc;
	iPtr->jumpDist = (SHORT)((char *)codePtr - (char *)iPtr);
}


/*****  Return size of generated code in bytes  *******************/

GLOBAL  LONG    CodeSize        ()
{
	return( codePtr - codeStart );
}


/*****  Execute the code        ****************************************/

LOCAL   VOID  Interpret(base, table, calccode, buffer, retval)
BASE            *base;
SHORT           table;
CALCCODE        *calccode;
CHAR            *buffer;
LPVALUEDESC     retval;
{
	INSTRUCTION     *ip;
	VALUEDESC       *sp;
	SHORT           varCnt;
	FIELDDESC       fd;
	LONG            lRand;

	/* initialize machine registers */
	ip = (INSTRUCTION *)calccode->buffer;
	sp = &stack[-1];

	while (TRUE)
	{
#if GEMDOS | FLEXOS
      {
	SHORT ret;
	SHORT mstate, kstate;

	graf_mkstate (&ret, &ret, &mstate, &kstate);    /* give chance to interrupt endless loops */
	if (kstate == (K_LSHIFT | K_CTRL | K_ALT)) return;
      }
#endif

#ifdef _WINDOWS
	if ((GetAsyncKeyState (VK_SHIFT) < 0) && (GetAsyncKeyState (VK_CONTROL) < 0)) return;
#endif

		if      (sp >= &stack[MAX_STACK - 1])
			CalcError(STACK_OVERFLOW);

		MarkFreeSlots(sp);

		switch  (ip->op.opcode)
		{
		case    O_END:                          /* end of calculation program */
			return;

		case    O_NEG:                          /* negate values */
			switch  (sp->type)
			{
			case    TYPE_WORD:      sp->val.wordVal  = (SHORT)(-sp->val.wordVal);   break;
			case    TYPE_FLOAT:     sp->val.floatVal = -sp->val.floatVal;                   break;
			case    TYPE_DBADDRESS:
			case    TYPE_LONG:      sp->val.longVal  = -sp->val.longVal;                    break;
			default:                        CalcError(TYPE_ERROR);                                                  break;
			}
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_ADD:                          /* add values */
			sp--;
			DoAddSub(sp, 1);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_SUB:                          /* subtract values */
			sp--;
			DoAddSub(sp, -1);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_MUL:                          /* multiply values */
			sp--;
			DoMul(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_DIV:                          /* divide values */
			sp--;
			DoDiv(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_EQ:                           /* compare for equal */
			sp--;
			sp->val.wordVal = (SHORT)(DoCmp(sp) == 0);
			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_NE:                           /* compare for not equal */
			sp--;
			sp->val.wordVal = (SHORT)(DoCmp(sp) != 0);
			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_GT:                           /* compare for greater than */
			sp--;
			sp->val.wordVal = (SHORT)(DoCmp(sp) > 0);
			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LT:                           /* compare for less than */
			sp--;
			sp->val.wordVal = (SHORT)(DoCmp(sp) < 0);
			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_GE:                           /* compare for greater equal */
			sp--;
			sp->val.wordVal = (SHORT)(DoCmp(sp) >= 0);
			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LE:                           /* compare for less equal */
			sp--;
			sp->val.wordVal = (SHORT)(DoCmp(sp) <= 0);
			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_AND:                          /* bitwise and */
			Convert(sp, TYPE_LONG);
			sp--;
			Convert(sp, TYPE_LONG);
			sp->val.longVal = sp[0].val.longVal & sp[1].val.longVal;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_OR:                           /* bitwise or */
			Convert(sp, TYPE_LONG);
			sp--;
			Convert(sp, TYPE_LONG);
			sp->val.longVal = sp[0].val.longVal | sp[1].val.longVal;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_NOT:                          /* not */
			Convert(sp, TYPE_LONG);
			sp->val.longVal = !sp->val.longVal;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_REPLACE:                      /* replace string2 in string1 by string3 */
			sp -= 2;
			Replace(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_CHR:                          /* character function: string = chr (i) */
			Chr(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LENGTH:                       /* length of a string: l = length (s) */
			Length(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_POSITION:
			sp -= 2;
			Position(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_STRLOWER:                     /* convert string to lower case */
			Convert(sp, TYPE_CHAR);
			str_lower (sp->val.stringVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_STRUPPER:                     /* convert string to upper case */
			Convert(sp, TYPE_CHAR);
			str_upper (sp->val.stringVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_COLON:
			sp -= 1;
			FillBlanks(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_SUBSTR:
			sp -= 2;
			SubString(sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_SUM:
		case    O_AVG:
		case    O_KEYCOUNT:
			Convert(sp, TYPE_CHAR);
			*sp = CalcSumAvg(base, table, buffer, sp->val.stringVal, ip->op.opcode);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_MIN:
		case    O_MAX:
			Convert(sp, TYPE_CHAR);
			*sp = CalcMinMax(base, table, buffer, sp->val.stringVal, ip->op.opcode == O_MAX);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_GET:
		case    O_PUT:
			if (ip->op.opcode == O_PUT) sp -= 1;
			DoGetPut(sp, base, table, buffer, ip->op.opcode == O_PUT);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_ALERT:
			Convert(sp, TYPE_CHAR);
			sp->val.wordVal = (SHORT)ALERT (sp->val.stringVal);
			sp->type        = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LFIELD:                       /* load field */
			sp++;
			fd.type  = ip->fld.type;
			fd.field = ip->fld.field;
			fd.addr  = ip->fld.address;
			LoadField(base, table, buffer, &fd, sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->fld));
			break;

		case    O_LNAMEDFLD:            /* load field (name computed)   */
			Convert(&sp[0], TYPE_CHAR);
			LookupField(base, table, sp[0].val.stringVal, &fd);
			if      (fd.field == FAILURE)
				CalcError(FIELD_ERROR);
			LoadField(base, table, buffer, &fd, sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_SFIELD:                       /* store field */
			fd.type  = ip->fld.type;
			fd.field = ip->fld.field;
			fd.addr  = ip->fld.address;
			StoreField(base, table, buffer, &fd, sp);
			sp--;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->fld));
			break;

		case    O_SNAMEDFLD:            /* store field (name computed)  */
			Convert(&sp[-1], TYPE_CHAR);
			LookupField(base, table, sp[-1].val.stringVal, &fd);
			if      (fd.field == FAILURE)
				CalcError(FIELD_ERROR);
			StoreField(base, table, buffer, &fd, sp);
			sp--;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LCONST:                       /* load constant */
			sp++;
			*sp = ip->con.value;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->con));
			if      (sp->type == TYPE_CHAR)
			{
				LONG    len;

				sp->val.stringVal = (char *)ip;
				len = strlen((char *)ip) + 1;
				len += len & 1;
				ip = (INSTRUCTION *)((char *)ip + len);
			}
			break;

		case    O_LCOUNT:                       /* load count */
			sp++;
			sp->type = TYPE_LONG;
			sp->val.longVal = calcCount;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LSYDATE:                      /* load system date */
			sp++;
			sp->type = TYPE_DATE;
			get_date(&sp->val.dateVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LSYTIME:                      /* load system time */
			sp++;
			sp->type = TYPE_TIME;
			get_time(&sp->val.timeVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LSYTIMST:                     /* load system timestamp */
			sp++;
			sp->type = TYPE_TIMESTAMP;
			get_tstamp(&sp->val.timeStampVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LDAY:                         /* load day of date */
		case    O_LMONTH:                       /* load month of date */
		case    O_LYEAR:                        /* load year of date */
		case    O_LHOUR:                        /* load hour of time */
		case    O_LMINUTE:                      /* load minute of time */
		case    O_LSECOND:                      /* load second of time */
		case    O_LMICSEC:                      /* load microsecond of time */
			LoadDateTime(ip, sp);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_SDAY:                         /* store day of date */
		case    O_SMONTH:                       /* store month of date */
		case    O_SYEAR:                        /* store year of date */
		case    O_SHOUR:                        /* store hour of time */
		case    O_SMINUTE:                      /* store minute of time */
		case    O_SSECOND:                      /* store second of time */
		case    O_SMICSEC:                      /* store microsecond of time */
			StoreDateTime(ip, sp, buffer);
			sp--;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->fld));
			break;

		case    O_FIRST:                        /* first (field) */
		case    O_LAST:                         /* last (field) */
			sp++;
			fd.type  = ip->fld.type;
			fd.field = ip->fld.field;
			fd.addr  = ip->fld.address;
			FirstLast(base, table, &fd, sp, (SHORT)(ip->fld.opcode == O_LAST));
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->fld));
			break;

		case    O_USER:                         /* get user id */
			sp++;
			sp->type          = TYPE_CHAR;
			sp->val.stringVal = AllocStrSlot();
			strcpy (sp->val.stringVal, base->username);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_IMPORT:
		case    O_EXPORT:
			sp -= 1;
			Convert (&sp [0], TYPE_CHAR);   /* file name in sp [0], field in sp [1] */

			if (ip->op.opcode == O_IMPORT)
				sp->val.wordVal = HASWILD (fd.type) ? DoTextImport (base, table, buffer, &fd, sp) : DoBlobImport (base, table, buffer, &fd, sp);
			else
				sp->val.wordVal = IS_BLOB (fd.type) ? DoBlobExport (base, table, buffer, &fd, sp) : DoTextExport (base, table, buffer, &fd, sp);

			sp->type = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_RETURN:							/* return value of db_execute */
			if (retval != NULL) *retval = *sp;
			return;									/* end of calculation program */


#ifdef _WINDOWS
		case	O_KBVREAD:                         /* read smart card of "Kassen‰rztliche Bundesvereinigung" */
			sp++;
			sp->type        = TYPE_WORD;
			sp->val.wordVal = KbvReadCard ();
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case	O_KBVGET:                         /* get certain field of smart card of card of "Kassen‰rztliche Bundesvereinigung" */
			Convert (sp, TYPE_CHAR);
			KbvGet (sp->val.stringVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;
#endif

		case    O_SIN:
		case    O_COS:
		case    O_TAN:
		case    O_ASIN:
		case    O_ACOS:
		case    O_ATAN:
		case    O_ABS:
		case    O_EXP:
		case    O_LN:
		case    O_LOG:
		case    O_SQRT:
			Math(sp, ip->op.opcode);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_ERRNO:                                /* get error number on math functions */
			sp++;
			sp->type        = TYPE_WORD;
			sp->val.wordVal = (SHORT)errno;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_RAND:
			Convert(sp, TYPE_LONG);
			lRand = (LONG)rand ();
			sp->val.longVal = lRand % sp->val.longVal;
			sp->type         = TYPE_LONG;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_POW:
			sp -= 1;
			Convert(&sp [0], TYPE_FLOAT);
			Convert(&sp [1], TYPE_FLOAT);
			sp->val.floatVal = pow (sp [0].val.floatVal, sp [1].val.floatVal);
			sp->type         = TYPE_FLOAT;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_ROUND:
			Convert(sp, TYPE_FLOAT);
			sp->val.floatVal = floor (sp->val.floatVal + 0.5);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_TRUNCATE:
			Convert(sp, TYPE_FLOAT);
			sp->val.floatVal = floor (sp->val.floatVal);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_SIGN:
			Convert(sp, TYPE_FLOAT);
			sp->val.wordVal = (SHORT)((sp->val.floatVal > 0) ? 1 : (sp->val.floatVal < 0) ? -1 : 0);
			sp->type        = TYPE_WORD;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_CHAR:                 /* convert value to TYPE_CHAR */
			Convert(sp, TYPE_CHAR);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_WORD:                 /* convert value to TYPE_WORD */
			Convert(sp, TYPE_WORD);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LONG:                 /* convert value to TYPE_LONG */
			Convert(sp, TYPE_LONG);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_FLOAT:                        /* convert value to TYPE_FLOAT */
			Convert(sp, TYPE_FLOAT);
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LNUL:                         /* load NULL value */
			sp++;
			sp->type = IS_NULL;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_JMP:
			ip = (INSTRUCTION *)((char *)ip + ip->jmp.jumpDist);
			break;

		case    O_FJMP:
			Convert(sp, TYPE_LONG);
			if      (sp->val.longVal)
				ip = (INSTRUCTION *)((char *)ip + sizeof(ip->jmp));
			else
				ip = (INSTRUCTION *)((char *)ip + ip->jmp.jumpDist);
			sp--;
			break;

		case    O_INITVARS:
			for     (varCnt = ip->op.varNum; --varCnt >= 0; )
			{
				sp++;
				sp->type = IS_NULL;
			}
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_LOADVAR:
			sp++;
			*sp = stack[ip->op.varNum];
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;

		case    O_STOREVAR:
			stack[ip->op.varNum] = *sp;
			sp--;
			ip = (INSTRUCTION *)((char *)ip + sizeof(ip->op));
			break;
		}
	}
}



/*****  Load a field from data base     ********************************/

LOCAL   VOID    LoadField(base, table, buffer, fd, sp)
BASE            *base;
SHORT           table;
CHAR            *buffer;
FIELDDESC       *fd;
VALUEDESC       *sp;
{
	sp->type = fd->type;

	if      (HASWILD(fd->type))
	{
		sp->val.stringVal = AllocStrSlot();
		strncpy(sp->val.stringVal, &buffer[fd->addr], SLOT_SIZE - 1);
		sp->val.stringVal[SLOT_SIZE - 1] = EOS;
	}
	else
	{
		db_getfield(base, table, fd->field, buffer, &sp->val);
		if (is_null(fd->type, &sp->val)) sp->type = IS_NULL;
    }

}


/*****  Store field in data base        ********************************/

LOCAL   VOID    StoreField(base, table, buffer, fd, sp)
BASE            *base;
SHORT           table;
CHAR            *buffer;
FIELDDESC       *fd;
VALUEDESC       *sp;
{
	STRSLOT longstr;

	if      (sp->type == IS_NULL)
	{
		set_null(fd->type, buffer + fd->addr);
		return;
	}

	if      (HASWILD(sp->type))
		strcpy(longstr, sp->val.stringVal);
	else
		bin2str(sp->type, &sp->val, longstr);

	if      (HASWILD(fd->type))
	{
		if      (format != NULL)
			str2format(sp->type, longstr, format[sp->type]);
		db_setfield(base, table, fd->field, buffer, longstr);
	}
	else
	{
		if      (HASWILD(sp->type))
			if      (format != NULL)
				format2str(fd->type, longstr, format[fd->type]);
		str2bin(fd->type, longstr, &sp->val);
		db_setfield(base, table, fd->field, buffer, &sp->val);
	}
}


/*****  Add or subtract values  ************************************/

LOCAL   VOID    DoAddSub(sp, sign)
VALUEDESC               *sp;
SHORT                   sign;
{
	SHORT           calcType;
	SHORT           resultType;
	LONG            days;
	CHAR            *d;
    VALUEDESC   sp0, sp1;

    sp0 = sp[0];
    sp1 = sp[1];
	calcType = CalcType(sp[0].type, sp[1].type);
	resultType = ResultType(sp[0].type, sp[1].type);
	Convert(&sp[0], calcType);
	Convert(&sp[1], calcType);
	switch  (calcType)
	{
	case    TYPE_FLOAT:
		if      (sign > 0)
			sp[0].val.floatVal += sp[1].val.floatVal;
		else
			sp[0].val.floatVal -= sp[1].val.floatVal;
		break;

	case    TYPE_DATE:
		if ((sp0.type ==TYPE_DATE) && (sp1.type == TYPE_DATE))
		{
			if      (sign > 0)
				sp[0].val.dateVal = DateAddSub(sign, &sp[0].val.dateVal, &sp[1].val.dateVal);
			else
			{
				sp[0].val.floatVal = Date2days(&sp[0].val.dateVal)
								   - Date2days(&sp[1].val.dateVal);
				sp[0].type = resultType = TYPE_FLOAT;
			}
		}
		else    /* handle cases like DATE +/- number and number + DATE */
		{
			if (sp0.type == TYPE_DATE)
			{
				Convert(&sp1, TYPE_FLOAT);
				days = (LONG)(Date2days(&sp0.val.dateVal) + sp1.val.floatVal * sign);
			}
			else    /* number + DATE */
			{
				Convert(&sp0, TYPE_FLOAT);
				days = (LONG)(Date2days(&sp1.val.dateVal) + sp0.val.floatVal * sign);
			}

			sp[0].type = TYPE_DATE;
			sp[0].val.dateVal = Days2date (days);
		}
		break;

	case    TYPE_TIMESTAMP:
		sp[0].val.timeStampVal = TStampAddSub(sign, &sp[0].val.timeStampVal, &sp[1].val.timeStampVal);
		break;

	case    TYPE_CHAR:
		if      (sign < 0)
			CalcError(TYPE_ERROR);
		else
		{
			d = AllocStrSlot();
			Append(d, sp[0].val.stringVal, strlen(sp[0].val.stringVal));
			Append(d, sp[1].val.stringVal, strlen(sp[1].val.stringVal));
			sp[0].val.stringVal = d;
		}
		break;
	}
	Convert(&sp[0], resultType);
}


/*****  Multiply values ********************************************/

LOCAL   VOID    DoMul(sp)
VALUEDESC       *sp;
{
	SHORT   calcType;
	SHORT   resultType;

	calcType = CalcType(sp[0].type, sp[1].type);
	resultType = ResultType(sp[0].type, sp[1].type);
	Convert(&sp[0], calcType);
	Convert(&sp[1], calcType);
	switch  (calcType)
	{
	case    TYPE_FLOAT:
		sp[0].val.floatVal *= sp[1].val.floatVal;
		break;

	default:
		CalcError(TYPE_ERROR);
		break;
	}
	Convert(&sp[0], resultType);
}


/*****  Divide values   ********************************************/

LOCAL   VOID    DoDiv(sp)
VALUEDESC       *sp;
{
	SHORT   calcType;
	SHORT   resultType;

	calcType = CalcType(sp[0].type, sp[1].type);
	resultType = ResultType(sp[0].type, sp[1].type);
	Convert(&sp[0], calcType);
	Convert(&sp[1], calcType);
	switch  (calcType)
	{
	case    TYPE_FLOAT:
		if      (sp[1].val.floatVal == 0)
			CalcError(DIV_BY_ZERO);
		else
			sp[0].val.floatVal /= sp[1].val.floatVal;
		break;

	default:
		CalcError(TYPE_ERROR);
		break;
	}
	Convert(&sp[0], resultType);
}


/*****  Find type in which to perform calculation       ****************/

LOCAL   SHORT   CalcType(type1, type2)
SHORT   type1;
SHORT   type2;
{
	if      (HASWILD(type1) || HASWILD(type2))
		return(TYPE_CHAR);

	if      (type1 == TYPE_TIMESTAMP || type2 == TYPE_TIMESTAMP)
		return(TYPE_TIMESTAMP);

	if      (type1 == TYPE_DATE || type2 == TYPE_DATE)
		return(TYPE_DATE);

	return(TYPE_FLOAT);
}


/*****  Find result type of calculation ****************************/

LOCAL   SHORT   ResultType(type1, type2)
SHORT   type1;
SHORT   type2;
{
	if      (HASWILD(type1) || HASWILD(type2))
		return(TYPE_CHAR);

	if      (type1 == TYPE_TIMESTAMP || type2 == TYPE_TIMESTAMP)
		return(TYPE_TIMESTAMP);

	if      (type1 == TYPE_DATE || type2 == TYPE_DATE)
		return(TYPE_DATE);

	if      (type1 == TYPE_TIME || type2 == TYPE_TIME)
		return(TYPE_TIME);

	return(TYPE_FLOAT);
}


/*****  Convert value to another type   ****************************/

LOCAL   VOID    Convert(val, toType)
VALUEDESC       *val;
SHORT           toType;
{
	LONG    days;
	STRSLOT longstr;

	if      (val->type == toType)
		return;

	switch  (toType)
	{
	case    TYPE_CHAR:
	case    TYPE_EXTERN:
		if      (!HASWILD(val->type))
		{
			CHAR    *d;

			d = AllocStrSlot();
			bin2str(val->type, &val->val, d);
			val->val.stringVal = d;
		}
		break;

	case    TYPE_WORD:
		Convert(val, TYPE_FLOAT);
		val->val.wordVal = (SHORT)floor(val->val.floatVal + 0.5);
		break;

	case    TYPE_LONG:
		Convert(val, TYPE_FLOAT);
		val->val.longVal = (LONG)floor(val->val.floatVal + 0.5);
		break;

	case    TYPE_FLOAT:
		switch  (val->type)
		{
		case    IS_NULL:
			val->val.floatVal = 0;
			break;

		case    TYPE_CHAR:
		case    TYPE_EXTERN:
			strcpy(longstr, val->val.stringVal);
			str2bin(toType, longstr, &val->val.floatVal);
			break;

		case    TYPE_WORD:
			val->val.floatVal = val->val.wordVal;
			break;

		case    TYPE_LONG:
			val->val.floatVal = val->val.longVal;
			break;

		case    TYPE_TIME:
			val->val.floatVal = Time2secs(&val->val.timeVal);
			break;

		case    TYPE_DATE:
			val->val.floatVal = Date2days(&val->val.dateVal);
			break;

		case    TYPE_TIMESTAMP:
			val->val.floatVal = Time2secs(&val->val.timeStampVal.time)
							  + Date2days(&val->val.timeStampVal.date)*86400.0;
			break;
		}
		break;

	case    TYPE_TIME:
		Convert(val, TYPE_FLOAT);
		val->val.timeVal = Secs2time((LONG)val->val.floatVal, NULL);
		break;

	case    TYPE_DATE:
		Convert(val, TYPE_FLOAT);
		val->val.dateVal = Days2date((LONG)val->val.floatVal);
		break;

	case    TYPE_TIMESTAMP:
		switch  (val->type)
		{
		case    TYPE_TIME:
			val->val.timeStampVal.time = val->val.timeVal;
			val->val.timeStampVal.date = Days2date(0);
			break;

		case    TYPE_DATE:
			val->val.timeStampVal.date = val->val.dateVal;
			val->val.timeStampVal.time = Secs2time(0L, NULL);
			break;

		default:
			Convert(val, TYPE_FLOAT);
			val->val.timeStampVal.time = Secs2time((long)val->val.floatVal, &days);
			val->val.timeStampVal.date = Days2date(days);
		}
	}
	val->type = toType;
}


/*****  Compare two values. Return <0, =0, >0, as in strcmp()   ****/

LOCAL   SHORT   DoCmp(sp)
VALUEDESC       *sp;
{
	DOUBLE  diff;

	if      (HASWILD(sp[0].type))
	{
		Convert(&sp[1], TYPE_CHAR);
		return( (SHORT)strcmp(sp[0].val.stringVal, sp[1].val.stringVal) );
	}
	else if (HASWILD(sp[1].type))
	{
		Convert(&sp[0], TYPE_CHAR);
		return( (SHORT)strcmp(sp[0].val.stringVal, sp[1].val.stringVal) );
	}
	else
	{
		if (sp [0].type == IS_NULL)             /* NULL is smaller than any number */
			return ((SHORT)((sp [1].type == IS_NULL) ? 0 : -1));

		if (sp [1].type == IS_NULL)
			return ((SHORT)((sp [0].type == IS_NULL) ? 0 : 1));

		Convert(&sp[0], TYPE_FLOAT);
		Convert(&sp[1], TYPE_FLOAT);
		diff = sp[0].val.floatVal - sp[1].val.floatVal;
		if      (diff == 0)
			return(0);
		else if (diff < 0)
			return(-1);
		else
			return(1);
	}
}


/*****  Perform field extraction instructions   ********************/

LOCAL   VOID    LoadDateTime(ip, sp)
INSTRUCTION             *ip;
VALUEDESC               *sp;
{
	DATE            *d;
	TIME            *t;
	long            val = 0;

	if      (is_null(sp->type, &sp->val))
		sp->type = IS_NULL;
	else
	{
		d = &sp->val.dateVal;
		t = &sp->val.timeVal;
		if      (sp->type == TYPE_TIMESTAMP)
		{
			d = &sp->val.timeStampVal.date;
			t = &sp->val.timeStampVal.time;
		}

		switch (ip->fld.opcode)
		{
		case    O_LDAY:         val = d->day;           break;
		case    O_LMONTH:       val = d->month;         break;
		case    O_LYEAR:        val = d->year;          break;
		case    O_LHOUR:        val = t->hour;          break;
		case    O_LMINUTE:      val = t->minute;        break;
		case    O_LSECOND:      val = t->second;        break;
		case    O_LMICSEC:      val = t->micro;         break;
		}

		sp->type = TYPE_FLOAT;
		sp->val.floatVal = val;
	}
}


/*****  Perform field insertion instructions    ********************/

LOCAL   VOID    StoreDateTime(ip, sp, buffer)
INSTRUCTION             *ip;
VALUEDESC               *sp;
CHAR                    *buffer;
{
	DATE            *d;
	TIME            *t;
	LONG            val;
	VALUE           *vp;
	BOOL            setField;
	STRSLOT         longStr;
	SHORT           type;
	DATE            null_date;
	TIME            null_time;
	TIMESTAMP       null_tstamp;

	setField = FALSE;

	mem_set (&null_date,   0, sizeof (null_date));
	mem_set (&null_time,   0, sizeof (null_time));
	mem_set (&null_tstamp, 0, sizeof (null_tstamp));

	if      (sp->type == IS_NULL)
	{
		set_null( ip->fld.type, buffer + ip->fld.address );
		return;
	}

	if      (HASWILD(sp->type))
		strcpy(longStr, sp->val.stringVal);
	else
		setField = bin2str(sp->type, &sp->val, longStr);

	setField = str2bin(TYPE_LONG, longStr, &val);

	if      (setField)
	{
		vp = (VALUE *)(buffer + ip->fld.address);
		d = &vp->dateVal;
		t = &vp->timeVal;
		type = ip->fld.type;
		if      (type == TYPE_TIMESTAMP)
		{
			d = &vp->timeStampVal.date;
			t = &vp->timeStampVal.time;
		}

		switch (ip->fld.opcode)
		{
		case    O_SDAY:
			d->day = (UCHAR)val;
			*d = DateAddSub(1, d, &null_date);
			break;

		case    O_SMONTH:
			d->month = (UCHAR)val;
			AdjustDate (d);
			break;

		case    O_SYEAR:
			d->year = (USHORT)val;
			break;

		case    O_SHOUR:
			t->hour = (UCHAR)val;
			if      (type == TYPE_TIME)
				*t = TimeAddSub(1, t, &null_time);
			else
				vp->timeStampVal = TStampAddSub(1, &vp->timeStampVal, &null_tstamp);
			break;

		case    O_SMINUTE:
			t->minute = (UCHAR)val;
			if      (type == TYPE_TIME)
				*t = TimeAddSub(1, t, &null_time);
			else
				vp->timeStampVal = TStampAddSub(1, &vp->timeStampVal, &null_tstamp);
			break;

		case    O_SSECOND:
			t->second = (UCHAR)val;
			if      (type == TYPE_TIME)
				*t = TimeAddSub(1, t, &null_time);
			else
				vp->timeStampVal = TStampAddSub(1, &vp->timeStampVal, &null_tstamp);
			break;

		case    O_SMICSEC:
			t->micro = val;
			if      (type == TYPE_TIME)
				*t = TimeAddSub(1, t, &null_time);
			else
				vp->timeStampVal = TStampAddSub(1, &vp->timeStampVal, &null_tstamp);
			break;
		}

	}
}


/*****  Perform FIRST/LAST instructions         ************************/

LOCAL   VOID    FirstLast(base, table, fd, sp, sign)
BASE            *base;
SHORT           table;
FIELDDESC       *fd;
VALUEDESC       *sp;
SHORT           sign;
{
	SHORT           inx, dir;
	CURSOR          cursor;
	TABLE_INFO      table_info;
	CHAR             *buffer;

	sp->type = IS_NULL;

	switch (fd->type)
	{
	case TYPE_CHAR:                                                                         break;
	case TYPE_WORD:                 sp->val.wordVal  = 0;           break;
	case TYPE_LONG:                 sp->val.longVal  = 0;           break;
	case TYPE_FLOAT:                sp->val.floatVal = 0;           break;
	case TYPE_CFLOAT:
	case TYPE_DATE:
	case TYPE_TIME:
	case TYPE_TIMESTAMP:
	case TYPE_VARBYTE:
	case TYPE_VARWORD:
	case TYPE_VARLONG:
	case TYPE_PICTURE:
	case TYPE_EXTERN:               sp->type                  = IS_NULL; break;
	case TYPE_DBADDRESS:    sp->val.longVal   = 0;          break;
	}

	db_tableinfo(base, table, &table_info);
	buffer = mem_alloc(table_info.size);
	if (buffer == NULL)
	{
		CalcError(DB_NOMEMORY);
		return;
	}

	inx = FindIndex(base, table, fd->field);
	if (inx == FAILURE)
		inx = 0;

	dir = (SHORT)((sign == 0) ? ASCENDING : DESCENDING);
	if      (db_initcursor (base, table, inx, dir, &cursor))
		if      (db_movecursor (base, &cursor, (LONG)dir))
			if      (db_read (base, table, buffer, &cursor, 0L, FALSE))
				LoadField(base, table, buffer, fd, sp);

	mem_free (buffer);
}


/*****  Return index number, if a key is defined on given field ****/

LOCAL SHORT     FindIndex(base, table, col)
BASE    *base;
SHORT   table;
SHORT   col;
{
	SHORT           index;
	TABLE_INFO      table_info;
	INDEX_INFO      index_info;

	db_tableinfo(base, table, &table_info);

	for (index = 0; index < table_info.indexes; index++)
		if      (  db_indexinfo (base, table, index, &index_info) != FAILURE
			&& (index_info.inxcols.size / sizeof (INXCOL) == 1) /* single key field */
			&& (index_info.inxcols.cols [0].col == col))
			return (index);

  return        (FAILURE);
}


/*****  Convert a date to number of days        ************************/

LOCAL   LONG    Date2days(date)
DATE    *date;
{
	LONG days, year;

	year  = date->year;
	days  = date->day;
	days += sumDays[LEAP_YEAR(year)][date->month - 1];
	year--;
	days += year * 365 + year / 4 - year / 100 + year / 400;

	return(days);
}


/*****  Convert number of days to a date        ************************/

LOCAL DATE Days2date(days)
LONG    days;
{
	BOOL    y;
	SHORT   month, year;
	LONG    d;
	DATE    date;

	year = (SHORT)(days / 365.25);
	if      (year > 0)
	{
		date.day        = 31;
		date.month      = 12;
		date.year       = year;
		days            -= Date2days(&date);
	} /* if */

	if      (days > 0)
	{
		year++;                                         /* go to actual year */
		d = 0;

		while (days >= 365)                     /* 1 <= days <= 440 (15-03-9996) */
		{
			d         = days;
			days -= 365;
			if      (LEAP_YEAR(year))
				days--;                         /* 366 days in leap years */
			if      (days > 0)
				year++;
		} /* while */

		if      (days <= 0)
			days = d;                               /* reset days */

		y       = LEAP_YEAR(year);
		month = 1;
		while   (days > sumDays[y][month])
			month++;         /* search for month */

		days -= sumDays[y][month-1];

		date.day        = (UCHAR)days;
		date.month      = (UCHAR)month;
		date.year       = (USHORT)year;
	} /* if */

	return(date);
} /* Days2date */


/****   Convert time to number of seconds       ************************/

LOCAL LONG Time2secs(time)
TIME *time;
{
	LONG seconds;

	seconds  = (LONG)time->second;
	seconds += (LONG)time->minute * 60;
	seconds += (LONG)time->hour * 3600;

	return(seconds);
} /* Time2secs */


/*****  Convert number of seconds to time       ************************/

LOCAL TIME Secs2time(seconds, days)
LONG    seconds;
LONG    *days;
{
	LONG d;
	TIME time;

	d = seconds / 86400L;
	while   (seconds < 0)
	{
		d--;
		seconds += 86400L;
	} /* if */

	time.hour       = (UCHAR)((seconds / 3600) % 24);
	seconds         = seconds % 3600;
	time.minute     = (UCHAR)(seconds / 60);
	time.second     = (UCHAR)(seconds % 60);
	time.micro      = 0;

	if      (days)
		*days = d;

	return(time);
} /* Secs2time */


/*****  Add or subtract dates   ************************************/

LOCAL DATE DateAddSub(sign, date1, date2)
SHORT sign;
DATE *date1;
DATE *date2;
{
	SHORT   err;
	SHORT   month, year;
	SHORT   day1, month1, year1;
	SHORT   day2, month2, year2;
	LONG    days;
	DATE    date;

	date = *date1;
	err     = CheckDate(&date);             /* date1 = DATE, date2 = date duration */
	if      (err == FAILURE)
		return(date);

	day1    = date1->day;
	month1  = date1->month;
	year1   = date1->year;
	day2    = (SHORT)(sign * date2->day);
	month2  = (SHORT)(sign * date2->month);
	year2   = (SHORT)(sign * date2->year);

	year    = (USHORT)(year1 + year2);
	year   += (USHORT)(month2 / 12);
	month2  = (SHORT)(month2 % 12);
	month   = (SHORT)(month1 + month2);

	if      (month > 12)
	{
		year++;
		month -= 12;
	} /* if */

	if      (month <= 0)
	{
		year--;
		month = (SHORT)(12 + month);
	} /* if */

	date.day        = (UCHAR)day1;
	date.month      = (UCHAR)month;
	date.year       = (USHORT)year;

	days = Date2days(&date) + day2;
	date = Days2date(days);

	CheckDate(&date);

	return(date);
} /* DateAddSub */


/*****  Add or subtract times **************************************/

LOCAL TIME TimeAddSub (sign, time1, time2)
SHORT sign;
TIME *time1;
TIME *time2;
{
	TIMESTAMP tstamp1, tstamp2;

	mem_set (&tstamp1, 0, sizeof (tstamp1));
	mem_set (&tstamp2, 0, sizeof (tstamp2));

	tstamp1.date.day   = 1;       /* take any valid default value */
	tstamp1.date.month = 1;
	tstamp1.date.year  = 1;
	tstamp2.date       = tstamp1.date;

	tstamp1.time = *time1;
	tstamp2.time = *time2;
	tstamp1      = TStampAddSub (sign, &tstamp1, &tstamp2);

	return (tstamp1.time);
} /* TimeAddSub */

/*****  Add or subtract time stamps     ********************************/

LOCAL TIMESTAMP TStampAddSub(sign, tstamp1, tstamp2)
SHORT           sign;
TIMESTAMP       *tstamp1;
TIMESTAMP       *tstamp2;
{
	SHORT           err;
	LONG            seconds, seconds1, seconds2;
	LONG            micro, days;
	TIMESTAMP       tstamp;

	tstamp  = *tstamp1;
	err             = CheckDate(&tstamp.date);       /* tstamp1 = TIMESTAMP, tstamp2 = timestamp duration */
	if      (err == FAILURE)
		return(tstamp);

	micro           = tstamp1->time.micro + tstamp2->time.micro * sign;
	seconds1        = Time2secs(&tstamp1->time);
	seconds2        = Time2secs(&tstamp2->time);
	seconds1        += micro / 1000000L;
	micro           = micro % 1000000L;
	seconds         = seconds1 + seconds2 * sign;

	if      (micro < 0)
	{
		seconds--;
		micro += 1000000L;
	} /* if */

	tstamp.time               = Secs2time(seconds, &days);
	tstamp.time.micro = micro;
	tstamp.date               = DateAddSub(sign, &tstamp1->date, &tstamp2->date);
	days                      = Date2days(&tstamp.date) + days;
	tstamp.date               = Days2date(days);

	CheckDate(&tstamp.date);

	return(tstamp);
} /* TstampAddSub */


/*****  Check whether date is reasonable and adjust ****************/

LOCAL SHORT CheckDate(date)
DATE *date;
{
	SHORT err;
	SHORT max_day;
	SHORT day, month, year;

	err       = SUCCESS;
	day       = date->day;
	month = date->month;
	year  = date->year;

	if      (  (year  >= MIN_YEAR)
		&& (year  <= MAX_YEAR)
		&& (month >= MIN_MONTH)
		&& (month <= MAX_MONTH)
		&& (day   >= MIN_DAY))
	{
		max_day = dayTable[LEAP_YEAR(year)][month];
		if      (day > max_day)
			date->day = (UCHAR)max_day;
	} /* if */
	else
		err = FAILURE;

	return(err);
} /* CheckDate */


/*****  Adjust date if  month is larger than 12 ********************/

LOCAL VOID AdjustDate (date)
DATE *date;

{
	SHORT month;
	SHORT year;

	month = (SHORT)((date->month - 1) % 12 + 1);
	year  = (SHORT)((date->month - 1) / 12);

	date->month  = (UCHAR)month;
	date->year  += year;

	if      (date->year < MIN_YEAR)
		date->year = MIN_YEAR;

	if      (date->year > MAX_YEAR)
		date->year = MAX_YEAR;

	CheckDate (date);
} /* AdjustDate */


/*****  Extract tables and fields from parameter and return error                        **/
/*****  Syntax is: SUM ("Field;SrcCol1[,SrcCol2,...,SrcColN]>DstTable.DstInx"); ***********/
/*****  or         SUM ("Table.Field");                                         ***********/

LOCAL   SHORT   ParseParam(base, table, param, field, src_col, dst_tbl, dst_col, dst_inx)
BASE    *base;
SHORT   table;
CHAR    *param;
SHORT   *field;
SHORT   *src_col;
SHORT   *dst_tbl;
SHORT   *dst_col;
SHORT   *dst_inx;
{
	STRSLOT         s;
	CHAR            *pField;
	CHAR            *pSrcCol;
	CHAR            *pDstTbl;
	CHAR            *pDstInx;
	TABLENAME       table_name;
	TABLE_INFO      table_info;
	FIELD_INFO      field_info;
	INDEX_INFO  index_info;

	strcpy (s, param);
	strcpy (table_name, db_tablename (base, table));
	pField = s;

	if      ((strchr (s, CHR_FIELDSEP) == NULL) &&
		 (strchr (s, CHR_JOIN)     == NULL) &&
		 (strchr (s, CHR_TBLSEP)   != NULL))            /* syntax is SUM ("Table.Field"); */
	{
		pField  = strchr (s, CHR_TBLSEP);
		*pField = EOS;
		pField++;               

		strcpy (table_info.name, s);
	str_rmchar (table_info.name, ' ');
	    *dst_tbl = db_tableinfo(base, FAILURE, &table_info);
	if      (*dst_tbl == FAILURE)
		return (CE_DST_TABLE);

		strcpy (field_info.name, pField);
	str_rmchar (field_info.name, ' ');
	    *field = db_fieldinfo(base, *dst_tbl, FAILURE, &field_info);
	if      (*field == FAILURE)
		return (CE_FUNCTION_FIELD);

	    *dst_inx = FindIndex(base, *dst_tbl, *field);
		*dst_col = 0;   /* sequential */

	return (SUCCESS);
	} /* if */

	pSrcCol = strchr (s, CHR_FIELDSEP);
	if      (pSrcCol == NULL)
	return (CE_SRC_FIELD);
	else
	{
		*pSrcCol = EOS;
		pSrcCol++;
	}

	pDstTbl = strchr (pSrcCol, CHR_JOIN);
	if      (pDstTbl == NULL)
	return (CE_DST_TABLE);
	else
	{
		*pDstTbl = EOS;
		pDstTbl++;
	}

	pDstInx = strchr (pDstTbl, CHR_TBLSEP);
	if      (pDstInx == NULL)
	return (CE_DST_INDEX);
	else
	{
		*pDstInx = EOS;
		pDstInx++;
	}

	strcpy (table_info.name, pDstTbl);
    str_rmchar (table_info.name, ' ');
    *dst_tbl = db_tableinfo(base, FAILURE, &table_info);
    if  (*dst_tbl == FAILURE)
	return (CE_DST_TABLE);

	strcpy (field_info.name, pField);
    str_rmchar (field_info.name, ' ');
    *field = db_fieldinfo(base, *dst_tbl, FAILURE, &field_info);
    if  (*field == FAILURE)
	return (CE_FUNCTION_FIELD);

	strcpy (field_info.name, pSrcCol);
    str_rmchar (field_info.name, ' ');
    *src_col = db_fieldinfo(base, table, FAILURE, &field_info);
    if  (*src_col == FAILURE)
	return (CE_SRC_FIELD);

	strcpy (index_info.name, pDstInx);
    str_rmchar (index_info.name, ' ');
	*dst_inx = db_indexinfo (base, *dst_tbl, FAILURE, &index_info);
    if  (*dst_inx == FAILURE)	/* try to find field with this index */
    {
		strcpy (field_info.name, pDstInx);
		str_rmchar (field_info.name, ' ');
	    *dst_col = db_fieldinfo(base, *dst_tbl, FAILURE, &field_info);

		if	(*dst_col == FAILURE)
		return (CE_DST_INDEX);

	    *dst_inx = FindIndex(base, *dst_tbl, *dst_col);
    } /* if */
	else
	{
		if (index_info.inxcols.size / sizeof (INXCOL) == 1) /* single key field */
			*dst_col = index_info.inxcols.cols [0].col;
		else
			*dst_col = FAILURE;
    } /* else */

	return (SUCCESS);
}


/*****  Calcuate sum/avg of field from given table, field **********/

LOCAL   VALUEDESC       CalcSumAvg(base, table, buffer, param, opcode)
BASE    *base;
SHORT   table;
CHAR    *buffer;
CHAR    *param;
CHAR    opcode;
{
	SHORT           field;
	SHORT           src_col;
	SHORT           dst_tbl;
	SHORT           dst_col;
	SHORT           dst_inx;
	SHORT           error;
	LONG            recs;
	VALUEDESC       val, res;
	CURSOR          cursor;
	TABLE_INFO      table_info;
	FIELD_INFO      field_info;
	CHAR            keyval [MAX_KEYSIZE];
	HPCHAR          db_buffer;

	res.type         = TYPE_FLOAT;
	res.val.floatVal = 0.0;

	error = ParseParam(base, table, param, &field, &src_col, &dst_tbl, &dst_col, &dst_inx);
	if      (error != SUCCESS)
	{
		CalcError (error);
		return (res);
	}

	db_fieldinfo(base, dst_tbl, field, &field_info);
	if      (HASWILD(field_info.type) && (opcode != O_KEYCOUNT))
	{
		CalcError(TYPE_ERROR);
		return (res);
	}

	db_tableinfo(base, dst_tbl, &table_info);
	db_buffer = mem_alloc(table_info.size);
	if (db_buffer == NULL)
	{
		CalcError(DB_NOMEMORY);
		return (res);
	}

	recs     = 0;
	res.type = val.type = field_info.type;
	mem_set (&res.val, 0, sizeof (res.val));

	if (opcode == O_KEYCOUNT) res.type = TYPE_LONG;

	if (dst_col == 0)       /* get all records sequentially */
	{
		if (opcode == O_KEYCOUNT)
		{
			res.val.longVal = table_info.recs;
			mem_free (db_buffer);

			return (res);
		} /* if */

		db_initcursor(base, dst_tbl, 0, ASCENDING, &cursor);
		while (db_movecursor (base, &cursor, 1L))
		{
			db_read (base, dst_tbl, db_buffer, &cursor, 0L, FALSE);
			recs++;

			if      (! is_null (field_info.type, db_buffer + field_info.addr))
			{
				db_getfield (base, dst_tbl, field, db_buffer, &val.val);

				switch (field_info.type)
				{
				case    TYPE_WORD:
					res.val.wordVal += val.val.wordVal;
					break;
				case    TYPE_LONG:
					res.val.longVal += val.val.longVal;
					break;
				case    TYPE_FLOAT:
					res.val.floatVal += val.val.floatVal;
					break;
				case    TYPE_TIME:
					res.val.longVal += Time2secs(&val.val.timeVal);
					break;
				default:
					CalcError(TYPE_ERROR);
					break;
				} /* switch */
			} /* if */
		} /* while */
	}
	else
	{
		db_fieldinfo(base, table, src_col, &field_info);
		db_setfield(base, dst_tbl, dst_col, db_buffer, buffer + field_info.addr);
		db_buildkey (base, dst_tbl, dst_inx, db_buffer, keyval);

		if      (! db_keysearch(base, dst_tbl, dst_inx, ASCENDING, &cursor, keyval, 0L))
		{
		    res.type = TYPE_LONG;
			mem_free (db_buffer);
			return (res);
		} /* if */

		db_fieldinfo(base, dst_tbl, field, &field_info);

		do
		{
			recs++;

			if (opcode == O_KEYCOUNT)
				continue;
			else
				db_read (base, dst_tbl, db_buffer, &cursor, 0L, FALSE);

			if      (! is_null (field_info.type, db_buffer + field_info.addr))
			{
				db_getfield (base, dst_tbl, field, db_buffer, &val.val);

				switch (field_info.type)
				{
				case    TYPE_WORD:
					res.val.wordVal += val.val.wordVal;
					break;
				case    TYPE_LONG:
					res.val.longVal += val.val.longVal;
					break;
				case    TYPE_FLOAT:
					res.val.floatVal += val.val.floatVal;
					break;
				case    TYPE_TIME:
					res.val.longVal += Time2secs(&val.val.timeVal);
					break;
				default:
					CalcError(TYPE_ERROR);
					break;
				}
			}
		} while (db_testcursor(base, &cursor, ASCENDING, keyval));
	} /* else */
	
	if      (res.type == TYPE_TIME)
		res.val.timeVal = Secs2time (res.val.longVal, NULL);

	Convert(&res, TYPE_FLOAT);

	switch (opcode)
	{
		case O_AVG:      res.val.floatVal /= recs; break;
		case O_KEYCOUNT: res.val.floatVal = recs;  break;
	} /* switch */

	if      (field_info.type == TYPE_TIME)
		Convert(&res, TYPE_TIME);

	mem_free (db_buffer);

	return (res);
}


/*****  Calcuate min/max of field from given table, field **********/

LOCAL   VALUEDESC       CalcMinMax(base, table, buffer, param, calc_max)
BASE    *base;
SHORT   table;
CHAR    *buffer;
CHAR    *param;
BOOL    calc_max;
{
	SHORT           field, type, inx;
	SHORT           src_col;
	SHORT           dst_tbl;
	SHORT           dst_col;
	SHORT           dst_inx;
	SHORT           error;
	SHORT           dir;
	BOOL            init;
	VALUE           minmax;
	VALUEDESC       res;
	CURSOR          cursor;
	TABLE_INFO      table_info;
	FIELD_INFO      field_info;
	CHAR            keyval [MAX_KEYSIZE];
	HPCHAR          db_buffer;

	res.type         = TYPE_FLOAT;
	res.val.floatVal = 0.0;

	error = ParseParam(base, table, param, &field, &src_col, &dst_tbl, &dst_col, &dst_inx);
	if      (error != SUCCESS)
	{
		CalcError (error);
		return (res);
	}

	db_fieldinfo(base, dst_tbl, field, &field_info);
	if      (HASWILD(field_info.type))
	{
		CalcError(TYPE_ERROR);
		return (res);
	}

	db_tableinfo(base, dst_tbl, &table_info);
	db_buffer = mem_alloc(table_info.size);
	if (db_buffer == NULL)
	{
		CalcError(DB_NOMEMORY);
		return (res);
	}

	dir      = (SHORT)(calc_max ? DESCENDING : ASCENDING);
	res.type = type = field_info.type;
	init     = FALSE;
	inx      = 0;   /* sequential */

	if (dst_col == 0)       /* get all records sequentially */
	{
		if (dst_inx != FAILURE) /* optimize */
			inx = dst_inx;

		db_initcursor(base, dst_tbl, inx, dir, &cursor);
		while (db_movecursor (base, &cursor, (LONG)dir))
		{
			db_read (base, dst_tbl, db_buffer, &cursor, 0L, FALSE);

			if (dst_inx != FAILURE)
			{
				db_getfield (base, dst_tbl, field, db_buffer, &res.val);
				mem_free (db_buffer);
				return (res);   /* min/max key aleady found using index */
			} /* if */

			if      (is_null (field_info.type, db_buffer + field_info.addr))
			{
				if      (! calc_max)
				{
					res.type = IS_NULL;             /* NULL is the smallest key */
					mem_free (db_buffer);
					return (res);
				}
			}
			else
			{
				res.type = type;
				db_getfield (base, dst_tbl, field, db_buffer, &res.val);

				if      (! init)
				{
					minmax = res.val;
					init   = TRUE;
				}

				if (calc_max)
				{
					if (cmp_vals (type, &res.val, &minmax) == 1)
						minmax = res.val;
				}
				else
				{
					if (cmp_vals (type, &res.val, &minmax) == -1)
						minmax = res.val;
				}
			} /* else */
		} /* while */
	}
	else
	{
		db_fieldinfo(base, table, src_col, &field_info);
		db_setfield(base, dst_tbl, dst_col, db_buffer, buffer + field_info.addr);
		db_buildkey (base, dst_tbl, dst_inx, db_buffer, keyval);

		if      (! db_keysearch(base, dst_tbl, dst_inx, dir, &cursor, keyval, 0L))
		{
			mem_free (db_buffer);
			return (res);
		}

		db_fieldinfo(base, dst_tbl, field, &field_info);

		do
		{
			db_read (base, dst_tbl, db_buffer, &cursor, 0L, FALSE);

			if      (is_null (field_info.type, db_buffer + field_info.addr))
			{
				if      (! calc_max)
				{
					res.type = IS_NULL;             /* NULL is the smallest key */
					mem_free (db_buffer);
					return (res);
				}
			}
			else
			{
				res.type = type;
				db_getfield (base, dst_tbl, field, db_buffer, &res.val);

				if      (! init)
				{
					minmax = res.val;
					init   = TRUE;
				}

				if (calc_max)
				{
					if (cmp_vals (type, &res.val, &minmax) == 1)
						minmax = res.val;
				}
				else
				{
					if (cmp_vals (type, &res.val, &minmax) == -1)
						minmax = res.val;
				}
			}
		} while (db_testcursor(base, &cursor, dir, keyval));
	} /* else */

	res.val = minmax;

	mem_free (db_buffer);

	return (res);
}


/*****  Get or put a field from/into another table *****************/

LOCAL   VOID    DoGetPut(sp, base, table, buffer, put)
VALUEDESC       *sp;
BASE            *base;
SHORT           table;
CHAR            *buffer;
BOOL            put;
{
	BOOL            bFound;
	SHORT           field;
	SHORT           src_col;
	SHORT           dst_tbl;
	SHORT           dst_col;
	SHORT           dst_inx;
	SHORT           error;
	SHORT           dir;
	SHORT           sStatus;
	VALUEDESC       res;
	CURSOR          cursor;
	TABLE_INFO      table_info;
	FIELD_INFO      field_info;
	FIELDDESC       fd;
	CHAR            *db_buffer;
	CHAR            *param;

	Convert(&sp[0], TYPE_CHAR);

	param    = sp[0].val.stringVal;
	res.type = IS_NULL;

	error = ParseParam(base, table, param, &field, &src_col, &dst_tbl, &dst_col, &dst_inx);
	if      (error != SUCCESS)
	{
		CalcError (error);
		sp [0] = res;
		return;
	}

	db_tableinfo(base, dst_tbl, &table_info);
	db_buffer = mem_alloc(table_info.size);
	if (db_buffer == NULL)
	{
		CalcError(DB_NOMEMORY);
		sp [0] = res;
		return;
	}

	dir = ASCENDING;

	db_fieldinfo(base, table, src_col, &field_info);
	db_setfield(base, dst_tbl, dst_col, db_buffer, buffer + field_info.addr);

	bFound = db_search(base, dst_tbl, dst_inx, dir, &cursor, db_buffer, 0L) &&
			 db_read (base, dst_tbl, db_buffer, &cursor, 0L, FALSE);

	if (bFound)
	{
		db_fieldinfo(base, dst_tbl, field, &field_info);

		fd.type  = field_info.type;
		fd.field = field;
		fd.addr  = field_info.addr;
		LoadField(base, dst_tbl, db_buffer, &fd, &res);

		if      (put)
		{
			StoreField(base, dst_tbl, db_buffer, &fd, &sp[1]);
			if      (db_reclock (base, *(LONG *)db_buffer))
				 db_update (base, dst_tbl, db_buffer, &sStatus);

			res.type        = TYPE_WORD;
			res.val.wordVal = db_status (base);
		}
	}

	mem_free (db_buffer);
	sp [0] = res;
}

/*****  Append s to d, no more than SLOT_SIZE chars ****************/



LOCAL   VOID    Append(d, s, len)
CHAR    *d;
CHAR    *s;
LONG    len;
{
	LONG    dLen, remLen;

	dLen = strlen(d);
	d += dLen;
	remLen = SLOT_SIZE - dLen - 1;
	if      (len > remLen)
		len = remLen;
	while   (--len >= 0)
		*d++ = *s++;
	*d = 0;
}


/*****  CHR(sp[0])      ***************************************************/

LOCAL   VOID    Chr(sp)
VALUEDESC       *sp;
{
	CHAR    *d, c;

	Convert(&sp[0], TYPE_WORD);
	c = (CHAR)sp->val.wordVal;
	sp[0].val.stringVal = d = AllocStrSlot();
	sp[0].type = TYPE_CHAR;

	d[0] = c;
	d[1] = EOS;
}

/*****  Length(sp[0])   ***********************************************/

LOCAL   VOID    Length(sp)
VALUEDESC       *sp;
{
	SIZE_T  len;

	Convert(&sp[0], TYPE_CHAR);
	len = strlen(sp->val.stringVal);
	sp[0].val.wordVal = (SHORT)len;
	sp[0].type = TYPE_WORD;
} /* Length */


/*****  POSITION(sp[0], sp[1], sp[2]) ********************************/

LOCAL   VOID    Position(sp)
VALUEDESC       *sp;
{
	CHAR    *s, *val, *d;
	LONG    start, pos;

	Convert(&sp[0], TYPE_CHAR);
	Convert(&sp[1], TYPE_CHAR);
	Convert(&sp[2], TYPE_LONG);

	s     = sp[0].val.stringVal;
	val   = sp[1].val.stringVal;
	start = sp[2].val.longVal;
	pos   = FAILURE;

	if      (start < 0) start = 0;
	if      (start >= (LONG)strlen (s)) start = 0;

	d = strstr (s + start, val);

	if (d != NULL) pos = (LONG)d - (LONG)s;

	sp[0].val.longVal = pos;
	sp[0].type = TYPE_LONG;
} /* Position */


/*****  Replace sp[1] in sp[0] by sp[2] ****************************/

LOCAL   VOID    Replace(sp)
VALUEDESC       *sp;
{
	CHAR    *s, *pat, *rep, *d, *e;
	LONG    patLen, repLen;

	Convert(&sp[0], TYPE_CHAR);
	Convert(&sp[1], TYPE_CHAR);
	Convert(&sp[2], TYPE_CHAR);

	s   = sp[0].val.stringVal;
	pat = sp[1].val.stringVal;
	rep = sp[2].val.stringVal;

	patLen = strlen(pat);
	repLen = strlen(rep);
	sp[0].val.stringVal = d = AllocStrSlot();
	while   ((e = strstr(s, pat)) != 0)
	{
		Append(d, s, e - s);
		Append(d, rep, repLen);
		s = e + patLen;
	}
	Append(d, s, strlen(s));
}


/*****  Fill up sp[0] with blanks **********************************/

LOCAL   VOID    FillBlanks(sp)
VALUEDESC       *sp;
{
	CHAR    *s, *d;
	LONG    len, width, spCnt;

	Convert(&sp[0], TYPE_CHAR);
	Convert(&sp[1], TYPE_LONG);
	s = sp[0].val.stringVal;
	len = strlen(s);
	width = labs(sp[1].val.longVal);
	if      (width > SLOT_SIZE-1)
		width = SLOT_SIZE-1;
	spCnt = width - len;
	sp[0].val.stringVal = d = AllocStrSlot();

	if      (sp[1].val.longVal >= 0)                /* spaces are left */
	{
		while   (--spCnt >= 0)
			*d++ = ' ';
		while   ((*d++ = *s++) != 0)
			;
	}
	else
	{
		while   ((*d++ = *s++) != 0)    /* spaces are right */
			;
		while   (--spCnt >= 0)
			*d++ = ' ';
	}
}


/*****  SUBSTR(sp[0], sp[1], sp[2]) ********************************/

LOCAL   VOID    SubString(sp)
VALUEDESC       *sp;
{
	CHAR    *s, *d;
	LONG    pos, len, sLen;

	Convert(&sp[0], TYPE_CHAR);
	Convert(&sp[1], TYPE_LONG);
	Convert(&sp[2], TYPE_LONG);

	s = sp[0].val.stringVal;
	sLen = strlen(s);
	pos = sp[1].val.longVal;
	len = sp[2].val.longVal;

	if      (pos < 0)
		pos = 0;
	else if (pos > sLen)
		pos = sLen;

	if      (len < 0)
		len = 0;
	else if (len > sLen-pos)
		len = sLen-pos;

	sp[0].val.stringVal = d = AllocStrSlot();
	memcpy(d, s + pos, (size_t)len);
	d[len] = 0;
}


/*****  Math(sp[0], opcode) ********************************/

LOCAL   VOID    Math(VALUEDESC *sp, CHAR opcode)
{
	DOUBLE argument, result;

	Convert(&sp[0], TYPE_FLOAT);
	argument = sp[0].val.floatVal;
	errno    = 0;

	switch (opcode)
	{
		case    O_SIN  : result = sin (argument);   break;
		case    O_COS  : result = cos (argument);   break;
		case    O_TAN  : result = tan (argument);   break;
		case    O_ASIN : result = asin (argument);  break;
		case    O_ACOS : result = acos (argument);  break;
		case    O_ATAN : result = atan (argument);  break;
		case    O_ABS  : result = fabs (argument);  break;
		case    O_EXP  : result = exp (argument);   break;
		case    O_LN   : result = log (argument);   break;
		case    O_LOG  : result = log10 (argument); break;
		case    O_SQRT : result = sqrt (argument);  break;
		default        : result = 0.0;              break;
	} /* switch */

	sp[0].val.floatVal = result;
	sp[0].type = TYPE_FLOAT;
} /* Math */

/*****  Import file into field of type blob     ****************************/

LOCAL SHORT DoBlobImport (BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp)
{
  BOOL       bOk;
  SHORT      sFormat, sStatus, sType;
  LONG       lSize;
  EXT        szExt;
  FULLNAME   szFileName, szPath, szName;
  HFILE      hFile;
  HPBLOB     blob;
  FIELD_INFO field_info;
  LONG       *lbuffer;
  SYSBLOB    *sysblob;
  METAHEADER *pMetaHeader;

  strcpy (szFileName, sp [0].val.stringVal);

  if (! file_exist (szFileName)) return (IE_FILEEXIST);

  switch (fd->type)
  {
    case TYPE_VARBYTE :
    case TYPE_VARWORD :
    case TYPE_VARLONG :
    case TYPE_PICTURE : str_upper (szFileName);
                        file_split (szFileName, NULL, szPath, szName, szExt);

                        sType = FAILURE;

                        if (strcmp (szExt, "GEM") == 0) sType = PIC_META;
                        if (strcmp (szExt, "IMG") == 0) sType = PIC_IMAGE;
                        if (strcmp (szExt, "IFF") == 0) sType = PIC_IFF;
                        if (strcmp (szExt, "TIF") == 0) sType = PIC_TIFF;
                        if (strcmp (szExt, "SAM") == 0) sType = BLOB_SOUND;
                        if (strcmp (szExt, "MID") == 0) sType = BLOB_MIDI;

                        if (sType == FAILURE) return (IE_WRONGFORMAT);

                        buffer  += fd->addr;
                        lbuffer  = (LONG *)buffer;

                        db_fieldinfo (base, table, fd->field, &field_info);
                        lSize = file_length (szFileName);
                        if (lSize > (LONG)(field_info.size - 2 * sizeof (LONG))) return (IE_FIELD2SMALL);	/* size & type of picture in first two long words */

                        hFile = file_open (szFileName, O_RDONLY);
                        if (hFile < 0) return (IE_FILEOPEN);

                        lbuffer [0] = lSize + sizeof (LONG);		/* add type of picture */
                        lbuffer [1] = sType;

                        if (file_read (hFile, lSize, &lbuffer [2]) != lSize)
                        {
                          file_close (hFile);
                          return (IE_FILEREAD);
                        } /* if */

                        file_close (hFile);

                        break;
    case TYPE_BLOB    : file_split (szFileName, NULL, szPath, szName, szExt);

                        blob  = (HPBLOB)(buffer + fd->addr);
                        lSize = file_length (szFileName);
                        if (lSize == 0) return (SUCCESS);	/* nothing to do */

                        blob->size     = lSize;
                        blob->name [0] = EOS;

                        strncpy (blob->ext, szExt, MAX_BLOBEXT);
                        blob->ext [MAX_BLOBEXT] = EOS;

                        sFormat = BlobFromExt (blob->ext);
                        if (sFormat == FAILURE) return (IE_WRONGFORMAT);		/* error BLOB cannot be interpreted */

                        strcpy (blob->name, BlobSpec [sFormat].szName);

                        sysblob = mem_alloc (lSize + sizeof (SYSBLOB));
                        if (sysblob == NULL) return (IE_OUTOFMEMORY);

                        sysblob->address = blob->address;
                        sysblob->size    = lSize;
                        sysblob->flags   = (sFormat == BLOB_OBJECT) ? (USHORT)BLOB_OLE : 0;

                        strcpy (sysblob->ext,  blob->ext);
                        strcpy (sysblob->name, blob->name);

                        hFile = file_open (szFileName, O_RDONLY);
                        if (hFile < 0) return (IE_FILEOPEN);

                        if (file_read (hFile, lSize, sysblob->blob) != lSize)
                        {
                          file_close (hFile);
                          mem_free (sysblob);
                          return (IE_FILEREAD);
                        } /* if */

                        if (sFormat == BLOB_WMF)
                        {
                          pMetaHeader         = (METAHEADER *)((LPSTR)sysblob->blob + sizeof (WMFHEADER));
                          pMetaHeader->mtSize = (lSize - sizeof (METAHEADER) - sizeof (WMFHEADER)) / sizeof (USHORT);	/* size of metafile in USHORTs */
                        } /* if */

                        if (sysblob->address != 0)
                          bOk = db_update (base, SYS_BLOB, sysblob, &sStatus);
                        else
                          bOk = db_insert (base, SYS_BLOB, sysblob, &sStatus);

                        blob->address = bOk ? sysblob->address : 0;
                        blob->size    = sysblob->size;

                        file_close (hFile);
                        mem_free (sysblob);

                        if (! bOk) return (IE_BLOBINSERT);
                        break;
    default           : return (IE_WRONGFORMAT);
  } /* switch */

  return (SUCCESS);
} /* DoBlobImport */

/*****  Export file into field of type blob     ****************************/

LOCAL SHORT DoBlobExport (BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp)
{
  SHORT    sResult, sType;
  LONG     lSize;
  FHANDLE  hFile;
  FULLNAME szFileName, szPath, szName;
  EXT      szExt;
  HPBLOB   blob;
  LONG     *lbuffer;
  SYSBLOB  *sysblob;

  sResult = SUCCESS;

  strcpy (szFileName, sp [0].val.stringVal);

  switch (fd->type)
  {
    case TYPE_VARBYTE :
    case TYPE_VARWORD :
    case TYPE_VARLONG :
    case TYPE_PICTURE : buffer  += fd->addr;
                        lbuffer  = (LONG *)buffer;
                        lSize    = lbuffer [0] - sizeof (LONG);		/* subtract type of picture */
                        sType    = (SHORT)lbuffer [1];

                        file_split (szFileName, NULL, szPath, szName, szExt);
                        if (fd->type == TYPE_PICTURE)
                          ExtFromPicType (sType, szExt);
                        else
                          ExtFromVarType (sType, szExt);

                        sprintf (szFileName, "%s%s.%s", szPath, szName, szExt);   /* append correct suffix */

                        hFile = file_create (szFileName);
                        if (hFile >= 0)
                        {
                          if (file_write (hFile, lSize, &lbuffer [2]) != lSize) sResult = IE_FILEWRITE;
                          file_close (hFile);
                        } /* if */
                        else
                          sResult = IE_FILECREATE;

                        break;
    case TYPE_BLOB    : blob = (HPBLOB)(buffer + fd->addr);
                        if ((blob->address == 0) || (blob->size == 0)) return (SUCCESS);

                        sysblob = db_readblob (base, NULL, NULL, blob->address, FALSE);
                        if (sysblob == NULL) return (IE_BLOBREAD);

                        file_split (szFileName, NULL, szPath, szName, szExt);
                        sprintf (szFileName, "%s%s.%s", szPath, szName, blob->ext);   /* append correct suffix */

                        hFile = file_create (szFileName);
                        if (hFile >= 0)
                        {
                          if (file_write (hFile, sysblob->size, sysblob->blob) != sysblob->size) sResult = IE_FILEWRITE;
                          file_close (hFile);
                        } /* if */
                        else
                          sResult = IE_FILECREATE;

                        mem_free (sysblob);
                        break;
    default           : return (IE_WRONGFORMAT);
  } /* switch */

  return (sResult);
} /* DoBlobExport */

/*****************************************************************************/

LOCAL SHORT DoTextImport (BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp)
{
  SHORT      sResult;
  LONG       lSize;
  FULLNAME   szFileName;
  STRING     sz;
  HFILE      hFile;
  HPCHAR     pDbBuffer;
  FIELD_INFO field_info;

  strcpy (szFileName, sp [0].val.stringVal);
  pDbBuffer = buffer;

  if (! file_exist (szFileName)) return (IE_FILEEXIST);

  db_fieldinfo (base, table, fd->field, &field_info);

  switch (field_info.type)
  {
    case TYPE_CHAR   :
    case TYPE_CFLOAT :
    case TYPE_EXTERN : buffer += field_info.addr;
		       break;
    default          : buffer          = sz;
		       field_info.size = MAX_LONGSTRSIZE;
		       break;
  } /* switch */

  sResult = SUCCESS;
  lSize   = min (file_length (szFileName), field_info.size - 1);

  hFile = file_open (szFileName, O_RDONLY);
  if (hFile >= 0)
  {
    if (file_read (hFile, lSize, buffer) == lSize)
    {
      buffer [lSize] = EOS;

      switch (field_info.type)
      {
	case TYPE_CHAR   :
	case TYPE_CFLOAT :
	case TYPE_EXTERN : break;
	default          : if (format != NULL)
			     format2str (field_info.type, sz, format [field_info.type]);
			   str2bin (field_info.type, sz, pDbBuffer + field_info.addr);
			   break;
      } /* switch */
    } /* if */
    else
      sResult = IE_FILEREAD;

    file_close (hFile);
  } /* if */
  else
    sResult = IE_FILEOPEN;

  return (sResult);
} /* DoTextImport */

/*****  Export file into field of type blob     ****************************/

LOCAL SHORT DoTextExport (BASE *base, SHORT table, HPCHAR buffer, FIELDDESC *fd, VALUEDESC *sp)
{
  SHORT      sResult;
  LONG       lSize;
  LONGSTR    sz;
  FHANDLE    hFile;
  FULLNAME   szFileName;
  HPCHAR     pDbBuffer;
  FIELD_INFO field_info;

  strcpy (szFileName, sp [0].val.stringVal);
  pDbBuffer = buffer;
  sz [0]    = EOS;
  sResult   = SUCCESS;

  db_fieldinfo (base, table, fd->field, &field_info);

  switch (field_info.type)
  {
    case TYPE_CHAR   :
    case TYPE_CFLOAT :
    case TYPE_EXTERN : buffer += field_info.addr;
		       break;
    default          : buffer = sz;
		       if (! is_null (field_info.type, pDbBuffer + field_info.addr))
			 bin2str (field_info.type, pDbBuffer + field_info.addr, buffer);
		       if (format != NULL)
			 str2format (field_info.type, buffer, format [field_info.type]);
		       RmLeadingBlanks (buffer);
		       break;
  } /* switch */

  lSize = strlen (buffer);

  hFile = file_create (szFileName);
  if (hFile >= 0)
  {
    if (file_write (hFile, lSize, buffer) != lSize) sResult = IE_FILEWRITE;
    file_close (hFile);
  } /* if */
  else
    sResult = IE_FILECREATE;

  return (sResult);
} /* DoTextExport */

/*****************************************************************************/

LOCAL SHORT BlobFromExt (CHAR *pExt)
{
  SHORT i;

  for (i = 0; i < MAX_BLOBTYPE; i++)
    if (strcmpi (pExt, BlobSpec [i].szExt) == 0) return (i);

  return (FAILURE);
} /* BlobFromExt */

/*****************************************************************************/

LOCAL VOID ExtFromPicType (SHORT sType, CHAR *pExt)
{
  switch (sType)
  {
    case PIC_META  : strcpy (pExt, "GEM"); break;
    case PIC_IMAGE : strcpy (pExt, "IMG"); break;
    case PIC_IFF   : strcpy (pExt, "IFF"); break;
    case PIC_TIFF  : strcpy (pExt, "TIF"); break;
    default        : strcpy (pExt, "___"); break;
  } /* switch */
} /* ExtFromPicType */

/*****************************************************************************/

LOCAL VOID ExtFromVarType (SHORT sType, CHAR *pExt)
{
  switch (sType)
  {
    case BLOB_UNKNOWN : strcpy (pExt, "___"); break;
    case BLOB_SOUND   : strcpy (pExt, "SAM"); break;
    case BLOB_MIDI    : strcpy (pExt, "MID"); break;
    default           : strcpy (pExt, "___"); break;
  } /* switch */
} /* ExtFromVarType */

/*****************************************************************************/

LOCAL LPSTR RmLeadingBlanks (LPSTR s)
{
  CHAR *src, *dst;

  src = dst = s;

  while (*src == ' ') src++;

  strcpy (dst, src);

  return (s);
} /* RmLeadingBlanks */

/*****  Read KBV smart card	****************************/

#ifdef _WINDOWS

#ifdef WIN32
BOOL WINAPI ReadCard (UCHAR *pChipIn)	/* read KBV smart card (in special KEYBOARD.DRV) */
{
  return (FALSE);
} /* ReadCard */
#endif

LOCAL SHORT KbvReadCard ()
{
  BOOL      ok;
  SHORT     i;
  HANDLE    hChip_in;
  HINSTANCE hInstDriver;
  UCHAR     ucXorTotal;
  UCHAR     *chip_in;

  hInstDriver = LoadLibrary ("KEYBOARD");
  if ((UINT)hInstDriver <= (UINT)HINSTANCE_ERROR) return (1);

  ok = GetProcAddress (hInstDriver, "READCARD") != NULL;

  FreeLibrary (hInstDriver);

  if (! ok) return (2);

  if ((hChip_in = GlobalAlloc (GMEM_SHARE, KBV_MAX)) == NULL) return (3);

  if ((chip_in = GlobalLock (hChip_in)) == NULL)
  {
    GlobalFree (hChip_in);
    return (4);
  } /* if */

  for (i = 30; i < KBV_MAX; chip_in [i++] = ' ');	/* clear buffer */

  KbvFailure = 0;									/* no error */
  if (! ReadCard (chip_in)) KbvFailure = 5;			/* read smartcard data into chip_in buffer */

  ucXorTotal = 0;

  if (KbvFailure == 0) ucXorTotal = KbvCalcChecksum (chip_in);
  KbvTransform (chip_in);

  if ((ucXorTotal == 0) && (KbvFailure == 0))
    KbvSortChipData (chip_in);
  else
    KbvFailure = 6;

  GlobalUnlock (hChip_in);
  GlobalFree (hChip_in);

  return (KbvFailure);
} /* KbvReadCard */

/*****  Get field of KBV smart card	****************************/

LOCAL VOID KbvGet (LPSTR pFieldName)
{
  SHORT   i;
  LONGSTR s, szFieldName;

  strcpy (szFieldName, pFieldName);
  str_upper (szFieldName);
  pFieldName [0] = EOS;

  for (i = 0; i < KBV_FIELDS; i++)
  {
    strcpy (s, KbvInfo [i].pFieldName);
    str_upper (s);
    if (strcmp (s, szFieldName) == 0)
    {
      strcpy (pFieldName, (CHAR *)KbvInfo [i].pData);
      return;
    } /* if */
  } /* for */
} /* KbvGet */

/*****************************************************************************/

LOCAL UCHAR KbvCalcChecksum (UCHAR *chip_in)
{
  SHORT i;
  UCHAR xor_total;

  xor_total = 0;

  for (i = 30; i < KBV_MAX; i++)
  {
    if ((chip_in [i] == 0xc0) && (i >= (30 + 50))) break;

    /* calculate xor_total = xor_total XOR chip_in [i] */
    xor_total = (UCHAR)((xor_total & (~chip_in [i])) | ((~xor_total) & chip_in [i]));
  } /* for */

  return (xor_total);
} /* KbvCalcChecksum */

/*****************************************************************************/
  
LOCAL VOID KbvTransform (UCHAR *chip_in)
{
  SHORT i;
  
  for (i = 33; i < KBV_MAX; i++)
  {
    if (chip_in [i] == 0x8e) break;		/* only transform upto checksum */
    else								/* Transform from ASCII 7 bit to OEM charset */
    {
      if (chip_in [i] ==  91) chip_in [i] = 0x8e;	/* German ƒ */
      if (chip_in [i] ==  92) chip_in [i] = 0x99;	/* German ÷ */
      if (chip_in [i] ==  93) chip_in [i] = 0x9a;	/* German ‹ */
      if (chip_in [i] == 123) chip_in [i] = 0x84;	/* German ‰ */
      if (chip_in [i] == 124) chip_in [i] = 0x94;	/* German ˆ */
      if (chip_in [i] == 125) chip_in [i] = 0x81;	/* German ¸ */
      if (chip_in [i] == 126) chip_in [i] = 0xe1;	/* German ﬂ */
      
      OemToAnsiBuff ((LPSTR)&chip_in [i], (LPSTR)&chip_in [i], 1);
	} /* else */
  } /* for */
} /* KbvTransform */

/*****  Sort data of KBV smart card	****************************/

LOCAL VOID KbvSortChipData (UCHAR *chip_in)
{
  SHORT cinx, i;

  cinx = 30;

  if (chip_in [cinx++] != 0x60) KbvFailure = 0x60;
  if (chip_in [cinx] == 0x81) cinx++;

  cinx++;

  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.KrankenKassenName, 0x80);

  if (KbvFailure == 0x80)		/* if error check next byte for correct code */
  {
    KbvFailure = 0;
    cinx++;
    cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.KrankenKassenName,0x80);
  } /* if */

  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.KrankenKassenNummer, 0x81);
  cinx = KbvGetData (chip_in, cinx, KbvCard.VKNR, 0x8f);
  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.VersichertenNummer, 0x82);
  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.VersichertenStatus, 0x83);
  cinx = KbvGetData (chip_in, cinx, KbvCard.StatusErgaenzung, 0x90);
  cinx = KbvGetData (chip_in, cinx, KbvCard.Titel, 0x84);
  cinx = KbvGetData (chip_in, cinx, KbvCard.VorName, 0x85);
  cinx = KbvGetData (chip_in, cinx, KbvCard.Namenszusatz, 0x86);
  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.FamilienName, 0x87);
  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.Geburtsdatum, 0x88);
  cinx = KbvGetData (chip_in, cinx, KbvCard.StrassenName, 0x89);
  cinx = KbvGetData (chip_in, cinx, KbvCard.WohnsitzLaenderCode, 0x8a);
  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.Postleitzahl, 0x8b);
  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.OrtsName, 0x8c);
  cinx = KbvGetData (chip_in, cinx, KbvCard.GueltigkeitsDatum, 0x8d);

  cinx = KbvGetDataOrError (chip_in, cinx, KbvCard.PruefSumme, 0x8e);
  i    = KbvCard.PruefSumme [0];
  if (KbvFailure == 0) sprintf ((CHAR *)KbvCard.PruefSumme, "%u", i);
} /* KbvSortChipData */

/*****************************************************************************/
  
LOCAL SHORT KbvGetDataOrError (UCHAR *chip_in, SHORT cinx, UCHAR *dest, UCHAR header)
{
  int e, b;

  dest [0] = EOS;

  if (KbvFailure == 0)
  {
    if (chip_in [cinx++] != header) KbvFailure = header;
	e = chip_in [cinx++];

	for (b = 0; b < e; dest [b++] = chip_in [cinx++]);

	dest [b] = EOS;
  } /* if */

  return (cinx);
} /* KbvGetDataOrError */

/*****************************************************************************/
  
LOCAL SHORT KbvGetData (UCHAR *chip_in, SHORT cinx, UCHAR *dest, UCHAR header)
{
  int e, b;

  dest [0] = EOS;

  if ((chip_in [cinx] == header) && (KbvFailure==0))
  {
	cinx++;
    e = chip_in [cinx++];

	for (b = 0; b < e; dest [b++] = chip_in [cinx++]);

	dest [b] = EOS;
  } /* if */

  return (cinx);
} /* KbvGetData */

/*****************************************************************************/
  
#endif

/*****  Get an entry in the string heap ****************************/

LOCAL   CHAR    *AllocStrSlot   ()
{
	USHORT  slotNum, slotMask;
	CHAR    *res;

	if      (slotSet == 0)
	{
		CalcError(SSP_OVERFLOW);
		return( strHeap[0] );
	}
	else
	{
		slotNum  = 0;
		slotMask = 1;
		while   ((slotMask & slotSet) == 0)
		{
			slotNum++;
			slotMask <<= 1;
		}
		slotSet &= ~slotMask;
		res = strHeap[slotNum];
		res[0] = 0;
		return( res );
	}
}


/*****  Mark free entries in the string heap    ********************/

LOCAL   VOID    MarkFreeSlots   (top)
VALUEDESC       *top;
{
	VALUEDESC       *sp;
	unsigned long   offs;

	slotSet = (USHORT)(1<<MAX_SLOT) - 1;
	for     (sp = stack; sp <= top; sp++)
	{
		if      (HASWILD (sp->type))
		{
			offs = sp->val.stringVal - strHeap[0];
			if      (offs < MAX_STRING)
				slotSet &= ~(1<<((USHORT)offs/SLOT_SIZE));
		}
	}
}


/*****  Main routine of interpreter.    ****************************/

GLOBAL INT WINAPI db_execute (base, table, calccode, buffer, count, form, retval)
LPBASE      base;
SHORT       table;
LPCALCCODE  calccode;
HPVOID      buffer;
LONG        count;
LPFORMAT    form;
LPVALUEDESC retval;
{
	INT             error;

	if (calccode->size == 0)
		return (SUCCESS);

	if      ((error = setjmp(errJmp)) != 0)
		return(error);

	calcCount = count;
	format    = form;

	Interpret (base, table, calccode, buffer, retval);

	return (SUCCESS);
}

/* ############################################################### */
