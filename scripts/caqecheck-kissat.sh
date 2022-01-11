#! /bin/bash


trap 'cleanup; exit 1' SIGHUP SIGINT SIGTERM

readonly VERSION="1.0"

readonly BASEDIR="./bin/"

readonly QBFSOLVER_SAT=10
readonly QBFSOLVER_UNSAT=20
readonly SATSOLVER_SAT=10
readonly SATSOLVER_UNSAT=20

readonly TMPDIR="/tmp"


## tools

readonly TOOLBASEDIR="$BASEDIR"

readonly QBFSOLVER="$TOOLBASEDIR/caqe"
QBFSOLVER_PARAMS=""

readonly CERTCHECK="$TOOLBASEDIR/caqe-2/certcheck"
CERTCHECK_PARAMS=""

readonly AIGTOCNF="$TOOLBASEDIR/aigtocnf"
AIGTOCNF_PARAMS=""

readonly SATSOLVER="$TOOLBASEDIR/ferp-models/picosat-965/picosat"
SATSOLVER_PARAMS=""

readonly SATSOLVER_KISSAT="$TOOLBASEDIR/kissat"
SATSOLVER_PARAMS=""

## files

readonly certificate="$TMPDIR/caqe$$.aag"
readonly merged="$TMPDIR/caqe-merged$$.aag"
readonly cnf="$TMPDIR/caqe$$.cnf"

## helpers

function cleanup
{
  rm -f "$merged"
  rm -f "$cnf"
  rm -f "$certificate"
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


echo $QBFSOLVER -c $formula 


/usr/bin/time -q -f "solving-time: %e" $QBFSOLVER -c $formula > $certificate
solver_result=$?

echo "solving done with exit code $solver_result"

cert_size=`ls -la $certificate | cut -d" " -f5`

echo "certificiate-size: $cert_size"

cert_header=`grep aag $certificate`

echo "certificate-header:  $cert_header"

if [[ $solver_result == $QBFSOLVER_SAT ]]; then
  die "QBF is true; "
elif [[ $solver_result != $QBFSOLVER_SAT && 
        $solver_result != $QBFSOLVER_UNSAT ]]; then
  die "QBF solver ERROR"
fi
echo ""
echo "QBF solver OK"
echo ""

# Merge AIGER and QDIMACS files into a formula checkable by a SAT solver


echo "merging  AIG into QBF ..."
echo ""
/usr/bin/time -q -f "merging-time %e" $CERTCHECK $formula $certificate > $merged
merge_result=$?

if [[ $merge_result != 0 ]]; then
  die "CERTCHECK ERROR"
fi
echo "CERTCHECK OK"
echo ""

mcnf_header=`grep aag $merged`
echo "merged-cnf-header: $mcnf_header"

mcnf_size=`ls -la $merged | cut -f5 -d" "`
echo "merged-cnf-size: $mcnf_size"

# transform aig to cnf

echo "transforming aig to cnf"

/usr/bin/time -q -f "aigtocnf: %e" $AIGTOCNF $merged > $cnf

cnf_header=`head -n1 $cnf`
cnf_size=`ls -la $cnf | cut -f5 -d" "`

echo "cnf-header: $cnf_header"
echo "cnf-size: $cnf_size"



# SAT solver: validate certificate
echo "running SAT solver..."
echo ""

/usr/bin/time -q -f "final-check-time: %e" $SATSOLVER_KISSAT $cnf 
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
