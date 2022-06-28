include(${CMAKE_CURRENT_LIST_DIR}/../../src/coreComponents/LvArray/host-configs/ORNL/crusher-cce@13.0.1.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/crusher-base.cmake)

set( CONDUIT_DIR "${GEOSX_TPL_DIR}/conduit-0.7.2-yjtizbev4kv2xwnezjpp77zrb5r4ujje/" CACHE PATH "" )
set( HDF5_DIR "${GEOSX_TPL_DIR}/hdf5-1.10.8-yddpmglvv6c4mam3swocgkeeomcfpoyw/" CACHE PATH "" )
set( SILO_DIR "${GEOSX_TPL_DIR}/silo-4.10.2-alawrmdlhl5srmhgbnoeqzj6ofhqsw65/" CACHE PATH "" )

set( BLAS_DIR "/opt/rocm-4.5.2/" CACHE PATH "" )

set( PUGIXML_DIR "${GEOSX_TPL_DIR}/pugixml-1.11.4-rjfutqv6lun34dfw655kvqal6rzyy55v/" CACHE PATH "" )
set( FMT_DIR "${GEOSX_TPL_DIR2}/fmt-8.0.1-btbom4skpofijcymgd5sgwbzw37l4isj" CACHE PATH "" )
set( SUITESPARSE_DIR "${GEOSX_TPL_DIR}/suite-sparse-5.10.1-jgaa5fs7xmdjfslwbyw5o3oeigw3sjvi/" CACHE PATH "" )

# HYPRE options
set( ENABLE_HYPRE_ROCM OFF CACHE BOOL "" )

if( ENABLE_HYPRE_ROCM )
  set( HYPRE_DIR "${GEOSX_TPL_DIR}/hypre-develop-4yr7cd2d2sfu6udyuy72j647u2n6h5kq" CACHE PATH "" )
  set( SUPERLU_DIST_DIR "${GEOSX_TPL_DIR}/superlu-dist-7.1.1-iu6gvmifimd6yjyl4rnxsbpdvkkdy67u/" CACHE PATH "" )
else()
  set( HYPRE_DIR "${GEOSX_TPL_DIR}/hypre-develop-7n62z7h5hd7emdq4dptavi6fcllsliwt" CACHE PATH "" )
  set( SUPERLU_DIST_DIR "${GEOSX_TPL_DIR}/superlu-dist-7.1.1-ryxxa3bxgu4fyk5xhqxcenskd6v3ziaw/" CACHE PATH "" )
endif()
