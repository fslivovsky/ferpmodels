

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "formula.h"
#include "admin.h"
#include "proof.h"
#include "simpleaig.h"

#define aiger_true SIMPLEAIG_TRUE
#define aiger_false SIMPLEAIG_FALSE

int a_vars_size, p_clauses_size;
A_Var *a_vars;
P_Clause *p_clauses;
int p_empty_clause;
int num_p_vars, num_p_clauses;


int avar_get_level (int l) {
  Var *v = vars + a_lit2var (l); 
  return v->scope->order; 

}


void print_p_clause (P_Clause *c) {
  int i;   

  printf ("%d: ", c->name); 
  for (i = 0; i < c->size; i++) printf ("%d ", c->nodes[i]);

  printf ("\n"); 

}

int get_max_exists_level (P_Clause *c) {
  int last; 

  if (!c->size) return 0; 
  last = a_lit2var (c->nodes [c->size-1]); 


  return lit2var (last)->scope->order; 
}

void proof_release () {
  int i; 

  for (i = 0; i < a_vars_size; i++) {
    if (!a_vars [i].next) free (a_vars [i].u_annotations); 
  }
  for (i = 0; i < p_clauses_size; i++) {
    free (p_clauses [i].nodes);
    free (p_clauses [i].universals);
    free (p_clauses [i].aig_labels);
  }
 
  free (p_clauses); 
  free (a_vars); 

}


int a_lit2var (int l) {
  assert (abs(l) < a_vars_size); 

  return a_vars [abs(l)].orig_ex_name;

}

static int cmpfunc (const void * a, const void * b) {
  const int *l1 = a, *l2 = b;
  int ov1, ov2; 
  Var *v1, *v2; 

  ov1 = a_lit2var (*l1); 
  ov2 = a_lit2var (*l2); 

  v1 = lit2var (ov1); 
  v2 = lit2var (ov2); 

  return v1->scope->order > v2->scope->order; 


}

void enlarge_proof_vars (int sz) {
  int new_size_a_vars = a_vars_size ? (2*a_vars_size + sz) : (1+sz);
  RSZ (a_vars, a_vars_size, new_size_a_vars);
  a_vars_size = new_size_a_vars; 
}

void enlarge_proof_clauses (int sz) {
  int new_size_p_clauses = p_clauses_size ? (2*p_clauses_size + sz) : (1+sz);
  RSZ (p_clauses, p_clauses_size, new_size_p_clauses);
  p_clauses_size = new_size_p_clauses;
} 

int parse_proof (FILE *f) {
  int tmp, cl; 
  int i, j, p1, p2; 
  int lit, count_vars = 0, count_an; 
  A_Var * next = NULL; 
  int *annos, *lts; 
  AnnotationNode *anode; 

  num_p_clauses = 1; 
  num_p_vars = 1; 

  assert (!num_lits); 
  do {
    tmp = fscanf (f, "x %d", &lit); 
    if (tmp == EOF) break; 

    if (tmp <= 0) break; 
    count_vars = 0; 
    next = NULL; 
    while (lit) {
      if (lit <= 0) die ("error in parsing proof; expecting var, found lit"); 
      push_literal (lit); 
      count_vars++;
      num_p_vars++; 
      if (a_vars_size <= lit) enlarge_proof_vars (lit); 

      (a_vars+lit)->next = next; 
      next = &a_vars [lit]; 

      tmp = fscanf (f, "%d", &lit); 

      if (tmp <= 0) die ("error in parsing proof");     
    }

    for (i = 0; i < count_vars; i++) {
      tmp = fscanf (f, "%d", &lit); 

      if (tmp <= 0) die ("error in parsing proof");     
      if (lit <= 0) die ("error in parsing proof %d", lit); 
      
      a_vars[lits[i]].orig_ex_name = lit;     
    }

    count_an = 0; 
    tmp = fscanf (f, "%d", &lit); 
    if (tmp <= 0) die ("error in parsing proof");     

    tmp = fscanf (f, "%d", &lit); 
    if (tmp <= 0) die ("error in parsing proof");  
   
    while (lit) {
      count_an++; 
      push_literal (lit); 
      tmp = fscanf (f, "%d", &lit); 
      if (tmp <= 0) die ("error in parsing proof");     
    }
    if (count_an) {
      annos = malloc (sizeof(int) * count_an); 
      for (i = 0; i < count_an; i++) annos[i] = lits [count_vars + i];     
      for (i = 0; i < count_vars; i++) {
	      a_vars [lits[i]].u_annotations = annos; 
	      a_vars [lits[i]].ann_size = count_an; 
      }

      for (i = 0; i < count_an; i++) {
        for (j = 0; j < count_vars; j++) {
	  if (lits [count_vars+i] < 0) continue; // neg annotations
          anode = malloc (sizeof (AnnotationNode)); 
	  anode->ex_var = lits[j]; 
	  anode->next = vars[lits [count_vars+i]].an; 
	  vars[lits [count_vars+i]].an = anode; 
	}

      }
    }
    tmp = fscanf (f, "\n"); 
    num_lits = 0; 
  } while (1); 

  do {
    tmp = fscanf (f, "%d", &lit); 
    if (tmp == EOF) break; 

    if (tmp <= 0) break; 
    if (lit <= 0) die ("error in parsing proof (neg clause idx)"); 

    if (p_clauses_size <= lit) enlarge_proof_clauses (lit); 
    

    if (p_clauses [lit].name) die ("clause init twice"); 
    p_clauses [lit].name = lit; 
  
    count_vars = 0; 
    cl = lit; 
    while (lit) {
      tmp = fscanf (f, "%d", &lit); 
      if (tmp <= 0) die ("error in parsing proof");     
      if (!lit) break; 
      push_literal (lit); 
      count_vars++; 
   } 

   lts = malloc (sizeof (int) * count_vars); 

   num_p_clauses++; 
   for (i = 0; i < count_vars; i++) lts [i] = lits[i]; 

   qsort (lts, count_vars, sizeof (int), cmpfunc); 

   p_clauses [cl].size = count_vars;    
   p_clauses [cl].nodes = lts;   

   if (count_vars == 0) p_empty_clause = cl; 
 
   tmp = fscanf (f, "%d %d", &p1, &p2); 

   if (p1 && p2) {  // two parents
     p_clauses[cl].p1 = p1; 
     p_clauses[cl].p2 = p2;
  
     for (i = 0; i < p_clauses [p1].size; i++) {
       a_vars [abs(p_clauses[p1].nodes[i])].mark =  p_clauses[p1].nodes[i]; 
     }

     for (i = 0; i < p_clauses [p2].size; i++) {
       if (a_vars [abs(p_clauses[p2].nodes[i])].mark ==  -p_clauses[p2].nodes[i]) break;  
     }

     assert (i < p_clauses [p2].size); 

     p_clauses[cl].pivot = -p_clauses[p2].nodes[i];
     for (i = 0; i < p_clauses [p1].size; i++) {
       a_vars [abs(p_clauses[p1].nodes[i])].mark = 0; 
     }
     tmp = fscanf (f, "%d", &lit); // process final zero
    
   } else {
    // Single parent

    assert(p1 && !p2);
    assert(p1 <= num_clauses);
    p_clauses[cl].p1 = p1;
    p_clauses[cl].p2 = 0;
   }

   num_lits = 0; 
   if (p_empty_clause) break; 
  } while (1); 

  assert (p_empty_clause+1 == num_p_clauses); 

  return 0; 
}

void print_proof () {
  int i, j;
  P_Clause *c;  
  printf ("number of proof clauses: %d\n", num_p_clauses); 
  for (i = 1; i <= num_p_clauses; i++) {
    c = p_clauses + i; 
    if (c) printf ("cl %d with parents %d %d and pivot %d\n", i, c->p1, c->p2, c->pivot); 
    for (j = 0; j < c->size; j++) printf ("%d ", c->nodes[j]); 
    printf ("\n"); 
  } 
}
