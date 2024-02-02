#ifndef FORMULA_H
#define FORMULA_H

typedef enum QType QType;
typedef struct Scope Scope;
typedef struct Var Var;
typedef struct Node Node;
typedef struct Clause Clause;
typedef struct Occ Occ;
typedef struct AnnotationNode AnnotationNode; 


enum QType {
  FORALL = 0,
  EXISTS = 1,
};


struct Scope {
  QType type;
  int order; 
  Var *first, *last;    
  Scope *inner, *outer;  
};

struct Occ {
  struct Node *first, *last; 
};



struct Var {
  struct Scope *scope; 
  int name;
  AnnotationNode *an; 
  int mark; 
  Var *next, *prev;     // next variable in same quantifier block 
  Occ occs[2]; 
};

struct AnnotationNode {
  int ex_var;  
  struct AnnotationNode *next; ; 
}; 


struct Node {
  int lit; 
  struct Node *prev, *next; 
  struct Node *cnext; 
  int clause_id;  
}; 


struct Clause {
  int p1, p2; 
  int size; 
  Node *first; 
}; 



extern Scope *outer_most, *inner_most;
extern Var *vars;
extern Clause *clauses, *empty_clause;
extern int num_vars, num_clauses;
extern int universal_vars, existential_vars, implicit_vars, orig_clauses;


void add_quantifier (int); 
Scope *lit2scope (int); 
Var *lit2var (int); 
Occ *lit2occ (int);
int lit2order (int); 
int is_universal (int);  
int fresh_var (); 
void release (); 
void enlarge_clauses ();
int parse_qbf (FILE *); 

#endif
