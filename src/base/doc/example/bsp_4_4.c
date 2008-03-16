#include "import.h"
#include "phoenix.h"

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
	SHORT err;
	init_conv (short_month, long_month, '.', ','); 
	db_init (0);
	base = db_create ("TEST", "", 0, 16L, 16L);
	if (base == NULL)
	{ 
		err = db_status (base);
		printf ("Fehler: %d\n", err); 
	} /* if */
	
	/* Datenbank bearbeiten */
	
	if (base != NULL)
		db_close (base);
	return (0);
} /* main */
