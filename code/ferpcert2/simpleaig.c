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

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include "simpleaig.h"

#define INIT_ANDS_SIZE 32
#define INIT_BUCKET_SIZE 2
#define INIT_INPUTS_SIZE 8
#define INIT_OUTPUTS_SIZE 8
#define ANDS_RESIZE_VALUE(size) ((size) >> 2)
#define BUCKETS_RESIZE_VALUE(size) (((size) >> 2) + 1)
#define INPUTS_RESIZE_VALUE(size) ((size) >> 2)
#define OUTPUTS_RESIZE_VALUE(size) ((size) >> 2)

#define PARSER_ABORT(cond, msg) \
  if (cond) return msg

#define PARSER_SKIP_SPACE_DO_WHILE(c) \
  do                                  \
  {                                   \
    c = _getc ();                     \
  }                                   \
  while (isspace (c))
  
#define PARSER_SKIP_SPACE_WHILE(c) \
  while (isspace (c)) c = _getc ()

#define PARSER_READ_NUM(num, c, msg) \
  do                                 \
  {                                  \
    num = 0;                         \
    for (;;)                         \
    {                                \
      if (!isdigit (c))              \
        PARSER_ABORT (1, msg);       \
      num = num * 10 + (c - '0');    \
      if (isspace (c = _getc ()))    \
        break;                       \
    }                                \
  }                                  \
  while (0)

static int simpleaig_lookup (simpleaig *, int, int);
static unsigned aiger_lit (int);
static int simpleaig_lit (unsigned);
static int simpleaig_reencode_lit (int *, int);
static void simpleaig_encode (FILE *, unsigned);
static const char * simpleaig_decode (unsigned *);
static void simpleaig_write_binary_aiger_to_file (simpleaig *, FILE *);
static const char * 
simpleaig_read_ascii_aiger (simpleaig *, unsigned, unsigned, unsigned);
static const char * 
simpleaig_read_binary_aiger (simpleaig *, unsigned, unsigned, unsigned);

struct bucket
{
  unsigned cnt;
  unsigned size;
  unsigned *collisions;
};

static char *in_mmap = NULL;
static size_t in_mmap_size = 0;
static unsigned long in_mmap_pos = 0;
static int (*_getc)(void) = NULL;

simpleaig *
simpleaig_init (void)
{
  simpleaig *aig = NULL;

  aig = (simpleaig *) malloc (sizeof (simpleaig));
  assert (aig != NULL);
  memset (aig, 0, sizeof (simpleaig));

  aig->ands_size = INIT_ANDS_SIZE;
  aig->inputs_size = INIT_INPUTS_SIZE;
  aig->outputs_size = INIT_OUTPUTS_SIZE;
  aig->buckets = NULL;

  aig->ands = (simpleaigand *) malloc (aig->ands_size * sizeof (simpleaigand));
  assert (aig->ands != NULL);
  aig->inputs = (int *) malloc (aig->inputs_size * sizeof (int));
  assert (aig->inputs != NULL);
  aig->outputs = (int *) malloc (aig->outputs_size * sizeof (int));
  assert (aig->outputs != NULL);

  return aig;
}

void
simpleaig_set_buckets (simpleaig *aig, unsigned buckets_size)
{
  assert (aig != NULL);
  assert (buckets_size > 0);

  unsigned i;

  /* get next power of two of size  */
  buckets_size -= 1;
  buckets_size |= buckets_size >> 1;
  buckets_size |= buckets_size >> 2;
  buckets_size |= buckets_size >> 4;
  buckets_size |= buckets_size >> 8;
  buckets_size |= buckets_size >> 16;
  buckets_size += 1;
  /* reduce bucket size by 4, otherwise there are too many empty buckets  */
  buckets_size = (buckets_size >= 4) ? (buckets_size >> 2) : 2;
  assert (buckets_size > 0);

  aig->buckets_size = buckets_size;
  aig->buckets = (bucket *) malloc (buckets_size * sizeof (bucket));
  assert (aig->buckets != NULL);

  for (i = 0; i < buckets_size; i++)
  {
    aig->buckets[i].size = INIT_BUCKET_SIZE;
    aig->buckets[i].cnt = 0;
    aig->buckets[i].collisions = 
      (unsigned *) malloc (INIT_BUCKET_SIZE * sizeof (unsigned));
    assert (aig->buckets[i].collisions != NULL);
  }

}

void
simpleaig_reset (simpleaig *aig)
{
  assert (aig != NULL);

  unsigned i;

  if (aig->buckets != NULL)
  {
    for (i = 0; i < aig->buckets_size; i++)
      free (aig->buckets[i].collisions);
  }

  free (aig->ands);
  free (aig->inputs);
  free (aig->outputs);
  free (aig->buckets);
  free (aig);
}

void
simpleaig_reset_ands (simpleaig *aig)
{
  assert (aig != NULL);

  unsigned i;

  /* reset hash table  */
  if (aig->buckets != NULL)
  {
    for (i = 0; i < aig->buckets_size; i++)
      free (aig->buckets[i].collisions);

    free (aig->buckets);
    aig->buckets_size = 0;
    aig->buckets = NULL;
  }

  aig->num_ands = 0;
  aig->ands_size = INIT_ANDS_SIZE;
  aig->ands = 
    (simpleaigand *) realloc (aig->ands, 
                              aig->ands_size * sizeof (simpleaigand));
  assert (aig->ands != NULL);
}

int
simpleaig_add_and (simpleaig *aig, int lhs, int rhs0, int rhs1)
{
  assert (aig != NULL);
  assert (lhs != SIMPLEAIG_FALSE || aig->lhs_aux > 0);
  assert (lhs >= 0);

  if (rhs0 > rhs1)
  {
    rhs0 ^= rhs1;
    rhs1 ^= rhs0;
    rhs0 ^= rhs1;
  }

  /* hashtable lookup  */
  if (aig->buckets != NULL)
  {
    if (lhs == SIMPLEAIG_FALSE)
    {
      lhs = simpleaig_lookup (aig, rhs0, rhs1);

      if (lhs != SIMPLEAIG_FALSE)
      {
        aig->num_ands_shared += 1;
        return lhs;
      }
    }
    /* just hash and-gate  */
    else
      simpleaig_lookup (aig, rhs0, rhs1);
  }

  /* create new auxiliary and  */
  if (lhs == SIMPLEAIG_FALSE)
  {
    lhs = aig->lhs_aux;
    aig->lhs_aux += 1;
  }

  if (aig->num_ands == aig->ands_size)
  {
    aig->ands_size += ANDS_RESIZE_VALUE(aig->ands_size);
    aig->ands = 
      (simpleaigand *) realloc (aig->ands, 
                                aig->ands_size * sizeof (simpleaigand));
    assert (aig->ands != NULL);
  }

  /* if lhs is negative it may have occured a integer overflow  */
  assert (lhs > 0);

  if (lhs > aig->max_var)
    aig->max_var = lhs;

  aig->ands[aig->num_ands].lhs = lhs; 
  aig->ands[aig->num_ands].rhs0 = rhs0; 
  aig->ands[aig->num_ands].rhs1 = rhs1; 
  aig->num_ands += 1;
  aig->num_ands_total += 1;

  assert (lhs != SIMPLEAIG_FALSE);
  assert (lhs != SIMPLEAIG_TRUE);

  return lhs;
}

void
simpleaig_add_input (simpleaig *aig, int input)
{
  assert (aig != NULL);
  assert (input > 0);

  if (aig->num_inputs == aig->inputs_size)
  {
    aig->inputs_size += INPUTS_RESIZE_VALUE (aig->inputs_size);
    aig->inputs = 
      (int *) realloc (aig->inputs, aig->inputs_size * sizeof (int));
    assert (aig->inputs != NULL);
  }

  if (input > aig->max_var)
    aig->max_var = input;

  aig->inputs[aig->num_inputs] = input;
  aig->num_inputs += 1;
}

void
simpleaig_add_output (simpleaig *aig, int output)
{
  assert (aig != NULL);
  assert (output != SIMPLEAIG_TRUE);
  assert (output != SIMPLEAIG_FALSE);

  if (aig->num_outputs == aig->outputs_size)
  {
    aig->outputs_size += OUTPUTS_RESIZE_VALUE (aig->outputs_size);
    aig->outputs = 
      (int *) realloc (aig->outputs, aig->outputs_size * sizeof (int));
    assert (aig->outputs != NULL);
  }

  /* output may be negated  */
  if (abs (output) > aig->max_var)
    aig->max_var = abs (output);

  aig->outputs[aig->num_outputs] = output;
  aig->num_outputs += 1;
}

int
simpleaig_not (int lit)
{
  if (lit == SIMPLEAIG_FALSE)
    return SIMPLEAIG_TRUE;
  else if (lit == SIMPLEAIG_TRUE)
    return SIMPLEAIG_FALSE;

  return -lit;
}

static int
simpleaig_lookup (simpleaig *aig, int rhs0, int rhs1)
{
  unsigned i, index;
  bucket *bucket;
  simpleaigand *ands;

  ands = aig->ands;
  bucket = aig->buckets + (((rhs0 * 17) ^ rhs1) & (aig->buckets_size - 1));

  for (i = 0; i < bucket->cnt; i++)
  {
    index = bucket->collisions[i];

    if (ands[index].rhs0 == rhs0 && ands[index].rhs1 == rhs1)
      return ands[index].lhs;
  }

  if (bucket->cnt + 1 >= bucket->size)
  {
    bucket->size += BUCKETS_RESIZE_VALUE(bucket->size);
    bucket->collisions = 
      (unsigned *) realloc (bucket->collisions,
                            bucket->size * sizeof (unsigned));
    assert (bucket->collisions != NULL);
  }

  bucket->collisions[bucket->cnt++] = aig->num_ands;

  return SIMPLEAIG_FALSE;
}

void
simpleaig_write_aiger_to_file (simpleaig *aig, FILE *out, char binary)
{
  assert (aig != NULL);
  assert (out != NULL);

  if (binary)
  {
    simpleaig_write_binary_aiger_to_file (aig, out);
  }
  else
  {
    simpleaig_write_aiger_header (aig, out, 0, 0);
    simpleaig_write_aiger_ios (aig, out);
    simpleaig_write_aiger_ands (aig, out);
  }
}

static unsigned
aiger_lit (int lit)
{
  char is_neg = 0;

  if (lit == SIMPLEAIG_TRUE)
    return 1;

  if (lit == SIMPLEAIG_FALSE)
    return 0;

  if (lit < 0)
  {
    is_neg = 1;
    lit = -lit;
  }

  return (((unsigned) lit) << 1) | is_neg;
}

void
simpleaig_write_aiger_header (simpleaig *aig, FILE *out, char binary, 
                              char dummy)
{
  assert (aig != NULL);
  assert (out != NULL);

  if (dummy)
  {
    if (binary) fprintf (out, "%-50s\n", "aig dummy");
    else        fprintf (out, "%-50s\n", "aag dummy");
  }
  else
  {
    rewind (out);
    fprintf (out, "%s %u %u %u %u %llu\n", 
             binary ? "aig" : "aag",
             aig->max_var,
             aig->num_inputs,
             0,
             aig->num_outputs,
             aig->num_ands_total);
  }
}

void
simpleaig_write_aiger_ios (simpleaig *aig, FILE *out)
{
  assert (aig != NULL);
  assert (out != NULL);

  unsigned i;

  for (i = 0; i < aig->num_inputs; i++)
    fprintf (out, "%u\n", aiger_lit (aig->inputs[i]));

  for (i = 0; i < aig->num_outputs; i++)
    fprintf (out, "%u\n", aiger_lit (aig->outputs[i]));
}

void
simpleaig_write_aiger_ands (simpleaig *aig, FILE *out)
{
  assert (aig != NULL);
  assert (out != NULL);

  unsigned i;

  for (i = 0; i < aig->num_ands; i++)
  {
    fprintf (out, "%u %u %u\n", 
             aiger_lit (aig->ands[i].lhs),
             aiger_lit (aig->ands[i].rhs0),
             aiger_lit (aig->ands[i].rhs1));
  }
}

static int
simpleaig_lit (unsigned lit)
{
  int sign = ((lit & 1) == 0) ? 1 : -1;

  if (lit == 0)
    return SIMPLEAIG_FALSE; 
  else if (lit == 1)
    return SIMPLEAIG_TRUE;

  return ((int) (lit >> 1)) * sign;
}

static int
cnf_lit (simpleaig *aig, int lit)
{
  if (lit == SIMPLEAIG_TRUE)
    return aig->max_var + 1; 
  else if (lit == SIMPLEAIG_FALSE)
    return -(aig->max_var + 1);

  return lit;
}

void
simpleaig_write_cnf_to_file (simpleaig *aig, FILE *out)
{
  assert (aig != NULL);
  assert (out != NULL);

  int lhs, rhs0, rhs1;
  unsigned i;

  fprintf (out, "p cnf %u %u\n", aig->max_var + 1, 3 * aig->num_ands + 2);

  for (i = 0; i < aig->num_ands; i++)
  {
    lhs = (aig->ands[i]).lhs;
    rhs0 = (aig->ands[i]).rhs0;
    rhs1 = (aig->ands[i]).rhs1;

      fprintf (out, "%d %d 0\n", cnf_lit (aig, simpleaig_not (lhs)),
                                 cnf_lit (aig, rhs0));

      fprintf (out, "%d %d 0\n", cnf_lit (aig, simpleaig_not (lhs)),
                                 cnf_lit (aig, rhs1));

      fprintf (out, "%d %d %d 0\n", cnf_lit (aig, lhs), 
                                    cnf_lit (aig, simpleaig_not (rhs0)), 
                                    cnf_lit (aig, simpleaig_not (rhs1)));
  }

  fprintf (out, "%d 0\n", cnf_lit (aig, SIMPLEAIG_TRUE));
  fprintf (out, "%d 0\n", cnf_lit (aig, aig->outputs[0]));
}

static int mmap_getc (void)
{
  if (in_mmap_pos == in_mmap_size)
    return EOF;

  return (unsigned char) in_mmap[in_mmap_pos++];   /* return 0 - 255  */
}

static int stdin_getc (void)
{
  return getc (stdin);
}

static const char *
simpleaig_decode_getc (unsigned char *ch)
{
  assert (_getc != NULL);

  int c = _getc ();
  if (c == EOF) return "*** decode: unexptected EOF\n";

  *ch = c;
  return NULL;
}

static const char *
simpleaig_decode (unsigned *num)
{
  assert (_getc != NULL);

  unsigned i = 0;
  unsigned char c;
  const char *error;

  *num = 0;
  while (!(error = simpleaig_decode_getc (&c)) && (c & 0x80))
    *num |= (c & 0x7f) << (7 * i++);

  if (error) return error;

  *num |= (c << (7 * i));
  return NULL;
}

static const char *
simpleaig_read_binary_aiger (simpleaig *aig, unsigned num_inputs, 
                             unsigned num_outputs, unsigned num_ands)
{
  assert (aig != NULL);
  assert (_getc != NULL);

  char c;
  unsigned i, j, num, next_lhs, lhs, rhs0, rhs1, delta;
  int *newenc;
  const char *error;

  /* read inputs (implicitely defined)  */
  for (i = 0; i < num_inputs; i++)
    simpleaig_add_input (aig, i + 1);  // simpleaig_lit (2 * (i + 1))

  /* read outputs  */
  for (i = 0; i < num_outputs; i++)
  {
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (num, c, "output index expected");
    PARSER_ABORT (num <= num_inputs, "invalid output index");
    simpleaig_add_output (aig, simpleaig_lit (num));
  }

  next_lhs = num_inputs + 1;
  /* read and gates  */
  for (i = 0; i < num_ands; i++)
  {
    /* restore delta encoded and gate  */
    lhs = 2 * next_lhs++;
    error = simpleaig_decode (&delta); 

    if (error) return error;

    rhs0 = lhs - delta;   /* rhs0 = lhs - delta0  */
    error = simpleaig_decode (&delta);

    if (error) return error;

    rhs1 = rhs0 - delta;  /* rhs1 = rhs0 - delta1  */

    simpleaig_add_and (aig, simpleaig_lit (lhs), simpleaig_lit (rhs0), 
                       simpleaig_lit (rhs1));
  }

  newenc = (int *) malloc ((aig->max_var + 1) * sizeof (int));
  assert (newenc != NULL);
  memset (newenc, 0, (aig->max_var + 1) * sizeof (int));

  next_lhs = 0;
  /* read symbols for inputs/outputs  */
  for (i = 0; i < num_inputs; i++)
  {
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_ABORT (c != 'i', "no symbol given for input");
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (j, c, "input symbol index expected");
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (num, c, "input index expected");
    PARSER_ABORT(j >= num_inputs, "invalid input symbol index");

    newenc[aig->inputs[j]] = num;
    aig->inputs[j] = num;

    if (num >= next_lhs)
      next_lhs = num + 1;

    if (num > (unsigned) aig->max_var)
      aig->max_var = num;
  }

  for (i = 0; i < num_outputs; i++)
  {
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_ABORT (c != 'o', "no symbol given for output");
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (j, c, "output symbol index expected");
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (num, c, "output index expected");
    PARSER_ABORT(j >= num_outputs, "invalid output symbol index");

    newenc[abs (aig->outputs[j])] = num;
    aig->outputs[j] = num;
  
    if (num >= next_lhs)
      next_lhs = num + 1;
  }

  /* restore input/output indices in order to match variable indices in
     input formula  */
  for (i = 0; i < num_ands; i++)
  {
    if (newenc[aig->ands[i].lhs] == 0)
      newenc[aig->ands[i].lhs] = next_lhs++;

    aig->ands[i].lhs = simpleaig_reencode_lit (newenc, aig->ands[i].lhs);
    aig->ands[i].rhs0 = simpleaig_reencode_lit (newenc, aig->ands[i].rhs0);
    aig->ands[i].rhs1 = simpleaig_reencode_lit (newenc, aig->ands[i].rhs1);

    if (aig->ands[i].lhs > aig->max_var)
      aig->max_var = aig->ands[i].lhs;
  }

  free (newenc);

  return NULL;
}

const char *
simpleaig_read_ascii_aiger (simpleaig *aig, unsigned num_inputs,
                            unsigned num_outputs, unsigned num_ands)
{
  assert (aig != NULL);
  assert (_getc != NULL);

  char c;
  unsigned i, num, lhs, rhs0, rhs1;

  /* read inputs  */
  for (i = 0; i < num_inputs; i++)
  {
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (num, c, "input index expected");
    simpleaig_add_input (aig, simpleaig_lit (num));
  }

  /* read outputs  */
  for (i = 0; i < num_outputs; i++)
  {
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (num, c, "output index expected");
    PARSER_ABORT(simpleaig_lit (num) == SIMPLEAIG_TRUE ||
                 simpleaig_lit (num) == SIMPLEAIG_FALSE,
                 "invalid output index");
    simpleaig_add_output (aig, simpleaig_lit (num));
  }

  /* read and gates  */
  for (i = 0; i < num_ands; i++)
  {
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (lhs, c, "lhs of AND gate expected"); 
    PARSER_ABORT (lhs <= 0, "lhs has to be greater 0");
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (rhs0, c, "first rhs of AND gate expected"); 
    PARSER_SKIP_SPACE_DO_WHILE (c);
    PARSER_READ_NUM (rhs1, c, "second rhs of AND gate expected"); 

    simpleaig_add_and (aig, simpleaig_lit (lhs), simpleaig_lit (rhs0), 
                       simpleaig_lit (rhs1));
  }

  return NULL;
}

const char *
simpleaig_read_aiger_from_file (simpleaig *aig, char *filename, char hashing)
{
  assert (aig != NULL);

  char c, read_binary = 0;
  const char *error;
  unsigned max_var, num, num_inputs, num_outputs, num_ands;
  struct stat s;
  int in_mmap_fd;

  if (filename != NULL)
  {
    in_mmap_fd = open (filename, O_RDONLY); 
    PARSER_ABORT (in_mmap_fd == -1, "failed to open aiger file");
    PARSER_ABORT (fstat (in_mmap_fd, &s) == -1, "failed to get file status");
    in_mmap_size = s.st_size;
    in_mmap = (char *) mmap (0, in_mmap_size, PROT_READ, 
                             MAP_PRIVATE | MAP_NORESERVE, in_mmap_fd, 0);
    PARSER_ABORT (in_mmap == MAP_FAILED, "failed to mmap aiger file");
    close (in_mmap_fd);

    _getc = mmap_getc;
  }
  else
  {
    _getc = stdin_getc;
  }

  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_ABORT (c != 'a', "invalid header");
  PARSER_SKIP_SPACE_DO_WHILE (c);
  if (c == 'i') read_binary = 1;
  PARSER_ABORT (c != 'a' && c != 'i', "invalid header");
  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_ABORT (c != 'g', "invalid header");

  /* read M I L O A  */
  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_READ_NUM (max_var, c, "maximum variable index expected");
  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_READ_NUM (num_inputs, c, "number of inputs expected");
  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_READ_NUM (num, c, "number of latches expected");
  PARSER_ABORT (num != 0, "number of latches expected to be 0");
  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_READ_NUM (num_outputs, c, "number of outputs expected");
  PARSER_SKIP_SPACE_DO_WHILE (c);
  PARSER_READ_NUM (num_ands, c, "number of AND gates expected");

  if (hashing)
    simpleaig_set_buckets (aig, num_ands / 16 + 1);

  if (read_binary)
    error = 
      simpleaig_read_binary_aiger (aig, num_inputs, num_outputs, num_ands);
  else
    error = simpleaig_read_ascii_aiger (aig, num_inputs, num_outputs, num_ands);

  aig->lhs_aux = aig->max_var + 1;

  if (in_mmap != NULL)
    munmap (in_mmap, in_mmap_size);

  return error;
}

void
simpleaig_statistics (simpleaig *aig, simpleaigstats *stats)
{
  assert (aig != NULL);
  assert (stats != NULL);

  unsigned i, buckets_cnt_total = 0, buckets_size_total = 0, min = UINT_MAX;
  unsigned max = 0, min_size = UINT_MAX, max_size = 0;

  memset (stats, 0, sizeof (simpleaigstats));

  for (i = 0; i < aig->buckets_size; i++)
  {
    if (aig->buckets[i].cnt == 0)
      stats->num_empty_buckets += 1;
    else
    {
      if (aig->buckets[i].cnt < min)
        min = aig->buckets[i].cnt;

      if (aig->buckets[i].cnt > max)
        max = aig->buckets[i].cnt;

      if (aig->buckets[i].size < min_size)
        min_size = aig->buckets[i].size;

      if (aig->buckets[i].size > max_size)
        max_size = aig->buckets[i].size;
    }
    buckets_cnt_total += aig->buckets[i].cnt;
    buckets_size_total += aig->buckets[i].size; 
  }

  stats->num_ands = aig->num_ands_total;
  stats->num_ands_shared = aig->num_ands_shared;
  stats->num_buckets = aig->buckets_size;
  stats->avg_bucket_cnt = 
    (double) buckets_cnt_total / (aig->buckets_size - stats->num_empty_buckets);
  stats->min_bucket_cnt = (min == UINT_MAX) ? 0 : min;
  stats->max_bucket_cnt = max;
  stats->min_bucket_size = (min_size == UINT_MAX) ? 0 : min_size;
  stats->max_bucket_size = max_size;
  stats->avg_bucket_size = 
    (double) buckets_size_total / aig->buckets_size;

  stats->mem_ands_alloc = aig->ands_size * sizeof (simpleaigand);
  stats->mem_ands_used = aig->num_ands * sizeof (simpleaigand);

  stats->mem_hash_alloc = aig->buckets_size * sizeof (bucket) + 
    buckets_size_total * sizeof (unsigned);
  stats->mem_hash_used = 
    (aig->buckets_size - stats->num_empty_buckets) * sizeof (bucket) + 
    buckets_cnt_total * sizeof (unsigned);

  stats->mem_total_alloc = sizeof (simpleaig) + aig->num_inputs * sizeof (int) +
    aig->num_outputs * sizeof (int) + stats->mem_ands_alloc + 
    stats->mem_hash_alloc;
  stats->mem_total_used = sizeof (simpleaig) + aig->num_inputs * sizeof (int) +
    aig->num_outputs * sizeof (int) + stats->mem_ands_used + 
    stats->mem_hash_used;
}

static int
simpleaig_reencode_lit (int *newenc, int lit)
{
  assert (newenc != NULL);

  if (lit == SIMPLEAIG_TRUE || lit == SIMPLEAIG_FALSE)
    return lit;

  assert (newenc[abs (lit)] > 0);

  if (lit < 0)
    return -newenc[-lit];

  return newenc[lit];
}

static void
simpleaig_encode (FILE *out, unsigned num)
{
  unsigned char c;

  while (num & ~0x7f)
  {
    c = (num & 0x7f) | 0x80;
    putc (c, out);
    num >>= 7;
  }
     
  c = num;
  putc (c, out);
}

static void
simpleaig_write_binary_aiger_to_file (simpleaig *aig, FILE *out)
{
  assert (aig != NULL);
  assert (out != NULL);

  unsigned i, lhs, rhs0, rhs1;
  int *newenc, new_index = 1;

  newenc = (int *) malloc ((aig->max_var + 1) * sizeof (int));
  assert (newenc != NULL);
  memset (newenc, 0, (aig->max_var + 1) * sizeof (int));

  /* re-encode inputs  */
  for (i = 0; i < aig->num_inputs; i++)
  {
    assert (newenc[aig->inputs[i]] == 0);
    newenc[aig->inputs[i]] = new_index++;
  }
  
  /* re-encode and gates  */
  for (i = 0; i < aig->num_ands; i++)
  {
    assert (newenc[aig->ands[i].lhs] == 0);
    newenc[aig->ands[i].lhs] = new_index++;
  }

  aig->max_var = new_index - 1;

  simpleaig_write_aiger_header (aig, out, 1, 0);

  /* write outputs  */
  for (i = 0; i < aig->num_outputs; i++)
    fprintf (out, "%u\n",
             aiger_lit (simpleaig_reencode_lit (newenc, aig->outputs[i])));

  /* write and gates  */
  for (i = 0; i < aig->num_ands; i++)
  {
    lhs = aiger_lit (simpleaig_reencode_lit (newenc, aig->ands[i].lhs));
    rhs0 = aiger_lit (simpleaig_reencode_lit (newenc, aig->ands[i].rhs0));
    rhs1 = aiger_lit (simpleaig_reencode_lit (newenc, aig->ands[i].rhs1));

    if (rhs0 < rhs1)
    {
      rhs0 ^= rhs1;
      rhs1 ^= rhs0;
      rhs0 ^= rhs1;
    }
    assert (lhs > rhs0);
    assert (rhs0 >= rhs1);
    simpleaig_encode (out, lhs - rhs0);  /* delta0 = lhs - rhs0  */
    simpleaig_encode (out, rhs0 - rhs1); /* delta1 = rhs0 - rhs1  */
  }

  /* write symbols for input/outputs  */
  for (i = 0; i < aig->num_inputs; i++)
    fprintf (out, "i%u %d\n", i, aig->inputs[i]);
  /* outputs may be negative  */
  for (i = 0; i < aig->num_outputs; i++)
    fprintf (out, "o%u %d\n", i, abs (aig->outputs[i]));

  free (newenc);
}

void
simpleaig_reencode_aux_ands (simpleaig *aig, int offset)
{
  assert (aig != NULL);
  assert (offset > 0);

  char *is_io;
  unsigned i;
  int lhs, rhs0, rhs1;

  is_io = (char *) malloc ((aig->max_var + 1) * sizeof (char));
  assert (is_io != NULL);
  memset (is_io, 0, (aig->max_var + 1) * sizeof (char));

  for (i = 0; i < aig->num_inputs; i++)
    is_io[aig->inputs[i]] = 1;

  for (i = 0; i < aig->num_outputs; i++)
    is_io[abs (aig->outputs[i])] = 1;

  /* re-encode non-output and gates  */
  for (i = 0; i < aig->num_ands; i++)
  {
    lhs = (aig->ands[i]).lhs;
    rhs0 = (aig->ands[i]).rhs0;
    rhs1 = (aig->ands[i]).rhs1;

    assert (lhs > 0);
    if (!is_io[lhs])
    {
      lhs += offset;
      aig->ands[i].lhs = lhs;

      if (lhs > aig->max_var)
        aig->max_var = lhs;
    }

    if (rhs0 != SIMPLEAIG_FALSE && rhs0 != SIMPLEAIG_TRUE && !is_io[abs (rhs0)])
    {
      rhs0 += (rhs0 < 0) ? -offset : offset;
      aig->ands[i].rhs0 = rhs0;

      if (abs (rhs0) > aig->max_var)
        aig->max_var = rhs0;
    }

    if (rhs1 != SIMPLEAIG_FALSE && rhs1 != SIMPLEAIG_TRUE && !is_io[abs (rhs1)])
    {
      rhs1 += (rhs1 < 0) ? -offset : offset;
      aig->ands[i].rhs1 = rhs1;

      if (abs (rhs1) > aig->max_var)
        aig->max_var = rhs1;
    }
  }

  aig->lhs_aux += offset;
  free (is_io);
}
