#include <zlib.h>

#include "Reader.h"

int main(int argc, const char* argv[])
{
  if(argc != 4)
  {
    printf("usage: %s <CNF> <TRACE> <FERP>\n", argv[0]);
    return -1;
  }
  
  const char* cnf_name = argv[1];
  const char* trace_name = argv[2];
  const char* ferp_name = argv[3];
  
  gzFile cnf_file = gzopen(cnf_name, "rb");
  
  if (cnf_file == Z_NULL)
  {
    printf("Could not open file: %s", cnf_name);
    return -2;
  }
  
  gzFile trace_file = gzopen(trace_name, "rb");
  
  if (trace_file == Z_NULL)
  {
    printf("Could not open file: %s", trace_name);
    return -3;
  }
  
  FILE* ferp_file = fopen(ferp_name, "wb");
  
  if(ferp_file == nullptr)
  {
    printf("Could not open file: %s", ferp_name);
    return -4;
  }
  
  VarManager vmngr;
  
  AnnotationReader cnf_reader(cnf_file);
  
  int res = cnf_reader.readCNF(vmngr);
  gzclose(cnf_file);
  if (res != 0)
  {
    printf("Something went wrong while reading CNF, code %d", res);
    return res;
  }
  
  TraceReader trace_reader(trace_file);
  
  res = trace_reader.readTrace(vmngr);
  gzclose(trace_file);
  if (res != 0)
  {
    printf("Something went wrong while reading CNF, code %d", res);
    return res;
  }
  
  vmngr.computeNames();
  vmngr.writeExpansions(ferp_file);
  
  trace_reader.writeTrace(vmngr, ferp_file);
  fclose(ferp_file);
  
  return 0;
}