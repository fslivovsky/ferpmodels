#ifndef ADMIN_H
#define ADMIN_H

#define NEWN(P,N) \
  do { \
    size_t NEWN_BYTES = (N) * sizeof *(P); \
    (P) = malloc (NEWN_BYTES); \
    if (!(P)) die ("out of memory"); \
    assert (P); \
    memset ((P), 0, NEWN_BYTES); \
  } while (0)
#define DELN(P,N) \
  do { \
    free (P); \
  } while (0)
#define RSZ(P,M,N) \
  do { \
    size_t RSZ_OLD_BYTES = (M) * sizeof *(P); \
    size_t RSZ_NEW_BYTES = (N) * sizeof *(P); \
    (P) = realloc (P, RSZ_NEW_BYTES); \
    if (RSZ_OLD_BYTES > 0 && !(P)) die ("out of memory"); \
    assert (P); \
    if (RSZ_NEW_BYTES > RSZ_OLD_BYTES) { \
      size_t RSZ_INC_BYTES = RSZ_NEW_BYTES - RSZ_OLD_BYTES; \
      memset (RSZ_OLD_BYTES + ((char*)(P)), 0, RSZ_INC_BYTES); \
    } \
  } while (0)
#define NEW(P) NEWN((P),1)
#define DEL(P) DELN((P),1)

void die (const char *, ...); 

int num_lits, size_lits, * lits;
void push_literal (int); 
void release_lits (); 

#endif 
