


#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extract.h"
#include "simpleaig.h"


#define DEBUG  0
#define dprint(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
#define aiger_false SIMPLEAIG_FALSE 
#define aiger_true SIMPLEAIG_TRUE

static int int_cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

static  int aig_aux;
static simpleaig * aig;
static simpleaig * aig_out;
int print_aig_and = 1;
int visited = 1;

static int * var_order;
int * tmp_var_array;
char * aig_var_mapped;
int * lit_to_lit;
int * var_to_idx;
int * idx_queue;
int idx_queue_size, idx_queue_pos;

int og_cmpfunc (const void * a, const void * b) {
  const int *l1 = a, *l2 = b;
  return var_order[abs(*l1)] - var_order[abs(*l2)];
}

int plit2qlit (int l) {
  if (l < 0) 
  return -a_vars [abs(l)].orig_ex_name;
  else return a_vars [abs(l)].orig_ex_name;
}

 int get_fresh_aig_idx () {
  aig_aux += 1;
  return aig_aux;
}


 int makeAND ( int a,  int b) {

 if((a == aiger_false) || (b == aiger_false))
    return aiger_false;
  if(a == aiger_true)
    return b;
  if(b == aiger_true)
    return a;
  if(a == b)
    return a;
  if(a == simpleaig_not(b))
    return aiger_false;

  return simpleaig_add_and (aig, aiger_false, a, b);

}

 int makeOR ( int a,  int b) {
  return simpleaig_not (makeAND (simpleaig_not (a), simpleaig_not (b))); 
}

 int makeITE ( int cond,  int a,  int b) {
 if(cond == aiger_true)
    return a;
  if(cond == aiger_false)
    return b;
  if(a == b)
    return a;

   int t = makeAND (cond, a); 
   int f = makeAND (simpleaig_not(cond), b); 

  return makeOR (t, f); 

}



void init_inputs_outputs () {
  Scope *s = outer_most;
  Var *v;
  while (s) {
    for (v = s->first; v; v = v->next) {
      if (s->type == FORALL) {
        simpleaig_add_output (aig, abs(v->name));
        dprint ("output: %d\n", v->name);
      }
      simpleaig_add_input (aig, abs(v->name));
      dprint ("input: %d\n", v->name);
    }

    s = s->inner;
  }

}

void init_output_aig () {
  aig_out = simpleaig_init ();
  simpleaig_set_buckets (aig_out, aig->num_ands) ; 
  
  aig_aux = num_vars ; 
  aig_out->lhs_aux = num_vars + 1; 

  Scope *s = outer_most;
  Var *v;
  while (s) {
    for (v = s->first; v; v = v->next) {
      if (s->type == FORALL) {
        simpleaig_add_output (aig_out, abs(v->name));
        dprint ("Final AIG output: %d\n", v->name);
      } else {
        simpleaig_add_input (aig_out, abs(v->name));
        dprint ("Final AIG input: %d\n", v->name);
      }
    }
    s = s->inner;
  }
}

void init_aig_traversal() {
  aig_var_mapped = (char *) malloc (sizeof (char) * (aig->max_var + 1));
  lit_to_lit = (int *) malloc (sizeof (int) * (aig->max_var + 1));
  var_to_idx = (int *) malloc (sizeof (int) * (aig->max_var + 1));
  idx_queue = (int *) malloc (sizeof (int) * (aig->num_ands));
  idx_queue_pos = 0;
  idx_queue_size = 0;

  for (int i = 0; i < aig->max_var + 1; i++) {
    aig_var_mapped[i] = 0;
    lit_to_lit[i] = 0;
    var_to_idx[i] = aig->num_ands;
  }
  for (int i = 0; i < aig->num_ands; i++) {
    assert(aig->ands[i].lhs > 0);
    var_to_idx[aig->ands[i].lhs] = i;
  }
  // Map inputs
  for (int i = 0; i < aig_out->num_inputs; i++) {
    aig_var_mapped[aig_out->inputs[i]] = 1;
    lit_to_lit[aig_out->inputs[i]] = aig_out->inputs[i];
  }
}

void release_aig_traversal() {
  free(aig_var_mapped);
  free(lit_to_lit);
  free(var_to_idx);
  free(idx_queue);
}

char aig_is_const (int v) {
  return v == aiger_true || v == aiger_false;
}

void aig_get_cone (int v) {
  assert(!aig_var_mapped[v]);
  aig_var_mapped[v] = 1;
  assert(var_to_idx[v] < aig->num_ands);
  idx_queue[0] = var_to_idx[v];
  idx_queue_size = 1;
  idx_queue_pos = 0;
  while (idx_queue_pos < idx_queue_size) {
    int idx = idx_queue[idx_queue_pos++];
    simpleaigand *and = aig->ands + idx;
    if (!aig_is_const(and->rhs0) && aig_var_mapped[abs(and->rhs0)] == 0) {
      aig_var_mapped[abs(and->rhs0)] = 1;
      assert(var_to_idx[abs(and->rhs0)] < aig->num_ands);
      idx_queue[idx_queue_size++] = var_to_idx[abs(and->rhs0)];
    }
    if (!aig_is_const(and->rhs1) && aig_var_mapped[abs(and->rhs1)] == 0) {
      aig_var_mapped[abs(and->rhs1)] = 1;
      assert(var_to_idx[abs(and->rhs1)] < aig->num_ands);
      idx_queue[idx_queue_size++] = var_to_idx[abs(and->rhs1)];
    }
  }
  qsort(idx_queue, idx_queue_size, sizeof(int), int_cmpfunc);
}

void aig_map_cone (int v) {
  for (int i = 0; i < idx_queue_size; i++) {
    int idx = idx_queue[i];
    simpleaigand *and = aig->ands + idx;
    int rhs0, rhs1;
    if (aig_is_const(and->rhs0)) {
      rhs0 = and->rhs0;
    } else {
      assert(aig_var_mapped[abs(and->rhs0)]);
      int rhs0_mapped = lit_to_lit[abs(and->rhs0)];
      rhs0 = and->rhs0 > 0 ? rhs0_mapped : simpleaig_not(rhs0_mapped);
    }
    if (aig_is_const(and->rhs1)) {
      rhs1 = and->rhs1;
    } else {
      assert(aig_var_mapped[abs(and->rhs1)]);
      int rhs1_mapped = lit_to_lit[abs(and->rhs1)];
      rhs1 = and->rhs1 > 0 ? rhs1_mapped : simpleaig_not(rhs1_mapped);
    }
    if (and->lhs == v) {
      lit_to_lit[and->lhs] = simpleaig_add_and(aig_out, v, rhs0, rhs1);
    } else {
      lit_to_lit[and->lhs] = simpleaig_add_and(aig_out, SIMPLEAIG_FALSE, rhs0, rhs1);
    }
    assert(aig_var_mapped[and->lhs]);
  }
}


void init_outermost_univ () {
  //TODO assumption: only one empty clause,
  //no clauses that are not connected to the empty clause
  //all declared vars occur in proof
  //


  Var *v;

  v = outer_most->first;

  while (v) {
    if (!v->an) {
      simpleaig_add_and (aig, v->name, aiger_false, aiger_false);
    } else {
      simpleaig_add_and (aig, v->name, aiger_true, aiger_true);
    }
    v = v->next;
  }

}

void extract_leaf (P_Clause *cl) {
  assert(cl->p2 == 0);
  // First, compute universal literals in original clause and annotations
  num_lits = 0; // Reused from parser
  // Go through all annotated literals and their annotations
  for (int i = 0; i < cl->size; i++) {
    int lit = cl->nodes[i];
    for (int j = 0; j < a_vars [abs(lit)].ann_size; j++) {
      int u_lit = a_vars [abs(lit)].u_annotations [j];
      // Use the clause index to check whether a variable has occurred before
      if (tmp_var_array[abs(u_lit)] < cl->name) {
        tmp_var_array [abs(u_lit)] = cl->name;
        push_literal(u_lit);
        assert(is_universal(u_lit));
      }
    }
  }

  Clause* og_clause =  &clauses [cl->p1];
  for (int i = 0; i < og_clause->size; i++) {
    int lit = og_clause->lits[i];
    if (is_universal(lit) && tmp_var_array [abs(lit)] < cl->name) {
        tmp_var_array [abs(lit)] = cl->name;
        push_literal(lit);
    }
  }
  // Copy complete annotation
  cl->universals = (int *) malloc (sizeof (int) * num_lits);
  for (int i = 0; i < num_lits; i++) {
    cl->universals[i] = lits[i];
  }
  qsort(cl->universals, num_lits, sizeof(int), og_cmpfunc);
  // Second, compute AIG labels for partial functions
  cl->aig_labels = (int *) malloc (sizeof (int) * num_lits);
  // This circuit checks whether the universals so far match the annotation
  int annotation_match_aig = aiger_true;

  for (int i = 0; i < num_lits; i++) {
    if (cl->universals[i] > 0) {
      cl->aig_labels[i] = aiger_true;
      annotation_match_aig = makeAND(annotation_match_aig, cl->universals[i]);
    } else {
      cl->aig_labels[i] = makeITE(annotation_match_aig, aiger_false, aiger_true);
      annotation_match_aig = makeAND(annotation_match_aig, simpleaig_not(abs(cl->universals[i])));
    }
    // Forget the sign of the annotation literal
    // We only need to know which universal variables have partial functions
    cl->universals[i] = abs(cl->universals[i]);
  }
  cl->num_universals = num_lits;
}


void extract_non_leaf (P_Clause *cl) {
  P_Clause *p1, *p2; 
  int pivot = cl->pivot;
  int pivot_og_var = a_lit2var(pivot);
  
  p1 = p_clauses + cl->p1; 
  p2 = p_clauses + cl->p2;
  dprint ("  parents: %d %d, pivot: %d\n", p1->name, p2->name, pivot); 

  assert (p1 && p2);

  int* universals1 = p1->universals;
  int* universals2 = p2->universals;

  int* aig_labels1 = p1->aig_labels;
  int* aig_labels2 = p2->aig_labels;

  int* aig_labels = (int *) malloc (sizeof (int) * (p1->num_universals + p2->num_universals));
  int nr_labels = 0;

  // Get pivot annotation
  qsort(a_vars [abs(pivot)].u_annotations, a_vars [abs(pivot)].ann_size, sizeof(int), og_cmpfunc);

  int annotation_match_aig = aiger_true;

  int i, j, k;
  num_lits = 0; // Reused from parser
  for (i = j = k = 0; i < p1->num_universals || j < p2->num_universals; ) {
    int u = 0;
    int aig1, aig2;
    int cmp;
    if (i == p1->num_universals) {
      cmp = 1;
    } else if (j == p2->num_universals) {
      cmp = -1;
    } else {
      cmp = og_cmpfunc(&universals1[i], &universals2[j]);
    }
    if (cmp == 0) {
      u = universals1[i];
      aig1 = aig_labels1[i];
      aig2 = aig_labels2[j];
      i++;
      j++;
    } else if (cmp < 0) {
      u = universals1[i];
      aig1 = aig_labels1[i];
      aig2 = aiger_true;
      i++;
    } else {
      u = universals2[j];
      aig1 = aiger_true;
      aig2 = aig_labels2[j];
      j++;
    }
    push_literal(u);
    // Update annotation match AIG with literals preceding u
    for (; k < a_vars[abs(pivot)].ann_size && og_cmpfunc(&a_vars[abs(pivot)].u_annotations[k], &u) < 0; k++) {
      int annotation_lit = a_vars[abs(pivot)].u_annotations[k];
      int lit_aig = annotation_lit > 0 ? annotation_lit : simpleaig_not(abs(annotation_lit));
      annotation_match_aig = makeAND(annotation_match_aig, lit_aig);
    }
    if (og_cmpfunc(&pivot_og_var, &u) > 0) {
      // Universal u precedes the pivot, u must appear in the pivot annotation
      assert(k < a_vars[abs(pivot)].ann_size && og_cmpfunc(&a_vars[abs(pivot)].u_annotations[k], &u) == 0);
      int annotation_lit = a_vars[abs(pivot)].u_annotations[k];
      if (annotation_lit > 0) {
        // Definitely in 1 part
        aig_labels[nr_labels++] = makeAND(aig1, aig2);
        annotation_match_aig = makeAND(annotation_match_aig, annotation_lit);
      } else {
        // Possibly in 0 part
        aig_labels[nr_labels++] = makeITE(annotation_match_aig, makeOR(aig1, aig2), makeAND(aig1, aig2));
        annotation_match_aig = makeAND(annotation_match_aig, simpleaig_not(abs(annotation_lit)));
      }
    } else if (og_cmpfunc(&pivot_og_var, &u) < 0) {
      // Universal u comes after the pivot, so this is either 1-local or shared
      assert(k == a_vars[abs(pivot)].ann_size);
      int pivot_aig = pivot > 0 ? pivot_og_var : simpleaig_not(pivot_og_var);
      int shared_aig = makeITE(pivot_aig, aig2, aig1);
      int local_aig = makeAND(aig1, aig2);
      aig_labels[nr_labels++] = makeITE(annotation_match_aig, shared_aig, local_aig);
    } else {
      // Pivot is existential and u is universal
      assert(0);
    }
  }
  cl->universals = (int *) malloc (sizeof (int) * num_lits);
  for (int i = 0; i < num_lits; i++) {
    cl->universals[i] = lits[i];
  }
  cl->num_universals = num_lits;
  cl->aig_labels = (int *) malloc (sizeof (int) * nr_labels);
  for (int i = 0; i < nr_labels; i++) {
    cl->aig_labels[i] = aig_labels[i];
  }
  free(aig_labels);
}

/* void extract_univ (Scope *s, int level) {
  AnnotationNode *a; 
  Var *v;
   int disj;
   int conj;  
   int univ;
   int i;   
  P_Clause *root = p_clauses + p_empty_clause;

  v = s->first; 

  while (v) {
    a = v->an;

    dprint (" building herbrand function for %d\n", v->name);  

    if (!a) {
       dprint ("  no pos annotations with univ var %d\n", v->name); 
       simpleaig_add_and (aig, v->name , aiger_false, aiger_false); 
       v = v->next; 
       continue; 
    }

    disj = aiger_false; 
    while (a) {
      dprint ("var %d occurs in ann of a->ex_var %d\n", v->name, a->ex_var); 	    
      if (!root->var_activity_p [a->ex_var]) {
        dprint ("skipping annotated var %d\n", a->ex_var);
        a = a->next;
        continue;
      }

      conj = aiger_true; 
      for (i = 0; i < a_vars [a->ex_var].ann_size; i++) {
        univ = a_vars [a->ex_var].u_annotations [i];

	if (!vars [abs (univ)].mark) continue; 
      
	conj = makeAND (conj, univ); 

      }

      conj = makeAND (conj, root->var_activity_p [a->ex_var]); 

      dprint ("  building disj from %d %d from avar %d\n", 
		      disj, root->var_activity_p [a->ex_var], a->ex_var);
      disj = makeOR (disj, conj); 
      a = a->next;
    }


    simpleaig_add_and (aig, v->name , disj, disj); 
    v = v->next;
    


  }
} */

void create_var_order() {
  var_order = (int *) malloc (sizeof (int) * (num_vars + 1));
  
  int order = 0;
  Scope *s = outer_most;

  while (s) {
    Var *v = s->first;
    while (v) {
      var_order[v->name] = order++;
      v = v->next;
    }
    s = s->inner;
  }
}

simpleaig * extract () {
  dprint ("extract: first fresh aiger var: %d\n", aig_aux);

  create_var_order();

  aig = simpleaig_init ();
  simpleaig_set_buckets (aig, (unsigned) universal_vars * p_clauses_size * 2) ; 
  init_inputs_outputs ();
  
  aig_aux = num_vars ; 
  aig->lhs_aux = num_vars + 1; 

  tmp_var_array = (int *) malloc (sizeof (int) * (num_vars + 1));

  for (int i = 0; i < num_vars + 1; i++) {
    tmp_var_array [i] = 0; 
  }

  // Compute partial interpolants

  for (int i = 0; i < num_p_clauses; i++) {
    if (!p_clauses [i].p2) {
      extract_leaf (p_clauses + i);
    } else {
      extract_non_leaf (p_clauses + i);
    }
  }

  // Set universal strategy function to constant true by default

  for (int i = 0; i < num_vars + 1; i++) {
    tmp_var_array [i] = aiger_true; 
  }

  P_Clause *root = p_clauses + p_empty_clause;

  for (int i = 0; i < root->num_universals; i++) {
    int u = root->universals[i];
    tmp_var_array[u] = root->aig_labels[i];
  }

  Scope *s = outer_most;

  while (s) {
    if (s->type == EXISTS) {
      s = s->inner;
      continue;
    }
    assert(s->type == FORALL);
    Var *v = s->first;
    while (v) {
      simpleaig_add_and (aig, v->name, tmp_var_array[v->name], tmp_var_array[v->name]);
      v = v->next;
    }
    s = s->inner;
  }

  init_output_aig();
  init_aig_traversal();

  s = outer_most;

  while (s) {
  if (s->type == EXISTS) {
    s = s->inner;
    continue;
  }
  assert(s->type == FORALL);
  Var *v = s->first;
  while (v) {
    aig_get_cone(v->name);
    aig_map_cone(v->name);
    v = v->next;
  }
  s = s->inner;
}

  ////aiger_prune (aig); 

  release_aig_traversal();

  free(tmp_var_array);
  free(var_order);

  simpleaig_reset(aig);

  return aig_out; 
}


