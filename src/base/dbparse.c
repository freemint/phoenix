/*****************************************************************************
 *
 * Module : DBPARSE.C
 * Author : Purex & JÅrgen & Dieter Geiû
 *
 * Creation date    : 01.04.91
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database parser routines.
 *
 * History:
 * 04.07.95: RETURN added
 * 02.11.94: KBVGET added
 * 18.10.94: KBVREAD added
 * 07.06.94: IMPORT, EXPORT added
 * 18.11.93: Error in function Assignment removed (was caused by change of 27.09.93)
 * 27.09.93: DAY, MONTH, YEAR, ..., MICROSECOND no longer needs field names as operands
 * 26.06.93: SrandStatement definition removed
 * 06.05.93: ERRNO added
 * 28.04.93: RANDOM, POW, ROUND, TRUNC, SIGN, KEYCOUNT, STRLOWER, STRUPPER, CHAR, WORD, LONG, FLOAT added
 * 27.04.93: SIN, COS, TAN, ASIN, ACOS, ATAN, ABS, EXP, LN, LOG, SQRT added
 * 26.04.93: LENGTH, POSITION added
 * 18.01.93: Function USER added
 * 18.11.92: Packing pragma added
 * 04.11.92: Modifications for WIN32 added
 * 31.10.92: Far pointer definitions for DLL implementation added
 * 26.10.92: db_compile has INT result
 * 25.10.92: Module header added
 * 01.04.89: Creation of body
 *****************************************************************************/

#include	<assert.h>
#include	<setjmp.h>
#include	<stdio.h>

#include	"import.h"

#include	"conv.h"
#include	"files.h"
#include	"utility.h"

#include	"dbroot.h"
#include	"dbdata.h"
#include	"dbtree.h"
#include	"dbcall.h"

#include	"dbscan.h"
#include	"dbexec.h"

#include	"export.h"


#define	VARNAMELEN	32
#define	VARCOUNT	64

typedef	enum
{
	PRIO_NULL,
	PRIO_COLON,
	PRIO_OR,
	PRIO_AND,
	PRIO_RELOP,
	PRIO_ADDOP,
	PRIO_MULOP
}	PRIO;


typedef	enum
{
	NEED_NONE,
	NEED_DATE,
	NEED_TIME,
	NEED_COL,
	NEED_OPND,
	NEED_NUM
}	NEED_FIELD;


#if MSDOS | NT
#pragma pack(1)
#endif

typedef	struct
{
	CHAR	*name;
	CHAR	loadInst;
	CHAR	storeInst;
	CHAR	needField;
	CHAR	opndCnt;
	CHAR	type;
}	SPECOP;


#if MSDOS | NT
#pragma pack()
#endif

LOCAL	SPECOP	specialTab[] =
{
	{	"SYSDATE",		O_LSYDATE,	0,			NEED_NONE,	0,	0				},
	{	"SYSTIME",		O_LSYTIME,	0,			NEED_NONE,	0,	0				},
	{	"SYSTIMESTAMP",	O_LSYTIMST,	0,			NEED_NONE,	0,	0				},
	{	"COUNT",		O_LCOUNT,	0,			NEED_NONE,	0,	0				},
	{	"DAY",			O_LDAY,		O_SDAY,		NEED_OPND,	1,	0				},
	{	"MONTH",		O_LMONTH,	O_SMONTH,	NEED_OPND,	1,	0				},
	{	"YEAR",			O_LYEAR,	O_SYEAR,	NEED_OPND,	1,	0				},
	{	"HOUR",			O_LHOUR,	O_SHOUR,	NEED_OPND,	1,	0				},
	{	"MINUTE",		O_LMINUTE,	O_SMINUTE,	NEED_OPND,	1,	0				},
	{	"SECOND",		O_LSECOND,	O_SSECOND,	NEED_OPND,	1,	0				},
	{	"MICROSECOND",	O_LMICSEC,	O_SMICSEC,	NEED_OPND,	1,	0				},
	{	"FIRST",		O_FIRST,	0,			NEED_COL,	0,	0				},
	{	"LAST",			O_LAST,		0,			NEED_COL,	0,	0				},
	{	"REPLACE",		O_REPLACE,	0,			NEED_OPND,	3,	0				},
	{	"SUM",			O_SUM,		0,			NEED_OPND,	1,	0				},
	{	"MIN",			O_MIN,		0,			NEED_OPND,	1,	0				},
	{	"MAX",			O_MAX,		0,			NEED_OPND,	1,	0				},
	{	"AVG",			O_AVG,		0,			NEED_OPND,	1,	0				},
	{	"GET",			O_GET,		0,			NEED_OPND,	1,	0				},
	{	"PUT",			O_PUT,		0,			NEED_OPND,	2,	0				},
	{	"ALERT",		O_ALERT,	0,			NEED_OPND,	1,	0				},
	{	"SUBSTR",		O_SUBSTR,	0,			NEED_OPND,	3,	0				},
	{	"CHR",			O_CHR,		0,			NEED_OPND,	1,	0				},
	{	"LENGTH",		O_LENGTH,	0,			NEED_OPND,	1,	0				},
	{	"POSITION",		O_POSITION,	0,			NEED_OPND,	3,	0				},
	{	"STRLOWER",		O_STRLOWER,	0,			NEED_OPND,	1,	0				},
	{	"STRUPPER",		O_STRUPPER,	0,			NEED_OPND,	1,	0				},
	{	"FIELD",		O_LNAMEDFLD,O_SNAMEDFLD,NEED_OPND,	1,	0				},
	{	"NULL",			O_LNUL,		0,			NEED_NONE,	0,	0				},
	{	"DATE",			O_LCONST,	0,			NEED_NUM,	0,	TYPE_DATE		},
	{	"TIME",			O_LCONST,	0,			NEED_NUM,	0,	TYPE_TIME		},
	{	"TIMESTAMP",	O_LCONST,	0,			NEED_NUM,	0,	TYPE_TIMESTAMP	},
	{	"USER",			O_USER,		0,			NEED_NONE,	0,	0				},
	{	"SIN",			O_SIN,		0,			NEED_OPND,	1,	0				},
	{	"COS",			O_COS,		0,			NEED_OPND,	1,	0				},
	{	"TAN",			O_TAN,		0,			NEED_OPND,	1,	0				},
	{	"ASIN",			O_ASIN,		0,			NEED_OPND,	1,	0				},
	{	"ACOS",			O_ACOS,		0,			NEED_OPND,	1,	0				},
	{	"ATAN",			O_ATAN,		0,			NEED_OPND,	1,	0				},
	{	"ABS",			O_ABS,		0,			NEED_OPND,	1,	0				},
	{	"EXP",			O_EXP,		0,			NEED_OPND,	1,	0				},
	{	"LN",			O_LN,		0,			NEED_OPND,	1,	0				},
	{	"LOG",			O_LOG,		0,			NEED_OPND,	1,	0				},
	{	"SQRT",			O_SQRT,		0,			NEED_OPND,	1,	0				},
	{	"RANDOM",		O_RAND,		0,			NEED_OPND,	1,	0				},
	{	"POW",			O_POW,		0,			NEED_OPND,	2,	0				},
	{	"ROUND",		O_ROUND,	0,			NEED_OPND,	1,	0				},
	{	"TRUNC",		O_TRUNCATE,	0,			NEED_OPND,	1,	0				},
	{	"SIGN",			O_SIGN,		0,			NEED_OPND,	1,	0				},
	{	"KEYCOUNT",		O_KEYCOUNT,	0,			NEED_OPND,	1,	0				},
	{	"CHAR",			O_CHAR,		0,			NEED_OPND,	1,	0				},
	{	"WORD",			O_WORD,		0,			NEED_OPND,	1,	0				},
	{	"LONG",			O_LONG,		0,			NEED_OPND,	1,	0				},
	{	"FLOAT",		O_FLOAT,	0,			NEED_OPND,	1,	0				},
	{	"ERRNO",		O_ERRNO,	0,			NEED_NONE,	0,	0				},
	{	"IMPORT",		O_IMPORT,	0,			NEED_OPND,	2,	0				},
	{	"EXPORT",		O_EXPORT,	0,			NEED_OPND,	2,	0				},
	{	"KBVREAD",		O_KBVREAD,	0,			NEED_NONE,	0,	0				},
	{	"KBVGET",		O_KBVGET,	0,			NEED_OPND,	1,	0				},
	{	"RETURN",		O_RETURN,	0,			NEED_OPND,	1,	0				},
	{	NULL,			0,			0,			0,			0,	0				}
};


LOCAL	SHORT	calc_table;	/* table number for actual calculation */
LOCAL	BASE	*basep;		/* pointer to base */
LOCAL	FORMAT	*format;	/* all formats */
LOCAL	CHAR	varTab[VARCOUNT][VARNAMELEN+1];
LOCAL	SHORT	varCnt;

LOCAL	SHORT	FindVar			_((CHAR *name));
LOCAL	VOID	DeclareVar		_((CHAR *name));
LOCAL	VOID	GetField		_((NEED_FIELD needField, FIELDDESC *fieldDesc));
LOCAL	SPECOP	*FindSpecial	_((CHAR	*name));
LOCAL	VOID	GenSpecialLoad	_((CHAR *name));
LOCAL	OPCODE	GetSpecialStore	_((CHAR *name, FIELDDESC *fieldDesc));
LOCAL	VOID	GenNum			_((SHORT type));
LOCAL	VOID	GetNum			_((SHORT type));
LOCAL	VOID	GetOperands		_((SHORT cnt));

LOCAL	VOID	Factor			_((VOID));
LOCAL	PRIO	PrioOfTok		_((TOK tok));
LOCAL	OPCODE	OpcodeOfTok		_((TOK tok));
LOCAL	VOID	Expr			_((PRIO prio));
LOCAL	VOID	StatementList	_((VOID));
LOCAL	VOID	Assignment		_((VOID));
LOCAL	VOID	IfStatement		_((VOID));
LOCAL	VOID	WhileStatement	_((VOID));
LOCAL	VOID	RepeatStatement	_((VOID));
LOCAL	VOID	VarDeclarations	_((VOID));
LOCAL	VOID	ExitStatement	_((VOID));

/*****	Scan field required for instruction	************************/

LOCAL	VOID	GetField	(needField, fd)
NEED_FIELD	needField;
FIELDDESC	*fd;
{
	if	(token.tok != TK_LPAREN)
		CalcError(NO_LPARENT);
	else
	{
		Scan();
		if	(token.tok != TK_IDENTIFIER)
			CalcError(NO_FIELD);
		else
		{
			LookupField(basep, calc_table, token.str, fd);
			if	(fd->field == FAILURE)
				CalcError(NO_FIELD);
			else
			{
				switch	(needField)
				{
				case	NEED_DATE:
					if	(fd->type != TYPE_DATE && fd->type != TYPE_TIMESTAMP)
						CalcError(NO_TYPE);
					break;

				case	NEED_TIME:
					if	(fd->type != TYPE_TIME && fd->type != TYPE_TIMESTAMP)
						CalcError(NO_TYPE);
					break;

				case	NEED_COL:
					if	((fd->flags & COL_ISINDEX) == 0)
						CalcError(NO_INDEX);
					break;
				}
				Scan();
				if	(token.tok != TK_RPAREN)
					CalcError(NO_RPARENT);
				else
					Scan();
				return;
			}
		}
	}
	fd->field = FAILURE;
}


/*****	Check whether name is one of the predefined functions	****/

LOCAL	SPECOP	*FindSpecial(name)
CHAR	*name;
{
	SPECOP	*spi;
	CHAR	upperName[30];

	strncpy(upperName, name, sizeof(upperName)-1);
	upperName[sizeof(upperName)-1] = EOS;
	str_upper(upperName);

	for	(spi = specialTab; spi->name != NULL; spi++)
		if	(strcmp(spi->name, upperName) == 0)
			return(spi);

	CalcError(UNKNOWN_SYMBOL);
	return(NULL);
}


/*****	Check whether name is one of the declared variables	********/

LOCAL	SHORT	FindVar(name)
CHAR	*name;
{
	SHORT	cnt;
	CHAR	upperName[VARNAMELEN];

	strncpy(upperName, name, VARNAMELEN-1);
	upperName[VARNAMELEN-1] = EOS;
	str_upper(upperName);

	for	(cnt = varCnt; --cnt >= 0; )
		if	(strcmp(upperName, varTab[cnt]) == 0)
			break;

	return(cnt);
}


/*****	Enter name into the variable table	************************/

LOCAL	VOID	DeclareVar(name)
CHAR	*name;
{
	CHAR	*var;

	if	(varCnt >= VARCOUNT)
		CalcError(TOO_MANY_VARS);
	else
	{
		var = varTab[varCnt];
		strncpy(var, name, VARNAMELEN);
		var[VARNAMELEN-1] = EOS;
		str_upper(var);
		varCnt++;
	}
}


/*****	Generate instruction to load number scanned	****************/

LOCAL	VOID	GenNum(type)
SHORT	type;
{
	VALUEDESC	val;

	val.type = type;

	if	(type == TYPE_CHAR)
		val.val.stringVal = token.str;
	else
	{
		if	((format != NULL) && (type != TYPE_FLOAT))
			format2str(type, token.str, format[type]);
		str2bin(type, token.str, &val.val);
	}

	GenConstLoad(&val);
}


/*****	Scan number in parentheses and load		********************/

LOCAL	VOID	GetNum(type)
SHORT	type;
{
	if	(token.tok != TK_LPAREN)
		CalcError(NO_DTCONST);
	else
	{
		ScanNum();
		GenNum(type);
		Scan();
		if	(token.tok != TK_RPAREN)
			CalcError(NO_RPARENT);
		else
			Scan();
	}
}


/*****	Parse a number of operands in parentheses	****************/

LOCAL	VOID	GetOperands(cnt)
SHORT	cnt;
{
	SHORT	i;

	if	(token.tok != TK_LPAREN)
		CalcError(NO_LPARENT);
	else
		Scan();
	for	(i = 0; i < cnt-1; i++)
	{
		Expr(PRIO_NULL);
		if	(token.tok != TK_COMMA)
			CalcError(NO_COMMA);
		else
			Scan();
	}
	Expr(PRIO_NULL);
	if	(token.tok != TK_RPAREN)
		CalcError(NO_RPARENT);
	else
		Scan();
}


/*****	Generate one of the predefined load instructions	********/

LOCAL	VOID	GenSpecialLoad(name)
CHAR		*name;
{
	FIELDDESC	fd;
	SPECOP		*spi;

	spi = FindSpecial(name);
	if	(spi == NULL)
		return;

	Scan();
	switch	(spi->needField)
	{
	case	NEED_NONE:
		GenInst(spi->loadInst, 0);
		break;

	case	NEED_DATE:
	case	NEED_TIME:
	case	NEED_COL:
		GetField(spi->needField, &fd);
		GenFieldInst(spi->loadInst, &fd);
		break;

	case	NEED_NUM:
		GetNum(spi->type);
		break;

	case	NEED_OPND:			/* built in functions, e.g. replace */
		GetOperands(spi->opndCnt);
		GenInst(spi->loadInst, 0);
		break;
	}
}


/*****	Generate one of the predefined store instructions	********/

LOCAL	OPCODE	GetSpecialStore(name, fd)
CHAR		*name;
FIELDDESC	*fd;
{
	SPECOP	*spi;

	spi = FindSpecial(name);
	if	(spi == NULL)
		return(0);

	Scan();
	if	(spi->storeInst)
	{
		if	(spi->needField == NEED_OPND)
			GetOperands(spi->opndCnt);
		else
			GetField(spi->needField, fd);
	}
	else
		CalcError(NO_STATEMENT);
	return(spi->storeInst);
}


/*****	Parse element of expression	********************************/

LOCAL	VOID	Factor()
{
	FIELDDESC	fieldDesc;
	SHORT		varNum;

	switch	(token.tok)
	{
	case	TK_PLUS:
		Scan();
		Factor();
		break;

	case	TK_MINUS:
		Scan();
		Factor();
		GenInst(O_NEG, 0);
		break;

	case	TK_NOT:
		Scan();
		Factor();
		GenInst(O_NOT, 0);
		break;

	case	TK_LPAREN:			/*	"("								*/
		Scan();
		Expr(PRIO_NULL);
		if	(token.tok == TK_RPAREN)
			Scan();
		else
			CalcError(NO_RPARENT);
		break;

	case	TK_CONST:
		GenNum(TYPE_FLOAT);
		Scan();
		break;

	case	TK_STRING:
		GenNum(TYPE_CHAR);
		Scan();
		break;

	case	TK_IDENTIFIER:		/*	a name							*/
		if	((varNum = FindVar(token.str)) >= 0)
		{
			GenInst(O_LOADVAR, varNum);
			Scan();
		}
		else
		{
			LookupField(basep, calc_table, token.str, &fieldDesc);
			if	(fieldDesc.field != FAILURE)
			{
				GenFieldInst(O_LFIELD, &fieldDesc);
				Scan();
			}
			else
				GenSpecialLoad(token.str);
		}
		break;

	default:
		CalcError(NO_FACTOR);
	}
}


/*****	Calculate priority (binding strength) of operator	********/

LOCAL	PRIO	PrioOfTok(tok)
TOK		tok;
{
	switch	(tok)
	{
	default:
		return(PRIO_NULL);

	case	TK_TIMES:
	case	TK_SLASH:
		return(PRIO_MULOP);

	case	TK_PLUS:
	case	TK_MINUS:
		return(PRIO_ADDOP);

	case	TK_EQ:
	case	TK_NE:
	case	TK_GT:
	case	TK_LT:
	case	TK_GE:
	case	TK_LE:
		return(PRIO_RELOP);

	case	TK_AND:
		return(PRIO_AND);

	case	TK_OR:
		return(PRIO_OR);

	case	TK_COLON:
		return(PRIO_COLON);
	}
}


/*****	Calculate opcode to generate for operator	****************/

LOCAL	OPCODE	OpcodeOfTok(tok)
TOK		tok;
{
	switch	(tok)
	{
	case	TK_PLUS:	return(O_ADD);
	case	TK_MINUS:	return(O_SUB);
	case	TK_TIMES:	return(O_MUL);
	case	TK_SLASH:	return(O_DIV);
	case	TK_EQ:		return(O_EQ);
	case	TK_NE:		return(O_NE);
	case	TK_GT:		return(O_GT);
	case	TK_LT:		return(O_LT);
	case	TK_GE:		return(O_GE);
	case	TK_LE:		return(O_LE);
	case	TK_AND:		return(O_AND);
	case	TK_OR:		return(O_OR);
	case	TK_COLON:	return(O_COLON);
	}
	assert(0);
	return(0);
}


/*****	Parse expression	****************************************/

LOCAL	VOID	Expr(prio)
PRIO	prio;
{
TOK		ltok;
PRIO	lprio;

	Factor();
	while	(TRUE)
	{
		ltok = token.tok;
		lprio = PrioOfTok(ltok);
		if	(prio >= lprio)
			break;
		Scan();
		Expr(lprio);
		GenInst(OpcodeOfTok(ltok), 0);
	}
}


/*****	Parse assignment	****************************************/

LOCAL	VOID	Assignment()
{
	FIELDDESC	fieldDesc;
	OPCODE		op;
	SHORT		varNum;

	op = O_END;		/* just an initalization to fool compiler warning */

	if	((varNum = FindVar(token.str)) < 0)
	{
		LookupField(basep, calc_table, token.str, &fieldDesc);
		if	(fieldDesc.field != FAILURE)
		{
			op = O_SFIELD;
			Scan();
		}
		else
		{
			op = GetSpecialStore(token.str, &fieldDesc);
			LookupField(basep, calc_table, token.str, &fieldDesc);	/* JG added */
		}
	}
	else
		Scan();

	if	(token.tok != TK_BECOMES)
		CalcError(NO_BECOMES);
	else
		Scan();

	Expr(PRIO_NULL);

	if	(varNum >= 0)
		GenInst(O_STOREVAR, varNum);
	else if	(op == O_SNAMEDFLD)
		GenInst(op, 0);
	else
		GenFieldInst(op, &fieldDesc);

}


/*****	Parse If-Statement	****************************************/

LOCAL	VOID	IfStatement()
{
	CODELOC		ifLoc, elseLoc;

	Scan();
	Expr(PRIO_NULL);
	ifLoc = GetCurLoc();
	GenFalseJmp(0);
	if	(token.tok != TK_THEN)
		CalcError(THEN_EXPECTED);
	Scan();
	StatementList();
	if	(token.tok == TK_ELSE)
	{
		Scan();
		elseLoc = GetCurLoc();
		GenJmp(0);
		PatchJmp(ifLoc);
		StatementList();
		PatchJmp(elseLoc);
	}
	else
		PatchJmp(ifLoc);
	if	(token.tok != TK_END)
		CalcError(END_EXPECTED);
	Scan();
}


/*****	Parse while-Statement	************************************/

LOCAL	VOID	WhileStatement()
{
	CODELOC		startLoc, condLoc;

	Scan();
	startLoc = GetCurLoc();
	Expr(PRIO_NULL);
	condLoc = GetCurLoc();
	GenFalseJmp(0);
	if	(token.tok != TK_DO)
		CalcError(DO_EXPECTED);
	Scan();
	StatementList();
	GenJmp(startLoc);
	PatchJmp(condLoc);
	if	(token.tok != TK_END)
		CalcError(END_EXPECTED);
	Scan();
}


/*****	Parse Repeat-Statement	************************************/

LOCAL	VOID	RepeatStatement()
{
	CODELOC		startLoc;

	Scan();
	startLoc = GetCurLoc();
	StatementList();
	if	(token.tok != TK_UNTIL)
		CalcError(UNTIL_EXPECTED);
	Scan();
	Expr(PRIO_NULL);
	GenFalseJmp(startLoc);
}


/*****	Parse variable declarations	********************************/

LOCAL	VOID	VarDeclarations()
{
	varCnt = 0;
	while	(token.tok == TK_VAR)
	{
		Scan();
		while	(token.tok == TK_IDENTIFIER)
		{
			DeclareVar(token.str);
			Scan();
			if	(token.tok == TK_COMMA)
				Scan();
			else
				break;
		}
		if	(token.tok != TK_SEMIC)
			CalcError(NO_SEMICOLON);
		else
			Scan();
	}
	if	(varCnt > 0)
		GenInst(O_INITVARS, varCnt);
}


/*****	Parse exit statement	************************************/

LOCAL	VOID	ExitStatement()
{
		GenInst(O_END, 0);
		Scan();
}

/*****	Parse statement list	************************************/

LOCAL	VOID	StatementList()
{
	while	(TRUE)
	{
		switch	(token.tok)
		{
		case	TK_IF:			IfStatement();			break;
		case	TK_WHILE:		WhileStatement();		break;
		case	TK_REPEAT:		RepeatStatement();		break;
		case	TK_IDENTIFIER:	Assignment();			break;
		case	TK_EXIT:		ExitStatement();		break;
		default:				return;
		}
		if	(token.tok != TK_SEMIC)
			CalcError(NO_SEMICOLON);
		else
			Scan();
	}
}


/*****	Main routine of compiler.	********************************/

GLOBAL INT WINAPI db_compile (base, table, syscalc, line, col, form)
LPBASE    base;
SHORT     table;
LPSYSCALC syscalc;
LPSHORT   line;
LPSHORT   col;
LPFORMAT  form;

{
	INT	error;

	calc_table  = table;
	basep       = base;
	format      = form;
	error       = setjmp(errJmp);

	if	(error)
	{
		*line = token.line;
		*col  = token.col;
		return(error);
	}

	StartScan(syscalc->text);
	Scan();

	StartGen(syscalc->code.buffer, sizeof(syscalc->code.buffer));

	VarDeclarations();
	StatementList();
	if	(token.tok != TK_EOI)
		CalcError(NO_STATEMENT);

	GenInst(O_END, 0);

	syscalc->code.size = CodeSize();

	return(SUCCESS);

} /* db_compile */
