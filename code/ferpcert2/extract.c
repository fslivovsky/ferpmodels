


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


static  int aig_aux;
static simpleaig * aig;
int print_aig_and = 1;
int visited = 1; 



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
      } else {
        simpleaig_add_input (aig, abs(v->name));
        dprint ("input: %d\n", v->name);
      }
    }

    s = s->inner;
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


void extract_leave (P_Clause *cl, int level) {
  int i, j, lit_level, set_univ = 0;
  int univ; 

  dprint ("\n extracting activitiy of leaf clause %d with aig-label %d at level %d\n", cl->name, cl->aig_label, level); 

  for (i = 0; i < cl->size; i++) {
    lit_level = avar_get_level (cl->nodes [i]);

    dprint ("  lit %d has level %d\n", cl->nodes [i], lit_level); 

    if (lit_level < level){	// literal has been assigned
	    			// therefore it is not active
      dprint ("  lit %d is inactive\n", cl->nodes [i]); 
      cl->var_activity [abs(cl->nodes [i])] = aiger_false;
      cl->var_activity_p [abs(cl->nodes [i])] = aiger_false;
      continue;
    }
	
					// include universals in 
					// activity of clause
    if ((!set_univ) && (lit_level >= level)) {
      set_univ = 1; 
      for (j = 0; j < a_vars [abs(cl->nodes[i])].ann_size; j++) {
        univ = a_vars [abs(cl->nodes [i])].u_annotations [j];

	 dprint ("  univ %d with level  %d\n", 
			 univ, (vars + abs(univ))->scope->order); 
	 if ((vars + abs(univ))->scope->order == level -1 ) {
	  dprint ("  adding univ %d to activity of cl %d\n", 
			   univ, cl->name);  
          cl->aig_label = makeAND (cl->aig_label, univ);
	 }
 	}
    }

    if (lit_level == level) {	// literal is now assigned
				// -> influences activity of clause
      dprint ("   lit %d (orig: %d) becomes inactive\n", 
		      cl->nodes[i], plit2qlit (cl->nodes[i]));
      cl->aig_label = makeAND (cl->aig_label, -plit2qlit (cl->nodes [i])); 

      dprint ("   cl %d has act label %d\n", cl->name, cl->aig_label); 

      cl->var_activity [abs(cl->nodes [i])] = aiger_false;
      cl->var_activity_p [abs(cl->nodes [i])] = aiger_false;
    } else {
      cl->var_activity [abs(cl->nodes [i])] = cl->aig_label;
      cl->var_activity_p [abs(cl->nodes [i])] = cl->aig_label;
      dprint ("  setting var activity of %d to %d\n", cl->nodes[i], cl->aig_label); 
    }



  }

}


 int get_var_activity (int a1, int a2,          // var act in parents
                               int p1, int p2,          // pivot act
                               int c1, int c2) {        // cl activity
   int cond1, or1; 
   int cond2, ite2, conj2;

  cond2 = makeAND (simpleaig_not (p1), c1); 

  conj2 = makeAND (simpleaig_not(p2), c2); 
  conj2 = makeAND (conj2, a2); 

  ite2 = makeITE (cond2, a1, conj2); 

  or1 = makeOR (a1, a2); 

  cond1 = makeAND (p1, p2);

  return makeITE (cond1, or1, ite2);  




}



void extract_non_leave (P_Clause *cl, int level) {
  P_Clause *p1, *p2; 
   int a1, a2, conj_piv; 
  int i; 
  int pivot = abs (cl->pivot); 
  
  dprint ("\n ** extracting activitiy of non-leaf clause %d with aig-label %d at level %d\n", cl->name, cl->aig_label, level); 
  
  p1 = p_clauses + cl->p1; 
  p2 = p_clauses + cl->p2;
  dprint ("  parents: %d %d, pivot: %d\n", p1->name, p2->name, pivot); 

  assert (p1 && p2);  

  // activity of clause
  
  conj_piv = makeAND (p1->var_activity [pivot], p2->var_activity [pivot]); 

  a1 = makeAND (simpleaig_not (p1->var_activity [pivot]), p1->aig_label); 
  a2 = makeAND (simpleaig_not (p2->var_activity [pivot]), p2->aig_label); 

  a1 = makeOR (conj_piv, a1); 
  a1 = makeOR (a1, a2); 

  cl->aig_label = a1; 
  
  // activity of literals

  for (i = 1; i < a_vars_size; i++) {
    a1 = p1->var_activity_p [i];
    a2 = p2->var_activity_p [i];

    
    if (!a1 && !a2) {
      cl->var_activity [i] = aiger_false;
      cl->var_activity_p [i] = aiger_false;
      continue;
    }

   

    if (i == pivot) {
      cl->var_activity [i] = aiger_false;
    } else {
      cl->var_activity [i] = get_var_activity (
                      p1->var_activity [i],
                      p2->var_activity [i],
                      p1->var_activity [pivot],
                      p2->var_activity [pivot],
                      p1->aig_label,
                      p2->aig_label);

    }

    cl->var_activity_p [i] = get_var_activity (a1, a2,
                      p1->var_activity [pivot],
                      p2->var_activity [pivot],
                      p1->aig_label,
                      p2->aig_label);

    dprint ("var %d in clause %d is %d %d\n", i, cl->name, 
		    cl->var_activity_p[i], cl->var_activity[i]); 

  }

}



void extract_exist (int level) {
  int i;

  for (i = 1; i < num_p_clauses; i++) {

    if (!p_clauses [i].p1) {
      extract_leave (p_clauses + i, level);
    } else {
      extract_non_leave (p_clauses + i, level);
    }

  }
}




void extract_univ (Scope *s, int level) {
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




}

simpleaig * extract () {
  Scope *s; 
  int level = 1; 


  dprint ("extract: first fresh aiger var: %d\n", aig_aux); 

  aig = simpleaig_init ();

  simpleaig_set_buckets (aig, (unsigned) a_vars_size * p_clauses_size * 2) ; 
  init_inputs_outputs ();
  
  aig_aux = num_vars ; 
  aig->lhs_aux = num_vars + 1; 

  s = outer_most;  

  if (s->type == FORALL) {
    init_outermost_univ (); 

    s = s->inner; 
    level++; 
  }

 
  while (s) {

    dprint ("\n------- considering level %d ---------- \n\n", level); 
    if (s->type == EXISTS) {
      extract_exist (level);  
    }
    else {
      extract_univ (s, level); 
    }

    level++; 

    s = s->inner; 
    if (!s || (!s->inner && s->type == EXISTS)) break;

  }

  ////aiger_prune (aig); 

  return aig; 
}


