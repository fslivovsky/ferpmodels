#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include "formula.h"
#include "admin.h"
#include "proof.h"
#include "extract.h"
#include "simpleaig.h"


int main (int argc, char **argv) {
  FILE *in_proof = stdin; 		// FERP proof
  FILE *in_qbf = stdin; 		// original QBF
  FILE *out_aig = stdin;		// aiger file
  simpleaig * aig = NULL; 
  char binary = 0; 

  if (argc != 4) die ("invalid number of arguments"); 
  
  in_qbf = fopen (argv [1], "r"); 
  in_proof = fopen (argv [2], "r"); 
  out_aig = fopen (argv [3], "w"); 

  if (!in_qbf) die ("could not open QBF %s", argv [1]); 
  if (!in_proof) die ("could not open proof %s", argv [2]); 

  if (parse_qbf (in_qbf)) {
    die ("could not parse QBF %s", argv [1]); 
  }
  if (parse_proof (in_proof)) {
    die ("could not parse proof %s", argv [2]); 
  }
  aig = extract(); 

  simpleaig_write_aiger_to_file (aig, out_aig, binary);  
//  printf ("%d\n", p_empty_clause); 
//  print_proof(); 
  simpleaig_reset (aig); 
  fclose (in_qbf); 
  fclose (in_proof);
  free (lits); 
  release (); 
  proof_release ();  
  return 0; 
}
