/*****************************************************************************
 *
 * Module : DBSCAN.C
 * Author : Purex & JÅrgen Geiû
 *
 * Creation date    : 01.04.91
 * Last modification: $Id$
 *
 *
 * Description: This module implements the database scan routines.
 *
 * History:
 * 19.01.93: Function isalnum replaced by IsCharAlphaNumeric
 * 18.11.92: Packing pragma added
 * 25.10.92: Module header added
 * 01.04.89: Creation of body
 *****************************************************************************/

#include	<ctype.h>
#include	<string.h>

#include	"import.h"

#include	"conv.h"
#include	"files.h"
#include	"utility.h"

#include	"dbroot.h"
#include	"dbdata.h"
#include	"dbtree.h"
#include	"dbcall.h"

#include	"utility.h"

#include	"export.h"
#include	"dbscan.h"


LOCAL	CHAR	*text;
LOCAL	CHAR	unGetCh;

#if MSDOS | NT
#pragma pack(1)
#endif

typedef	struct
{
	SHORT	tok;
	CHAR	*name;
}	KEYWORD;


#if MSDOS | NT
#pragma pack()
#endif

LOCAL	KEYWORD		keyWordTab[] =
{
	{	TK_IF,			"IF"		},
	{	TK_THEN,		"THEN"		},
	{	TK_ELSE,		"ELSE" 		},
	{	TK_END,			"END" 		},
	{	TK_WHILE,		"WHILE"		},
	{	TK_DO,			"DO" 		},
	{	TK_REPEAT,		"REPEAT" 	},
	{	TK_UNTIL,		"UNTIL" 	},
	{	TK_VAR,			"VAR"	 	},
	{	TK_EXIT,		"EXIT"		},
	{	TK_IDENTIFIER,	NULL 		},
};


LOCAL	CHAR	GetChar		_((VOID));
LOCAL	VOID	UnGetChar	_((CHAR ch));
LOCAL	SHORT		KeyWord		_((VOID));


/*****	Initialize the scanner	************************************/

GLOBAL	VOID	StartScan(t)
CHAR	*t;
{
	text = t;
	token.line = 0;
	token.col = 0;
	unGetCh = 0;
}


/*****	Get the next character	************************************/

LOCAL	CHAR	GetChar()
{
	CHAR	ch;
	BOOL	lf;

	lf = FALSE;
	ch = unGetCh;
	unGetCh = 0;
	if	(ch)
		return(ch);

	ch = *text++;
	while	(ch == '\r' || ch == '\n')
	{
		if	(ch == '\n')
		{
			token.line++;
			token.col = 0;
			lf = TRUE;
		}
		ch = *text++;
	}

	if	(lf)
		{
			ch = ' ';
			text--;
		}

 	if	(ch == EOS)
		text--;
	else
		token.col++;
	return(ch);
}


/*****	Put character back		************************************/

LOCAL	VOID	UnGetChar(ch)
CHAR	ch;
{
	unGetCh = ch;
}


/*****	Check whether token.str	is a keyword	********************/

LOCAL	SHORT		KeyWord()
{
	KEYWORD		*key;
	CHAR		upperName[30];

	strncpy(upperName, token.str, sizeof(upperName)-1);
	upperName[sizeof(upperName)-1] = EOS;
	str_upper(upperName);

	for	(key = keyWordTab; key->name != NULL; key++)
		if	(strcmp(key->name, upperName) == 0)
			break;

	return(key->tok);
}


/*****	The scanner		********************************************/

GLOBAL	TOK		Scan()
{
	CHAR		ch, del;
	SHORT		len;

	do
		ch = GetChar();
	while	(isspace(ch));

	switch	(ch)
	{
	case	EOS:	token.tok = TK_EOI;		break;
	case	'+':	token.tok = TK_PLUS;	break;
	case	'-':	token.tok = TK_MINUS;	break;
	case	'*':	token.tok = TK_TIMES;	break;
	case	'/':	token.tok = TK_SLASH;	break;
	case	'(':	token.tok = TK_LPAREN;	break;
	case	')':	token.tok = TK_RPAREN;	break;
	case	';':	token.tok = TK_SEMIC;	break;
	case	',':	token.tok = TK_COMMA;	break;
	case	'&':	token.tok = TK_AND;		break;
	case	'|':	token.tok = TK_OR;		break;
	case	'~':	token.tok = TK_NOT;		break;
	case	'!':	token.tok = TK_NOT;		break;
	case	':':	token.tok = TK_COLON;	break;

	case	'=':
		ch = GetChar();
		if	(ch == '=')
			token.tok = TK_EQ;
		else
		{
			token.tok = TK_BECOMES;
			UnGetChar(ch);
		}
		break;

	case	'<':
		ch = GetChar();
		if	(ch == '=')
			token.tok = TK_LE;
		else if	(ch == '>')
			token.tok = TK_NE;
		else
		{
			token.tok = TK_LT;
			UnGetChar(ch);
		}
		break;

	case	'>':
		ch = GetChar();
		if	(ch == '=')
			token.tok = TK_GE;
		else
		{
			token.tok = TK_GT;
			UnGetChar(ch);
		}
		break;

	case	'\'':
	case	'`':
	case	'"':
		len = 0;
		del = ch;
		ch = GetChar();

		while	(ch != del && ch != EOS)
		{
			if	(len < MAXSTR)
				token.str[len++] = ch;
			ch = GetChar();
		}

		token.str[len] = EOS;
		token.tok = TK_STRING;
		break;

	case	'0':
	case	'1':
	case	'2':
	case	'3':
	case	'4':
	case	'5':
	case	'6':
	case	'7':
	case	'8':
	case	'9':
		len = 0;
		while	(isdigit(ch) || ch == '.' /* || ch == ','*/)
		{
			if	(len < MAXSTR)
				token.str[len++] = ch;
			ch = GetChar();
		}
		UnGetChar(ch);

		token.str[len] = EOS;
		token.tok = TK_CONST;
		break;

	default:
		len = 0;
#ifdef _WINDOWS
		while	(IsCharAlphaNumeric(ch) || ch == '_')
#else
		while	(isalnum(ch_ascii(ch)) || ch == '_')
#endif
		{
			if	(len < MAXSTR)
				token.str[len++] = ch;
			ch = GetChar();
		}
		UnGetChar(ch);

		token.str[len] = EOS;
		token.tok = KeyWord();
		if	(len == 0)
			token.tok = TK_OTHER;
		break;
	}
	return( token.tok );
}


/*****	Get a number composed of arbitrary chars in parentheses	****/

GLOBAL	VOID	ScanNum()
{
	SHORT	len;
	CHAR	ch;

	len = 0;

	ch = GetChar();
	while (ch != ')' && ch != EOS)
	{
		if	(len < MAXSTR)
			token.str[len++] = ch;
		ch = GetChar();
	}
	UnGetChar(ch);
	token.str[len] = EOS;
}
