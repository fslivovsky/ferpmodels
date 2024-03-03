#!/usr/bin/env python2

import sys, os, shutil, errno, subprocess, signal

home = os.path.dirname(os.path.abspath(__file__)) + "/"
dependencies = ["ijtihad/ijtihad", "picosat-965/picosat", 
                "booleforce-1.2/tracecheck", "toferp/toferp", 
                "ferpcert/ferpcheck", "ferpcert2/ferpcert",
                "certcheck-1.0.1/certcheck", "cadical/build/cadical"]

dependencies = [home + x for x in dependencies]
tmp_dir = home + "tmp/tmp-%d/" % os.getpid()

trim = False

def assure_dir(path):
  try:
    if not os.path.exists(path):
      os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise e


def parse_args():
  sys.stdout.write("Parsing command line arguments ... ")
  assert len(sys.argv) == 3
  input_path = os.path.abspath(sys.argv[1])
  output_path = os.path.abspath(sys.argv[2])
  qbf_name = ".".join(input_path.split("/")[-1].split(".")[:-1])
  output_dir = "/".join(output_path.split("/")[:-1])
  if not os.path.exists(input_path):
    raise OSError("Input file %s does not exist" % input_path)
  assure_dir(output_dir)
  print("DONE")
  return input_path, output_path, qbf_name


def check_dependencies():
  padlen = len(max(dependencies, key=len))
  missing = 0
  for dep in dependencies:
    sys.stdout.write("Checking dependency: %s " % dep)
    sys.stdout.write((" " * (padlen - len(dep))) + "... ")
    if os.path.exists(dep) :
      print("OK")
    else:
      print("MISSING")
      missing += 1
  if missing > 0:
    raise OSError("One or more dependencies are missing")


def clean(ret):
  # shutil.rmtree(tmp_dir, ignore_errors=True)
  sys.exit(ret)


def term_handler(signum, frame):
  print(signum, "signal handler called. Clean exit.")
  clean(-1)


def main():
  signal.signal(signal.SIGTERM, term_handler)
  signal.signal(signal.SIGINT, term_handler)

  print("Home is at", home)
  print("Tmp is at", tmp_dir)

  input_path, output_path, qbf_name = parse_args()
  check_dependencies()
  assure_dir(tmp_dir)
  FNULL = open(os.devnull, 'w')

  # Call the qbf solver and produce the .cnf file

  sys.stdout.write("Calling QBF solver ... ")
  sys.stdout.flush()
  ret = subprocess.call([dependencies[0], "--wit_per_call=-1", "--cex_per_call=-1",
                         "--tmp_dir="+tmp_dir, "--log_phi="+tmp_dir+"tmp.cnf", input_path]) #, stdout=FNULL, stderr=FNULL)
  if ret == 10:
    print("DONE")
    print("The given formula is TRUE.")
    clean(1)
  elif ret != 20:
    print("FAILED")
    print("There has been an error with code %d" % ret)
    clean(2)
  else:
    print("DONE")

  assert(os.path.exists(tmp_dir + "tmp.cnf"))

  # Call the sat solver again on the .cnf file
  # This has to be done because no proof logging can be done in incremental mode

  sys.stdout.write("Calling SAT solver ... ")
  sys.stdout.flush()
  ret = subprocess.call([dependencies[1], "-T", tmp_dir+"tmp.proof", tmp_dir+"tmp.cnf"] ) #,
                        # stdout=FNULL, stderr=FNULL)
  if ret == 10:
    print("FAILED")
    print("The expanded formula is SAT")
    clean(3)
  elif ret != 20:
    print("FAILED")
    print("There has been an error with code %d" % ret)
    clean(4)
  else:
    print("DONE")

  assert (os.path.exists(tmp_dir + "tmp.proof"))

  # The SAT solver only produces normal extended tracecheck proofs
  # Check the proof with tracecheck and extract binary resolution proof

  sys.stdout.write("Checking unsat proof ... ")
  sys.stdout.flush()
  ret = subprocess.check_output([dependencies[2], "-B", tmp_dir + "tmp.proof2", "-c",
                                 tmp_dir + "tmp.cnf", tmp_dir + "tmp.proof"])
  ret = ret.strip()
  if ret != b"resolved 1 root and 1 empty clause":
    print("FAILED")
    print(ret)
    clean(5)
  else:
    if trim: os.remove(tmp_dir + "tmp.proof")
    print("DONE")
  
  assert (os.path.exists(tmp_dir + "tmp.proof2"))
  
  # Merge the comment information from the cnf and the binary resolution
  # proof into a FERP trace.

  sys.stdout.write("Producing FERP trace ... ")
  sys.stdout.flush()
  ret = subprocess.call([dependencies[3], tmp_dir + "tmp.cnf", tmp_dir + "tmp.proof2", tmp_dir + "tmp.ferp"])

  if ret != 0:
    print("FAILED", ret)
    clean(6)
  else:
    if trim: os.remove(tmp_dir + "tmp.cnf")
    if trim: os.remove(tmp_dir + "tmp.proof2")
    print("DONE")

  # Check whether the FERP trace is consistent

  sys.stdout.write("Checking FERP trace ... ")
  sys.stdout.flush()
  ret = subprocess.call([dependencies[4], input_path, tmp_dir + "tmp.ferp"])

  if ret != 0:
    print("FAILED", ret)
    clean(7)
  else:
    print("DONE")

  # Extract a circuit for the universals into an AIGER file

  sys.stdout.write("Extracting strategy ... ")
  sys.stdout.flush()
  ret = subprocess.call([dependencies[5], input_path, tmp_dir + "tmp.ferp", output_path])

  if ret != 0:
    print("FAILED", ret)
    clean(8)
  else:
    if trim: os.remove(tmp_dir + "tmp.ferp")
    print("DONE")

  # Merge AIGER and QDIMACS files into a formula checkable by a SAT solver

  sys.stdout.write("Producing CNF ... ")
  sys.stdout.flush()

  FCNF = open(tmp_dir + "tmp.cnf2", "wb")
  ret = subprocess.call([dependencies[6], input_path, output_path], stdout=FCNF)
  FCNF.close()
  
  if ret != 0:
    print("FAILED", ret)
    clean(9)
  else:
    print("DONE")

  sys.stdout.write("Check validity of certificate ... ")
  sys.stdout.flush()
  ret = subprocess.call([dependencies[7], tmp_dir + "tmp.cnf2"],
                        stdout=FNULL, stderr=FNULL)
  if ret == 10:
    print("FAILED")
    print("The merged formula is SAT")
    clean(10)
  elif ret != 20:
    print("FAILED")
    print("There has been an error with code %d" % ret)
    clean(11)
  else:
    print("SUCCESS")
    subprocess.call(["gzip", output_path])
    clean(0)


if __name__ == '__main__':
  main()
