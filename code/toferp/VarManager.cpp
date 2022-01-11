//
// Created by vedad on 21/08/18.
//

#include <stdint.h>
#include <stdio.h>

#include "VarManager.h"

VarManager::~VarManager()
{
  for(std::vector<Lit>* a : annotations)
    delete a;
}

int VarManager::addVariables(const std::vector<Var>& prop, const std::vector<Var>& orig, const std::vector<Lit>& anno)
{
  std::vector<Lit>* anno_ptr = new std::vector<Lit>(anno);
  annotations.push_back(anno_ptr);
  
  bool success = true;
  for(uint32_t i = 0; success && i < prop.size(); i++)
    success &= prop_to_original.insert(std::pair<Var, Var>(prop[i], orig[i])).second;
  
  if(!success) return 1;
  
  for(uint32_t i = 0; i < prop.size(); i++)
    prop_to_annotation.insert(std::pair<Var, std::vector<Lit>*>(prop[i], anno_ptr));
  
  return 0;
}

void VarManager::computeNames()
{
  uint32_t fv = 1;
  for(uint32_t pv = 0; pv < occurring_prop.size(); pv++)
    if(occurring_prop[pv])
      prop_to_ferp[pv] = fv++;
}

void VarManager::writeExpansions(FILE* file)
{
  std::map<std::vector<Lit>*, uint32_t> annotation_to_id;
  for(uint32_t i = 0; i < annotations.size(); i++)
    annotation_to_id[annotations[i]] = i;
  
  std::vector<std::vector<Var>> props;
  props.resize(annotations.size());
  
  for(uint32_t v = 0; v < occurring_prop.size(); v++)
    if(occurring_prop[v])
      props[annotation_to_id[prop_to_annotation[v]]].push_back(v);
  
  std::vector<std::vector<Var>> origs;
  origs.resize(annotations.size());
  
  for(uint32_t i = 0; i < props.size(); i++)
  {
    const std::vector<Var>& ps = props[i];
    std::vector<Var>& os = origs[i];
    for(const Var v : ps)
      os.push_back(prop_to_original[v]);
  }
  
  for(uint32_t i = 0; i < props.size(); i++)
  {
    const std::vector<Var>& ps = props[i];
  
    if(ps.size() == 0) continue;
    
    const std::vector<Var>& os = origs[i];
    const std::vector<Lit>& as = *annotations[i];
    
    fprintf(file, "x ");
    
    for(const Var v : ps)
      fprintf(file, "%d ", prop_to_ferp[v]);
    fprintf(file, "0 ");
    for(const Var v : os)
      fprintf(file, "%d ", v);
    fprintf(file, "0 ");
    for(const Lit l : as)
      fprintf(file, "%d ", l);
    fprintf(file, "0\n");
  }
}