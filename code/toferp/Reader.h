//
// Created by vedad on 07/10/17.
//

#ifndef NANOQBF_READER_H
#define NANOQBF_READER_H

#include "parseutils.h"
#include "common.h"
#include "VarManager.h"
#include <vector>

/// Reader class for parsing files
class Reader
{
protected:
  StreamBuffer stream; ///< Input stream
  
  /// Parses an unsigned integer, which is a Var
  int parseUnsigned(unsigned& ret);
  
  /// Parses a signed integer, which is a Lit
  int parseSigned(int& ret);
public:
  /// Reader Constructor
  Reader (gzFile& file) : stream(file) {}
};

class AnnotationReader : protected Reader
{
private:
  std::vector<Var> propositional;       ///< Variables appearing in the CNF
  std::vector<Var> original;            ///< Variables appearing in the QBF
  std::vector<Lit> annotation;          ///< Annotation for propositional variables
  std::vector<Var> clause_origin;       ///< Stores which CNF clause comes from which QBF clause
public:
  /// Reads the important parts of the CNF and stores it in \a mngr
  int readCNF(VarManager& mngr);
  
  AnnotationReader(gzFile& file) : Reader(file) {};
};

class TraceReader : protected Reader
{
private:
  std::vector<std::vector<Lit>> trace_clauses;      ///< Clauses as they appear in the trace
  std::vector<uint32_t> trace_id_to_cnf_id;         ///< Clause ids as they appear in the trace
  std::map<uint32_t, uint32_t> cnf_id_to_trace_id;  ///< Lookup table in other direction
  std::vector<std::array<uint32_t, 2>> antecedents;             ///< Stores anteceedents of each trace clause
  
  int readClause();
public:
  /// Reads the important parts of the CNF and stores it in \a mngr
  int readTrace(VarManager& mngr);
  void writeTrace(VarManager& mngr, FILE* file);
  
  TraceReader(gzFile& file) : Reader(file) {};
};




#endif // NANOQBF_READER_H
