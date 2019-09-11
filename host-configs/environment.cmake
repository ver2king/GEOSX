site_name(HOST_NAME)

set(CMAKE_C_COMPILER "$ENV{CC}" CACHE PATH "" FORCE)
set(CMAKE_CXX_COMPILER "$ENV{CXX}" CACHE PATH "" FORCE)
set(ENABLE_FORTRAN OFF CACHE BOOL "" FORCE)

set(ENABLE_MPI ON CACHE PATH "" FORCE)
set(MPI_C_COMPILER "$ENV{MPICC}" CACHE PATH "" FORCE)
set(MPI_CXX_COMPILER "$ENV{MPICXX}" CACHE PATH "" FORCE)
set(MPIEXEC_EXECUTABLE "$ENV{MPIEXEC}" CACHE PATH "" FORCE)

set( ENABLE_GTEST_DEATH_TESTS ON CACHE BOOL "" FORCE )
