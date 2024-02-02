
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admin.h"

int num_lits, size_lits, * lits;

void die (const char * fmt, ...) {
  va_list ap;
  fputs ("*** ferpcert: ", stderr);
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  fflush (stderr);
  exit (1);
}


static void enlarge_lits (void) {
  int new_size_lits = size_lits ? 2*size_lits : 1;
  RSZ (lits, size_lits, new_size_lits);
  size_lits = new_size_lits;
}



void release_lits () {

  DELN (lits, size_lits);
  size_lits = num_lits = 0;
  lits = NULL;

}

void push_literal (int lit) {
  if (size_lits == num_lits) enlarge_lits ();
  lits[num_lits++] = lit;
}
