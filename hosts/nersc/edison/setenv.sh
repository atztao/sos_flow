# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "${BASH_SOURCE}")"; pwd)"
BASEDIR=`dirname \`dirname ${SCRIPTPATH}\``
echo "SOS Base directory: ${BASEDIR}"

module load cmake
module swap PrgEnv-pgi PrgEnv-gnu
module load dataspaces

export SOS_HOST_KNOWN_AS="\"NERSC (Edison)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""
export SOS_CMD_PORT=22500
export SOS_ROOT=${BASEDIR}
export SOS_WORK=.
export ACCOUNT=csc103
export BUILDDIR=${MEMBERWORK}/${ACCOUNT}/sos_flow/build-titan
export CC=gcc
export CXX=g++
export MPICC=cc
export MPICXX=CC
export TAU_ARCH=craycnl
export TAU_CONFIG=-mpi-pthread
export TAU_ROOT=${HOME}/src/tau2
export ADIOS_ROOT=${HOME}/src/chaos/adios/1.11-gcc
export PATH=${ADIOS_ROOT}/bin:${PATH}

export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_gnu_49_mt.so -DADIOS_ROOT=${ADIOS_ROOT} -DENABLE_ADIOS_EXAMPLES=TRUE -DFIX_ADIOS_DEPENDENCIES=TRUE"

export PKG_CONFIG_PATH=${HOME}/src/chaos/titan-gcc/lib/pkgconfig:${PKG_CONFIG_PATH}

export SOS_ENV_SET=1

#echo "Reconfiguring the build scripts..."
#cd $SOS_ROOT
#$SOS_ROOT/scripts/configure.sh -c
#cd $SOS_BUILD_DIR
#make clean
#echo "-- Compile SOSflow with the following command:"
#echo ""
#echo "        make -j install"
#echo ""

