/*  SimpleAIG: Simple And-Inverter Graph library. 
 *
 *  Copyright (c) 2011-2012 Mathias Preiner.
 *
 *  This file is part of QRPcert.
 *
 *  QRPcert is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  QRPcert is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with QRPcert.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDE_SIMPLEAIG_H 
#define INCLUDE_SIMPLEAIG_H

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#define SIMPLEAIG_FALSE 0 
#define SIMPLEAIG_TRUE INT_MIN

typedef struct simpleaig simpleaig;
typedef struct simpleaigand simpleaigand;
typedef struct simpleaigstats simpleaigstats;
typedef struct bucket bucket;

struct simpleaig
{
  int max_var;
  unsigned num_inputs;
  unsigned num_outputs;
  unsigned num_ands;
  unsigned long long num_ands_total;  /* not reset on simpleaig_reset_ands  */
  unsigned long long num_ands_shared;

  int lhs_aux;

  unsigned inputs_size;
  unsigned outputs_size;
  unsigned ands_size;

  int *inputs;         /* [0 ... num_inputs[  */
  int *outputs;        /* [0 ... num_outputs[  */
  simpleaigand *ands;  /* [0 ... num_ands[  */

  unsigned buckets_size;
  bucket *buckets;
};

struct simpleaigand
{
  int lhs;
  int rhs0;
  int rhs1;
};

struct simpleaigstats
{
  unsigned long long num_ands;         /* number of and gates  */
  unsigned long long num_ands_shared;  /* number of and gates shared  */
  unsigned num_buckets;                /* number of hash table buckets  */
  unsigned num_empty_buckets;          /* number of empty hash table buckets */
  double avg_bucket_cnt;
  unsigned min_bucket_cnt;
  unsigned max_bucket_cnt;
  unsigned min_bucket_size;
  unsigned max_bucket_size;
  double avg_bucket_size;
  size_t mem_ands_alloc;
  size_t mem_ands_used;
  size_t mem_hash_alloc;
  size_t mem_hash_used;
  size_t mem_total_alloc;
  size_t mem_total_used;
};

simpleaig * simpleaig_init (void);   /* constructor  */
void simpleaig_reset (simpleaig *);  /* destructor  */

/**
 * Set expected number of buckets for the hash table. No bucket resize and 
 * reshashing is performed.
 * E.g., buckets_size = expected number of and gates
 */
void simpleaig_set_buckets (simpleaig *, unsigned buckets_size);

/**
 * Only reset and gates and hash table (required for incremental aig 
 * construction).
 * The total number of and gates of an incrementally built AIG is stored in
 * simpleaig.num_ands_total.
 */
void simpleaig_reset_ands (simpleaig *);

/**
 * Add a new and gate with rhs0 /\ rhs1 and return the lhs index.
 * If lhs is set to SIMPLEAIG_FALSE, the lhs of a new and gate is set to a new 
 * auxiliary index. Otherwise, the lhs of the and gate is set to the given 
 * value of lhs (i.e. the output of an and gate is forced to a certain value).
 *
 * If hashing is enabled an and gate with rhs0 /\ rhs1 (resp. rhs1 /\ rhs0) is 
 * only added once. Each successive call with the same rhs0/rhs1 values 
 * returns the lhs of the hashed and gate unless lhs is forced to a value.
 */
int simpleaig_add_and (simpleaig *, int lhs, int rhs0, int rhs1);

/**
 * Add primary input/output of aig.
 */
void simpleaig_add_input (simpleaig *, int input);
void simpleaig_add_output (simpleaig *, int output);

/**
 * Negate given literal.
 */
int simpleaig_not (int lit);

/**
 * Transform aig into a CNF via Tseitin encoding and write it to out.
 */
void simpleaig_write_cnf_to_file (simpleaig *, FILE *out);

/**
 * Write aig in AIGER format (binary/ascii) to out.
 */
void simpleaig_write_aiger_to_file (simpleaig *, FILE *out, char binary);

/**
 * Only write AIGER header to out. If dummy is set to 1 a placeholder header
 * is written to out, which can be replaced by the real header if all header
 * information is available (required for incremental aig construction).
 */
void simpleaig_write_aiger_header (simpleaig *, FILE *out, char binary, 
                                   char dummy);
/**
 * Only write inputs and outputs to out (only for ascii AIGER). 
 */
void simpleaig_write_aiger_ios (simpleaig *, FILE *out);

/** 
 * Write and gates to out (only for ascii AIGER).
 */
void simpleaig_write_aiger_ands (simpleaig *, FILE *out);

/**
 * Re-encode all auxiliary and gate indices with given offset 
 * (i.e. new_index = index + offset. Input and output indices are not 
 * re-encoded.
 */
void simpleaig_reencode_aux_ands (simpleaig *, int offset);

/**
 * Read binary/ascii AIGER from given file. If hashing is enabled, all parsed
 * and gates will be hashed.
 */
const char * simpleaig_read_aiger_from_file (simpleaig *, char *filename,
                                             char hashing);

/**
 * Compute some useful statistics for given AIG.
 */
void simpleaig_statistics (simpleaig *, simpleaigstats *);

#endif 
