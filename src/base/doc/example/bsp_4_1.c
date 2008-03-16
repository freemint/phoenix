#include "import.h"
#include <phoenix.h>

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
	BYTE s [80];
	DATE date;
	TIME time;
	TIMESTAMP timestamp;
	SHORT w = 21;
	LONG l = 123456L;
	DOUBLE d = 1234.56;
	init_conv (short_month, long_month, '.', ',');
	get_date (&date); 
	get_time (&time); 
	get_tstamp (&timestamp);
	bin2str (TYPE_WORD, &w, s); 
	puts (s); 
	bin2str (TYPE_LONG, &l, s); 
	puts (s);
	bin2str (TYPE_FLOAT, &d, s);
	puts (s); 
	bin2str (TYPE_DATE, &date, s);
	puts (s);
	bin2str (TYPE_TIME, &time, s);
	puts (s);
	bin2str (TYPE_TIMESTAMP, &timestamp, s);
	puts (s);
	return (0);
} /* main */
