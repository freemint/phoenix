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
{ /* wie oben */ 
	BYTE s [80];
	DATE date;
	TIME time;
	TIMESTAMP timestamp;
	SHORT w = 21;
	LONG l = 123456L;
	DOUBLE d = 1234.56;
	FORMAT format;
	init_conv (short_month, long_month, '.', ',');
	get_date (&date); 
	get_time (&time); 
	get_tstamp (&timestamp);
	build_format (TYPE_WORD, "99999", format); 
	bin2str (TYPE_WORD, &w, s); 
	str2format (TYPE_WORD, s, format); 
	puts (s);
	build_format (TYPE_LONG, "000-000-000", format);
	bin2str (TYPE_LONG, &l, s); 
	str2format (TYPE_LONG, s, format); 
	puts (s);
	build_format (TYPE_FLOAT, "DM *,***,**0.00", format);
	bin2str (TYPE_FLOAT, &d, s);
	str2format (TYPE_FLOAT, s, format);
	puts (s);
	build_format (TYPE_DATE, "DD.MM.YY", format);
	bin2str (TYPE_DATE, &date, s); 
	str2format (TYPE_DATE, s, format); 
	puts (s);
	build_format (TYPE_TIME, "HH:MI am", format);
	bin2str (TYPE_TIME, &time, s);
	str2format (TYPE_TIME, s, format); 
	puts (s);
	build_format (TYPE_TIMESTAMP, "DD. MONTH YYYY HH:MI:SS.mmmmmm", format);
	bin2str (TYPE_TIMESTAMP, &timestamp, s);
	str2format (TYPE_TIMESTAMP, s, format);
	puts (s);
	return (0); 
}
