#ifndef PROOF_H
#define PROOF_H

typedef struct A_Var A_Var;
typedef struct P_Clause P_Clause;


struct A_Var {
  int orig_ex_name; 
  int mark; 
  A_Var *next;  
  int ann_size; 
  int *u_annotations; 
};


struct P_Clause {
  int name; 
  int pivot; 		// pivot
  int p1, p2; 		// parent clauses
  int size;		// number of nodes
  int *nodes;  		// literals of clause
  int *universals; // universal variables with non-trivial partial functions
  int *aig_labels; // aig labels for partial functions
  int num_universals;
};

extern int a_vars_size, p_clauses_size;
extern A_Var *a_vars;
extern P_Clause *p_clauses;
extern int p_empty_clause;
extern int num_p_vars, num_p_clauses;

int a_lit2var (int); 
int avar_get_level (int); 
void proof_release (); 
void enlarge_proof_vars (int);
void enlarge_proof_clauses (int);
int parse_proof (FILE *); 
void print_proof (); 
int get_max_exists_level (P_Clause *); 
void print_p_clause (P_Clause *);
#endif
