#include "import.h"
#include "conv.h"
#include "files.h"
#include "utility.h"

#include "export.h"

typedef struct
{
  LONG next;                    /* next page number for link */
  LONG prev;                    /* previous page number for link */
  WORD size;                    /* size of used area of page */
  WORD num_keys;                /* number of keys in page */
  LONG pointer;                 /* number of left/right page */
} KEY_PAGE;

/*****************************************************************************/

GLOBAL INT main (argc, argv)
INT  argc;
BYTE *argv [];

{
  FHANDLE  handle;
  FULLNAME filename;
  BYTE     buffer [512], c;
  LONG     blk, size, blk2, size2;
  KEY_PAGE *kp;
  INT      verbose;

  if (argc == 1)
  {
    printf ("file:");
    gets (filename);
  } /* if */
  else
    strcpy (filename, argv [1]);

  strcat (filename, ".ind");

  verbose = FALSE;
  if (argc >= 3) sscanf (argv [2], "%d", &verbose);

  handle = file_open (filename, O_RDONLY);

  if (handle < 0) return (1);

  file_seek (handle, 512L, SEEK_SET);

  kp   = (KEY_PAGE *)buffer;
  blk  = blk2  = 1;
  size = size2 = 0;

  if (verbose) printf ("|block| next| prev|size|keys|\n");

  while ((file_read (handle, 512L, buffer) == 512L) && (kp->size != 0))
  {
    c = (kp->pointer) == 0 ? '*' : ' ';
    
    if (verbose)
      printf ("|%5ld|%5ld|%5ld|%4d|%4d|%c\n", blk, kp->next, kp->prev, kp->size, kp->num_keys, c);

    blk++;
    size += kp->size;

    if (kp->pointer == 0)
    {
      blk2++;
      size2 += kp->size;
    } /* if */
  } /* while */

  printf ("Gesamt        :%ld von %ld = %ld%%\n", size, (blk - 1) * 512, size * 100 / ((blk - 1) * 512));
  printf ("Gesamt Bl„tter:%ld von %ld = %ld%%\n", size2, (blk2 - 1) * 512, size2 * 100 / ((blk2 - 1) * 512));

  return (0);
} /* main */
