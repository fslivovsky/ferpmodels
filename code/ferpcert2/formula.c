
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "admin.h"
#include "formula.h"

Scope *outer_most, *inner_most;
Var *vars;
Clause *clauses, *empty_clause;
int num_vars, num_clauses;
int universal_vars, existential_vars, implicit_vars, orig_clauses;

int order = 1; 
int max_var;
int max_cl = 0;

static int lineno = 1;
static int remaining_clauses_to_parse;
static char * line;
static int nline, szline;
static int remaining; 


static int init_variables (int start) {
  int i;
  assert (0 < start && start <= num_vars);
  for (i = start; i <= num_vars; i++) {
    Var * v = vars + i;
    v->mark = 0;
    v->scope = NULL;
  }
  return i;
}



int parse_qbf (FILE * inFile) {
  int ch, m, n, i, c, q, lit, sign;

  lineno = 1;
  i = c = q = 0;

  szline = 128;
  assert (!line);
  NEWN (line, szline);
  nline = 0;

SKIP:
  ch = getc (inFile);
  if (ch == '\n') { lineno++; goto SKIP; }
  if (ch == ' ' || ch == '\t' || ch == '\r') goto SKIP;
  if (ch == 'c') {
    line[nline = 0] = 0;
    while ((ch = getc (inFile)) != '\n') {
      if (ch == EOF) {
        printf( "end of file in comment");
        return 1;
      }
      if (nline + 1 == szline) {
        RSZ (line, szline, 2*szline);
        szline *= 2;
      }
      line[nline++] = ch;
      line[nline] = 0;
    }
    lineno++;
    goto SKIP;
  }

  if (ch != 'p') {
HERR:
    printf ("invalid or missing header");
    return 1;
  }

 if (getc (inFile) != ' ') goto HERR;
  while ((ch = getc (inFile)) == ' ')
    ;
  if (ch != 'c') goto HERR;
  if (getc (inFile) != 'n') goto HERR;
  if (getc (inFile) != 'f') goto HERR;
  if (getc (inFile) != ' ') goto HERR;
  while ((ch = getc (inFile)) == ' ')
    ;
  if (!isdigit (ch)) goto HERR;
  m = ch - '0';
  while (isdigit (ch = getc (inFile)))
    m = 10 * m + (ch - '0');
  if (ch != ' ') goto HERR;
  while ((ch = getc (inFile)) == ' ')
    ;
  if (!isdigit (ch)) goto HERR;
  n = ch - '0';
  while (isdigit (ch = getc (inFile)))
    n = 10 * n + (ch - '0');
  while (ch != '\n')
    if (ch != ' ' && ch != '\t' && ch != '\r') goto HERR;
    else ch = getc (inFile);
  lineno++;
  remaining = num_vars = m;

  NEWN (vars, num_vars +1);

  init_variables (1);
  remaining_clauses_to_parse = n;


NEXT:
   ch = getc (inFile);
   if (ch == '\n') { lineno++; goto NEXT; }
   if (ch == ' ' || ch == '\t' || ch == '\r') goto NEXT;
   if (ch == 'c') {
     while ((ch = getc (inFile)) != '\n')
       if (ch == EOF) {
         printf ( "end of file in comment");
         return 1;
       }
     lineno++;
     goto NEXT;
   }
   if (ch == EOF) {
     if (i < n) {
       printf( "clauses missing");
       return 1;
     }
     orig_clauses = i;
     //if (!q && !c) ; // todo handle free vars
     goto DONE;
   }
   if (ch == '-') {
     if (q) {
       printf ("negative number in prefix");
       return 1;
     }
     sign = -1;
     ch = getc (inFile);
     if (ch == '0') {
       printf("'-' followed by '0'");
       return 1;
     }
   } else sign = 1;
   if (ch == 'e') {
     if (c) {
       printf("'e' after at least one clause");
       return 1;
     }
     if (q) {
       printf( "'0' missing after 'e'");
       return 1;
     }
     q = 1;
     goto NEXT;
   }
   if (ch == 'a') {
     if (c) {
       printf("'a' after at least one clause");
       return 1;
     }
     if (q) {
       printf ( "'0' missing after 'a'");
       return 1;
     }
     q = -1;
     goto NEXT;
  }
   if (!isdigit (ch)) {
     printf ("expected digit");
     return 1;
   }
   lit = ch - '0';
   while (isdigit (ch = getc (inFile)))
     lit = 10 * lit + (ch - '0');
   if (ch != EOF && ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
     printf ("expected space after literal");
     return 1;
   }
   if (ch == '\n') lineno++;
   if (lit > m) {
     printf("maximum variable index exceeded");
     return 1;
   }
   if (!q && i == n) {
     printf( "too many clauses");
     return 1;
   }
   if (q) {
     if (lit) {
       if (sign < 0) {
         printf ( "negative literal quantified");
         return 1;
       }
       if (lit2scope (lit)) {
            printf ("variable quantified twice");
            return 1;
       }
       lit *= q;
       add_quantifier (lit);
       if (q > 0) existential_vars++; else universal_vars++;
     } else q = 0;
   }
   else {
     if (!q && !c) {
       ; // todo handle free vars
     }
     if (lit) lit *= sign, c++; else i++, remaining_clauses_to_parse--;
     if (lit) push_literal (lit);
     else {
       if (!empty_clause ) {
         ; //add_clause ();
	 num_lits = 0; 
         if (empty_clause) {
           orig_clauses = i;
           goto DONE;
         }
       } else {
         num_lits = 0;
       }
      }
   }
   goto NEXT;
DONE:
  free (line); 
  return 0;

}


 
 
void add_var (int var, Scope *scope) {
  Var *v; 
  assert (var <= num_vars && 0 < var); 

  v = lit2var (var); 

  assert (!v->scope); 

  v->scope = scope;
  v->next = NULL; 
  v->prev = scope->last; 
  v->name = var; 

  if (scope->last) scope->last->next = v; 
  else scope->first = v; 
  scope->last = v; 

}

void add_quantifier (int lit) {
  Scope *scope; 
  QType qtype; 

  if (lit < 0) qtype = FORALL; else qtype = EXISTS; 

  if (!outer_most) {
    NEW (outer_most); 
    outer_most->type = qtype; 
    inner_most = outer_most; 
    outer_most->order = order++; 
  }

  if (inner_most->type != qtype) {
    NEW (scope); 
    scope->outer = inner_most; 
    scope->inner = NULL; 
    scope->type = qtype;
    scope->order = order++;
    scope->order = inner_most->order+1;  
    inner_most->inner = scope; 
    inner_most = scope; 
  } else {
    scope = inner_most; 
  } 
  add_var (abs(lit), scope); 

}

Scope * lit2scope (int lit) {
  return lit2var (lit)->scope;
}

Occ * lit2occ (int lit) {
  Var * v = lit2var (lit);
  return v->occs + (lit < 0);
}


int lit2order (int lit) {
  return lit2scope (lit)->order;
}

Var * lit2var (int lit) {
  assert (lit && abs (lit) <= num_vars);
  return vars + abs (lit);
}

int is_universal (int lit) {
  Scope *s = lit2scope (lit); 
  return s->type == FORALL; 
}

void init_vars_clauses (int vnum, int cnum) {
  num_vars = vnum; 
  NEWN (vars, num_vars + 1);
  max_var = num_vars + 1; 
  num_clauses = cnum; 
  NEWN (clauses, num_clauses + 1); 
}


static void * fix_ptr (void * ptr, long delta) {
  if (!ptr) return 0;
  return delta + (char*) ptr;
}

static void fix_vars (long delta) {
  Var * v;
  Scope * s;
  for (s = outer_most; s; s = s->inner) {
    v = s->first;
    if (!v) continue;
    s->first = fix_ptr (v, delta);
    s->last = fix_ptr (s->last, delta);
    for (v = s->first; v; v = v->next) {
      v->prev = fix_ptr (v->prev, delta);
      v->next = fix_ptr (v->next, delta);
    }
  }

}


void enlarge_clauses () {
  RSZ (clauses, num_clauses + 1, num_clauses*10 + 1);

  num_clauses = num_clauses * 10; 
}


void enlarge_vars (int new_num) {
  char * old_vars, * new_vars;
  long delta;

  assert (num_vars <= new_num);

  old_vars = (char*) vars;
  RSZ (vars, num_vars + 1, new_num + 1);
  new_vars = (char*) vars;

  delta = new_vars - old_vars;
  if (delta) fix_vars (delta); 
  num_vars = new_num; 
}

int fresh_var () {
  if (max_var >= num_vars) {
    enlarge_vars (num_vars * 2); 
  }

  return max_var++; 

}





void release () {
  Scope *s, *snext;
  int i;  
  Clause *c;
  Node *n, *next;
  AnnotationNode *a, *a2;   

  for (s = outer_most; s; s = snext) {
    snext = s->inner;
    free (s); 
  }
  for (i = 0; i < num_clauses; i++) {
    c = clauses + i; 
    n = c->first; 
    while (n) {
      next = n->cnext; 
      free (n); 
      n = next; 
   }

  }

  for (i = 1; i <= num_vars; i++) {
    a = vars[i].an; 

    while (a) {
      a2 = a->next; 
      free (a); 
      a = a2; 
    }
  }

  free (clauses); 
  free (vars); 


}
