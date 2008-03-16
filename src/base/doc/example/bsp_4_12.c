#include "import.h"
#include "phoenix.h"

#define TBL_KUNDEN 20 /* Nummer der Kunden-Tabelle */ 
#define MAX_NAME 29 /* Maximale Anzahl von Buchstaben */

typedef CHAR NAMEN [MAX_NAME + 1]; /* Typ fur Vorname und Nachname */

typedef struct /* Struktur fur Kunden */ 
{ 
	LONG address; /* Adresse des Datensatzes */ 
	LONG KuNummer; /* Kundennummer */ 
	NAMEN Vorname; /* Vorname des Kunden */ 
	NAMEN Nachname; /* Nachname des Kunden */ 
} KUNDEN;

LOCAL CHAR *short_month [12] = 
{ 
	"Jan", "Feb", "Mar", "Apr", "Mai", "Jun", 
	"Jul", "Aug", "Sep", "Okt", "Nov", "Dez" 
};

LOCAL CHAR *long_month [12] = 
{ 
	"Januar", "Februar", "Marz", "April", "Mai", "Juni", "Juli", 
	"August", "September", "Oktober", "November", "Dezember" 
};

INT main ()
{ 
	BASE *base; 
	CURSOR *cursor; 
	SHORT err; 
	KUNDEN kunde;
	init_conv (short_month, long_month, '.', ','); 
	db_init (0);
	base = db_open ("KUNDEN", "", BASE_FLUSH, 16L, 20,"Geiss", "XYZ");
	if (base == NULL) 
	{ 
		err = db_status (base); 
		printf ("Fehler: %d\n", err); 
		return (0); 
	} /* if */
	cursor = db_newcursor (base);
	if (cursor != NULL) 
	{ 
		if (db_initcursor (base, TBL_KUNDEN, 1, ASCENDING, cursor)) 
			while (db_movecursor (base, cursor, 1L)) 
				if (db_read (base, TBL_KUNDEN, &kunde, cursor, 0L, FALSE)) 
				{ 
					printf ("%5ld|%-30s|%-30s",kunde.KuNummer,kunde.Vorname,kunde.Nachname); 
				} /* if */ 
				else 
					printf ("Fehler beim Lesen des Satzes!\n"); 
	} /* if */ 
	else 
		printf ("Kein Cursor mehr frei!\n");
	if (base != NULL) 
		db_close (base);
	return (0); 
} /* main */
