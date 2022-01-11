//
// Created by vedad on 21/08/18.
//

#ifndef TOFERP_VARMENAGER_H
#define TOFERP_VARMENAGER_H

#include <vector>
#include <map>
#include <set>

#include "common.h"

class VarManager
{
private:
  std::map<Var,Var> prop_to_original;
  std::map<Var, Var> prop_to_ferp;
  std::map<Var, std::vector<Lit>*> prop_to_annotation;
  std::vector<std::vector<Lit>*> annotations;
  std::vector<bool> occurring_prop;
  std::vector<uint32_t> clause_origin;
public:
  inline VarManager();
  ~VarManager();
  
  int addVariables(const std::vector<Var>& prop, const std::vector<Var>& orig, const std::vector<Lit>& anno);
  void computeNames();
  void writeExpansions(FILE* file);
  inline void pushClauseOrigin(uint32_t cl);
  inline void addOccurence(Var v);
  inline uint32_t numClauseOrigins() const;
  inline uint32_t getClauseOrigin(uint32_t clause_id) const;
  inline Lit getLitFerp(const Lit l) const;
};

//////////// INLINE IMPLEMENTATIONS ////////////

VarManager::VarManager()
{
  clause_origin.push_back(0);
}

void VarManager::pushClauseOrigin(uint32_t cl)
{
  clause_origin.push_back(cl);
}

void VarManager::addOccurence(Var v)
{
  if (v >= occurring_prop.size())
    occurring_prop.resize(v + 1, false);
  occurring_prop[v] = true;
}

uint32_t VarManager::numClauseOrigins() const
{
  return (uint32_t)clause_origin.size() - 1;
}

uint32_t VarManager::getClauseOrigin(uint32_t clause_id) const
{
  return clause_origin[clause_id];
}

Lit VarManager::getLitFerp(const Lit l) const
{
  auto it = prop_to_ferp.find(var(l));
  if(it == prop_to_ferp.end()) return 0;
  return make_lit(it->second, sign(l));
}

#endif //TOFERP_VARMENAGER_H
