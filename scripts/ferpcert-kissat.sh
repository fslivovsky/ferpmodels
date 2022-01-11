#! /bin/bash


trap 'cleanup; exit 1' SIGHUP SIGINT SIGTERM

timeout=$2
space=$3


readonly VERSION="1.0"

readonly BASEDIR="./ferp-models"

readonly QBFSOLVER_SAT=10
readonly QBFSOLVER_UNSAT=20
readonly SATSOLVER_SAT=10
readonly SATSOLVER_UNSAT=20

readonly TMPDIR="/tmp"


## tools

readonly TOOLBASEDIR="$BASEDIR"

readonly QBFSOLVER="$TOOLBASEDIR/ijtihad/ijtihad"
QBFSOLVER_PARAMS=""

readonly TOFERP="$TOOLBASEDIR/toferp/toferp"
QRPCHECK_PARAMS=""

readonly FERPCHECK="$TOOLBASEDIR/ferpcert/ferpcheck"
QRPCHECK_PARAMS=""

readonly FERPCERT="$TOOLBASEDIR/ferpcert/ferpcert"
QRPCERT_PARAMS=""

readonly CERTCHECK="$TOOLBASEDIR/certcheck-1.0.1/certcheck"
CERTCHECK_PARAMS=""

readonly TRACECHECK="$TOOLBASEDIR/booleforce-1.2/tracecheck"
CERTCHECK_PARAMS=""

readonly SATSOLVER="$TOOLBASEDIR/picosat-965/picosat"
SATSOLVER_PARAMS=""

readonly SATSOLVER_KISSAT="$TOOLBASEDIR/kissat/kissat"
SATSOLVER_PARAMS=""

## files

readonly expanded="$TMPDIR/ferpcert$$expanded.cnf"
readonly ferp="$TMPDIR/ferp_proof$$.ferp"
readonly proof="$TMPDIR/ferpcert_proof$$.qrp"
readonly proof2="$TMPDIR/ferpcert_proof2$$.qrp"
readonly certificate="$TMPDIR/ferpcert_certificate$$.aig"
readonly merged_cnf="$TMPDIR/ferpcert_merged_cnf$$.cnf"

RUNLIM="/home/biere/bin/runlim \
  --single \
  --time-limit="$timeout" \
  --real-time-limit="$timeout" \
  --space-limit="$space" \
"

## helpers

function cleanup
{
  rm -f "$expanded"
  rm -f "$ferp"
  rm -f "$proof"
  rm -f "$proof2"
  rm -f "$certificate"
  rm -f "$merged_cnf"
}

function clean_exit
{
  if [[ $1 != 0 ]]; then
    echo "clean up and exit"
  fi
  cleanup
  exit $1
}

function die
{
  echo "$1"
  clean_exit 1
}

function check_binary
{
  if [[ ! -f "$1" ]]; then
    die "no binary found at $1"
  fi
}


trap 'clean_exit 1' SIGHUP SIGINT SIGTERM


## input formula

formula=$1


## check tmp directory

if [[ ! -d "$TMPDIR" ]]; then
  if [[ -f "$TMPDIR" ]]; then
    die "invalid tmp dir: '$TMPDIR' (is a file)"
  fi
  mkdir -p "$TMPDIR" 2> /dev/null
  if [[ $? != 0 ]]; then
    die "could not create tmp directory: '$TMPDIR'"
  fi
fi

echo "using tmp directory:"
echo "  $(readlink -f "$TMPDIR")"

# QBF solver
echo "running QBF solver..."
echo ""


echo $QBFSOLVER --dependencies=0 --wit_per_call=-1 --cex_per_call=-1 --log_phi=$expanded $formula


/usr/bin/time -q -f "solving-time: %e" $QBFSOLVER --wit_per_call=-1 --cex_per_call=-1 --log_phi=$expanded $formula
solver_result=$?

echo "solving done with exit code $solver_result"

if [[ $solver_result == $QBFSOLVER_SAT ]]; then
  die "QBF is true; currently not suppored"
elif [[ $solver_result != $QBFSOLVER_SAT && 
        $solver_result != $QBFSOLVER_UNSAT ]]; then
  die "QBF solver ERROR"
fi
echo ""
echo "QBF solver OK"
echo ""

 # Call the sat solver again on the .cnf file
 # This has to be done because no proof logging can be done in incremental mode
echo "running SAT Solver..."
echo ""

echo $expanded
/usr/bin/time -q -f "proof-gen-satsolver: %e" $SATSOLVER "-T" $proof $expanded 
solver_result=$?


trace_size=`ls -la $proof | cut -f5 -d" "`

if [[ $solver_result == $QBFSOLVER_SAT ]]; then
  die "FAILED - expanded formula is SAT"
fi 

if [[ $solver_result != $QBFSOLVER_UNSAT ]]; then
  die "ERROR in proof generation"
fi 


echo "" 
echo "PROOF GENERATION OK"
echo ""

# The SAT solver only produces normal extended tracecheck proofs
# Check the proof with tracecheck and extract binary resolution proof


echo "checking unsat proof"
echo ""

/usr/bin/time -q -f "tracecheck-time: %e" $TRACECHECK -B $proof2 -c $expanded $proof
tc_result=$?

trace_size=`ls -la $proof2 | cut -f5 -d" "`
echo "trace-size2: $trace_size"

if [[ $tc_result != 0 ]]; then
  die "TRACE CHECK ERROR"
fi
echo ""
echo "TRACE CHECK OK"
echo ""

# Merge the comment information from the cnf and the binary resolution
# proof into a FERP trace.


echo "running toferp..."
echo ""
/usr/bin/time -q -f "toferp-time: %e" $TOFERP $expanded $proof2 $ferp 
toferp_result=$?

ferp_size=`ls -la $ferp| cut -d" " -f5`
echo "ferp-size: $ferp_size" 

if [[ $toferp_result != 0 ]]; then
  die "TOFERP ERROR"
fi
echo "TOFERP OK"
echo ""

# Check whether the FERP trace is consistent


echo "checking FERP PROOF..."
echo ""


/usr/bin/time -q -f "ferpcheck-time: %e" $FERPCHECK $formula $ferp 
ferpcheck_result=$?

if [[ $ferpcheck_result != 0 ]]; then
  die "FERP CHECK ERROR"
fi
echo "FERP CHECK OK"
echo ""

# Extract a circuit for the universals into an AIGER file

echo "extracting AIG ..."
echo ""

/usr/bin/time -q -f "ferpcert-time: %e" $FERPCERT $formula $ferp $certificate
ferpcert_result=$?

aig_size=`ls -la $certificate | cut -f5 -d" "`
echo "aig-size: $aig_size"
aig_header=`grep aag $certificate`

echo "aig-header: $aig_header"

if [[ $ferpcert_result != 0 ]]; then
  die "FERP CERT ERROR"
fi
echo "FERP CERT OK"
echo ""

# Merge AIGER and QDIMACS files into a formula checkable by a SAT solver


echo "merging  AIG into QBF ..."
echo ""
/usr/bin/time -q -f "merging-time %e" $CERTCHECK $formula $certificate > $merged_cnf
merge_result=$?

if [[ $merge_result != 0 ]]; then
  die "CERTCHECK ERROR"
fi
echo "CERTCHECK OK"
echo ""

mcnf_header=`head -n1 $merged_cnf`
echo "merged-cnf-header: $mcnf_header"

mcnf_size=`ls -la $merged_cnf | cut -f5 -d" "`
echo "merged-cnf-size: $mcnf_size"



# SAT solver: validate certificate
echo "running SAT solver..."
echo ""

/usr/bin/time -q -f "final-check-time: %e" $SATSOLVER_KISSAT $merged_cnf 
sat_solver_result=$?

if [[ $sat_solver_result == $SATSOLVER_SAT ]]; then
  die "INVALID CHECKING RESULT"
elif [[ $sat_solver_result != $SATSOLVER_UNSAT ]]; then
  die "SAT solver ERROR"
fi
echo ""
echo "SAT solver OK"

echo ""
echo "VALID"
clean_exit 0
