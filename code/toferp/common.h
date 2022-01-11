//
// Created by vedad on 27/06/18.
//

#ifndef NANOQBF_COMMON_H
#define NANOQBF_COMMON_H

#include <stdlib.h>

typedef int             Lit;
typedef int*            lit_iterator;
typedef const int*      const_lit_iterator;

typedef unsigned        Var;
typedef unsigned*       var_iterator;
typedef const unsigned* const_var_iterator;

/// Returns variable corresponding to literal \a l
inline Var var(Lit l)
{
  return std::abs(l);
}

/// Returns true if literal \a l is negated
inline bool sign(Lit l)
{
  return l < 0;
}

/// Constructs literal from variable \a v and \a sign
inline Lit make_lit(Var v, bool sign)
{
  return (Lit)((v ^ -((Lit)sign)) + (Lit)sign);
}

/// Defines a custom order relation for literals
inline bool lit_order(Lit a, Lit b)
{
  const Var va = var(a);
  const Var vb = var(b);
  return va < vb || (va == vb && a < b);
}

/// Enum representing quantifier types in QBF
enum QuantType
{
  NONE,
  EXISTS,
  FORALL
};


#endif //NANOQBF_COMMON_H
