#include "PetscSolver.hpp"
#include <math.h>

// mimic testLAOperations.cpp

// to compile: make all (check makefile)
// to run: mpiexec -n 2 ./testLAOperations

void testPetscVector(int rank)
{
  // TESTING PetscVector

  // make a PetscVector
  PetscVector vec1;

  // test create from array
  if (rank == 0) printf("create a vector:\n");
  double values1[5] = {2, 2, 3, 1, 4};
  vec1.create(5, values1);

  // test create from vector
  std::vector<double> vec (4, 100);
  printf("size: %lu\n", vec.size());

  PetscVector vec4;
  vec4.create(vec);
  
  // test print
  vec1.print();
  vec4.print();

  // test copy constructor
  if (rank == 0) printf("copy a vector:\n");
  PetscVector vec2;
  vec2 = PetscVector(vec1);
  vec2.print();

  // test set
  if (rank == 0) printf("set value in a vector:\n");
  vec2.set(1, 10);
  vec2.print();

  // test add
  if (rank == 0) printf("add to value in a vector:\n");
  vec2.add(1, 2);
  vec2.print();

  // test scale
  if (rank == 0) printf("scale a vector:\n");
  vec1.scale(.5);
  vec1.print();

  // test dot
  PetscVector vec3;
  double values2[5] = {1, 2, 3, 1, 2};
  vec3.create(5, values2);

  // test copy
  if (rank == 0) printf("copy a vector:\n");
  vec2.copy(vec3);
  vec2.print();

  // test dot
  double dotproduct;
  vec1.dot(vec3, &dotproduct);
  if(rank == 0) printf("dot product is: %f\n", dotproduct);

  // test axpby
  if (rank == 0) printf("axpy a vector:\n");
  vec1.print();
  vec1.axpy(2, vec2);
  vec1.print();
  
  // test axpby
  if (rank == 0) printf("axpby a vector:\n");
  vec1.axpby(2, vec3, 3);
  vec1.print();

  // test getVec
  if (rank == 0) printf("get a vector:\n");
  VecView(vec1.getVec(), PETSC_VIEWER_STDOUT_WORLD);

  // test getPointer
  if (rank == 0) printf("get a pointer:\n");
  printf("%p\n", vec1.getPointer());

  // test norms
  double norm1, norm2, normInf;
  vec3.norm1(norm1);
  vec3.norm2(norm2);
  vec3.normInf(normInf);

  if(rank == 0){
    printf("1-norm is: %f\n", norm1);
    printf("2-norm is: %f\n", norm2);
    printf("infinity-norm is: %f\n", normInf);
  }
  
  // test globalSize, localsize
  if(rank == 0) printf("global size is: %d\n", vec1.globalSize());
  printf("local size of vec1 is: %d\n", vec1.localSize());

  // test getElement
  vec3.print();
  if(rank == 0) printf("Element 0 is: %f\n", vec3.getElement(0));
  if(rank == 0) printf("Element 1 is: %f\n", vec3.getElement(1));
  if(rank == 0) printf("Element 2 is: %f\n", vec3.getElement(2));
  if(rank == 1) printf("Element 3 is: %f\n", vec3.getElement(3));
  if(rank == 1) printf("Element 4 is: %f\n", vec3.getElement(4));

	// test zero
	if (rank == 0) printf("Before zero:\n");
	vec3.print();
	vec3.zero();
	if (rank == 0) printf("After zero:\n");
	vec3.print();

	// test rand
	if (rank == 0) printf("After rand:\n");	
	vec3.rand();
	vec3.print();

	// test create with local/global size
	PetscVector vec5;
	vec5.createWithLocalSize(2, PETSC_COMM_WORLD);
	if(rank == 0) printf("Local size of vector (should be 2): %d\n", vec5.localSize());
	if(rank == 0) printf("Global size of vector (should be 4): %d\n", vec5.globalSize());	

	PetscVector vec6;
	vec6.createWithGlobalSize(6, PETSC_COMM_WORLD);
	if(rank == 0) printf("Local size of vector (should be 3): %d\n", vec6.localSize());
	if(rank == 0) printf("Global size of vector (should be 6): %d\n", vec6.globalSize());	

	// test set/add with arrays
	vec5.zero();
	int indices5[3] = {0, 1, 3};
	double values5[3] = {1, 2, 3};
	vec5.set(indices5, values5, 3);
	vec5.print();

	int indices6[2] = {1, 2};
	double values6[2] = {1, 1};
	vec5.add(indices6, values6, 2);	
	vec5.print();

	// test set with value
	if (rank == 0) printf("After set(5):\n");	
	vec3.set(5);
	vec3.print();

	// test get (same as getElement)
	if(rank == 0) printf("Element 0 is: %f\n", vec3.get(0));
  if(rank == 0) printf("Element 1 is: %f\n", vec3.get(1));
  if(rank == 0) printf("Element 2 is: %f\n", vec3.get(2));

	// test open and close
	vec3.open();
	vec3.close();

	// test write
	vec3.write("test.mat"); // vec3.dat?

	return;
}

void testPetscSparseMatrix(int rank)
{
  // TESTING PetscSparseMatrix
  printf("\n\n\n");

  // make vector
  PetscVector vec3;
  double values2[5] = {1, 2, 3, 1, 2};
  vec3.create(5, values2);

  // test create
  if (rank == 0) printf("create a square matrix:\n");
  PetscSparseMatrix mat1;
  // 5 rows with 3 nonzeros per row
  mat1.create(PETSC_COMM_WORLD, 5, 3);
  mat1.print();

  // test add
  if (rank == 0) printf("add value:\n");
  mat1.add(3, 3, 1);
  mat1.print();

  // test set
  if (rank == 0) printf("set value:\n");
  mat1.add(4, 3, -1);
  mat1.print();

  // test constructor
  if (rank == 0) printf("copy a matrix:\n");
  PetscSparseMatrix mat2(mat1);
  mat2.print();

  // test create
  if (rank == 0) printf("create a rectangular matrix:\n");
  PetscSparseMatrix mat3;
  // 4 rows, 3 columns with 3 nonzeros per row
  mat3.create(PETSC_COMM_WORLD, 5, 3, 3);
  mat3.print();

  // test create
  if (rank == 0) printf("copy a matrix:\n");
  PetscSparseMatrix mat4;
  mat4.create(mat3);
  mat4.print();

  // test zero
  mat4.set(0, 1, .5);
  mat4.set(2, 2, 3);
  mat4.print();
  mat4.zero();
  if (rank == 0) printf("zero a matrix:\n");
  mat4.print();

  // test set
  if (rank == 0) printf("set values to a matrix:\n");
  if (rank == 0) printf("before:\n");
  mat1.print();

  int row = 2;
  const int ncols = 2;
  int cols[ncols] = {0, 2};
  double values[ncols] = {3, -1};

  mat1.set(row, ncols, values, cols);
  if (rank == 0) printf("after:\n");
  mat1.print();

  // test add
  if (rank == 0) printf("add values to a matrix:\n");
  if (rank == 0) printf("before:\n");
  mat1.print();

  int row2 = 3;
  const int ncols2 = 3;
  int cols2[3] = {0, 3, 4};
  double values3[3] = {1, .5, -.1};

  mat1.add(row2, ncols2, values3, cols2);
  if (rank == 0) printf("after:\n");
  mat1.print();

  // test multiply
  if (rank == 0) printf("multiply a matrix and vector:\n");
  PetscVector vec4(vec3);
  mat1.multiply(vec3, vec4);
  vec4.print();

  // make new vectors and stuff
  PetscVector vec5;
  double values5[3] = {1, 0, 2};
  vec5.create(3, values5);
  vec5.print();

  PetscVector vec6;
  double values6[3] = {2, 4.5, 2};
  vec6.create(3, values6);
  vec6.print();

  PetscVector vec7;
  vec7 = PetscVector(vec6);
  vec7.print();

  PetscSparseMatrix mat5;
  mat5.create(PETSC_COMM_WORLD, 3, 3, 3);
  int cols_[3] = {0, 1, 2};
  double row1[3] = {2, 1, 0};
  double row2_[3] = {.5, -1, 2};
  double row3[3] = {3, 2, 1};
  mat5.set(0, 3, row1, cols_);
  mat5.set(1, 3, row2_, cols_);
  mat5.set(2, 3, row3, cols_);
  mat5.print();

  // test residual
  if (rank == 0) printf("compute residual:\n");
  mat5.residual(vec5, vec6, vec7);
  vec7.print();

  // test scale
  if (rank == 0) printf("scale a matrix:\n");
  mat2.scale(.5);
  mat2.print();

  // test leftScale
  if (rank == 0) printf("left scale a matrix:\n");
  PetscSparseMatrix mat6(mat5);
  mat6.leftScale(vec5);
  mat6.print();

  // test rightScale
  if (rank == 0) printf("right scale a matrix:\n");
  PetscSparseMatrix mat7(mat5);
  mat7.rightScale(vec5);
  mat7.print();

  // test leftRightScale
  if (rank == 0) printf("left and right scale a matrix:\n");
  PetscSparseMatrix mat8(mat5);
  mat8.leftRightScale(vec5, vec7);
  mat8.print();

  // test gemv
  if (rank == 0) printf("compute gemv:\n");
  mat6.gemv(.5, vec5, 2, vec7, false);
  vec7.print();

  // test clearRow
  if (rank == 0) printf("clear a matrix row:\n");
  mat1.clearRow(3, 2);
  mat1.print();

  // test globalRows, globalCols
  if(rank == 0){
    printf("global rows: %d\n", mat4.globalRows());
    printf("global columns: %d\n", mat4.globalCols());
  }

  // test ilower and iupper
  mat4.print();
  printf("rank %d first row: %d\n", rank, mat4.ilower());
  printf("rank %d last row: %d\n", rank, mat4.iupper());

  // test myRows and myCols
  printf("rank %d number of rows: %d\n", rank, mat4.myRows());
  printf("rank %d number of columns: %d\n", rank, mat4.myCols());

  // test rowMapGID
  printf("rank %d LID of GID 2: %d\n", rank, mat4.rowMapLID(2));
  printf("rank %d LID of GID 4: %d\n", rank, mat4.rowMapLID(4));


  // test norms
  double matnorm1, matnormInf, matnormFrob;
  matnorm1 = mat1.norm1();
  matnormInf = mat1.normInf();
  matnormFrob = mat1.normFrobenius();

  mat1.print();
  if(rank == 0){
    printf("1-norm of mat1 is: %f\n", matnorm1);
    printf("infinity-norm of mat1 is: %f\n", matnormInf);
    printf("Frobenius of mat1 is: %f\n", matnormFrob);
  }

  // test getPointer
  if (rank == 0) printf("get a pointer:\n");
  printf("%p\n", mat1.getPointer());

  // test getMat
  MatView(mat1.getMat(), PETSC_VIEWER_STDOUT_WORLD);

  // test getRow
  int row_ = 2;
  int numEntries_;
  int indices_[2];
  double values_[2];
  if(mat1.ilower() <= row_ && row_ <= mat1.iupper())
  {
    mat1.getRow(row_, numEntries_, values_, indices_);
    printf("rank %d: number of entires in row %d: %d\n", rank, row_, numEntries_);
    printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, indices_[0], values_[0]);
    printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, indices_[1], values_[1]);
  }

  // test getLocalRow
  row_ = 1;
  double values__[2];
  int indices__[2];
  mat1.print();
  mat1.getLocalRow(row_, numEntries_, values__, indices__);
  printf("rank %d number of entires in row %d: %d\n", rank, row_, numEntries_);
  printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, indices__[0], values__[0]);
  if (rank == 0) {
    row_ = 2;
    mat1.getLocalRow(row_, numEntries_, values__, indices__);
    printf("rank %d number of entires in row %d: %d\n", rank, row_, numEntries_);
    printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, indices__[0], values__[0]);
  }

  // test getRow vector
  row_ = 2;
  int numEntries__;
  std::vector<int> vecindices_(2);
  std::vector<double> vecvalues_(2);
  if(mat1.ilower() <= row_ && row_ <= mat1.iupper())
  {
    mat1.getRow(row_, numEntries__, vecvalues_, vecindices_);
    printf("rank %d: number of entires in row %d: %d\n", rank, row_, numEntries__);
    printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, vecindices_[0], vecvalues_[0]);
    printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, vecindices_[1], vecvalues_[1]);
  }
  
  // test getLocalRow vector
  row_ = 1;
  std::vector<int> vecindices__(2);
  std::vector<double> vecvalues__(2);
  mat1.print();
  mat1.getLocalRow(row_, numEntries__, vecvalues__, vecindices__);
  printf("rank %d number of entires in row %d: %d\n", rank, row_, numEntries__);
  printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, vecindices__[0], vecvalues__[0]);
  if (rank == 0) {
    row_ = 2;
    mat1.getLocalRow(row_, numEntries__, vecvalues__, vecindices__);
    printf("rank %d number of entires in row %d: %d\n", rank, row_, numEntries__);
    printf("rank %d: mat1[%d][%d] = %f\n", rank, row_, vecindices__[0], vecvalues__[0]);
  }

	// test clearRow
	mat1.print();
	mat1.clearRow(2, 3.5); // everyone calls clearRow()
	if(rank ==0) 
	{
		// check clearRow
		int numValRown;
		std::vector<double> vecValuesRown;
		std::vector<int> vecIndicesRown;
		mat1.getRow(2, numValRown, vecValuesRown, vecIndicesRown);
		printf("This should be -3.5: %f\n", vecValuesRown[0]);
	}
	mat1.print();

	// test new create functions
	if(rank == 0) printf("new create functions:\n");
	PetscSparseMatrix mat9;
	mat9.createWithLocalSize(3, 3, PETSC_COMM_WORLD);
	if(rank == 0){
    printf("global rows: %d\n", mat9.globalRows());
    printf("global columns: %d\n", mat9.globalCols());
  }

	mat9.createWithLocalSize(2, 4, 3, PETSC_COMM_WORLD);
	if(rank == 0){
    printf("global rows: %d\n", mat9.globalRows());
    printf("global columns: %d\n", mat9.globalCols());
  }

	mat9.createWithGlobalSize(5, 5, PETSC_COMM_WORLD);
  if(rank == 0){
    printf("global rows: %d\n", mat9.globalRows());
    printf("global columns: %d\n", mat9.globalCols());
  }

	mat9.createWithGlobalSize(5, 4, 3, PETSC_COMM_WORLD);
	if(rank == 0){
    printf("global rows: %d\n", mat9.globalRows());
    printf("global columns: %d\n", mat9.globalCols());
  }	

	// matrix-matrix multiplication
	if(rank == 0) printf("test matrix multiply:\n");
	mat1.print();
	mat2.print();
	mat1.multiply(mat2, mat3);
	mat3.print();

	// test write
	mat3.write("test.mat"); // mat3.dat?

  return;

}

// compute identity matrix
PetscSparseMatrix computeIdentity(int N, int rank)
{

	// make a matrix
	PetscSparseMatrix I;
	I.create(PETSC_COMM_WORLD, N, 1);

	// check bounds
	// printf("rank: %d, ilower: %d\n", rank, I.ilower());
	// printf("rank: %d, iupper: %d\n", rank, I.iupper());

	// fill the matrix
	for(int i = I.ilower(); i < I.iupper(); i++)
	{

		// printf("rank: %d, row: %d\n", rank, i);
		double temp = 1;
		I.insert(i, 1, &temp, &i); // row, number of columns, values, columns
		
	}

	// assemble matrix and return
	I.close();
	return I;
}

// compute 2D Laplace operator
PetscSparseMatrix compute2DLaplaceOperator(int N, int rank)
{
	// make a matrix
	PetscSparseMatrix laplace2D;
	laplace2D.create(PETSC_COMM_WORLD, N, 5);

	// size of dummy mesh
	int n = int(sqrt(N));
	if (rank == 0) printf("rank: %d, n: %d\n", rank, n);

	double values[5];
	int cols[5];

	printf("rank: %d, ilower: %d\n", rank, laplace2D.ilower());
	printf("rank: %d, iupper: %d\n", rank, laplace2D.iupper());

	// fill the matrix
	for(int i = laplace2D.ilower(); i < laplace2D.iupper(); i++)
	{
		// reset for row i
		int nnz = 0;

		// -n position
		if(i-n >= 0)
		{
			cols[nnz] = i - n;
			values[nnz] = -1.0;
			nnz++;
		}

		// left position
		if(i-1 >= 0)
		{
			cols[nnz] = i - 1;
			values[nnz] = -1.0;
			nnz++;
		}

		// diagonal position
		cols[nnz] = i;
		values[nnz] = 4.0;
		nnz++;

		// right position
		if(i+1 < N)
		{
			cols[nnz] = i + 1;
			values[nnz] = -1.0;
			nnz++;
		}

		// +n position
		if(i+n < N)
		{
			cols[nnz] = i + n;
			values[nnz] = -1.0;
			nnz++;
		}

		laplace2D.insert(i, nnz, values, cols); // row, number of columns, values, columns
	}

	// close and return the matrix
	laplace2D.close();
	return laplace2D;
}

void testSolvers(int rank, int size)
{

	// size of mesh
	int n = size;
	int N = n*n; // mat size

	PetscSparseMatrix testMatrix = compute2DLaplaceOperator(N, rank);
	PetscSparseMatrix preconditioner = computeIdentity(N, rank);

	// check the matrix was populated correctly
	int row0, row1, rown; // number of entires

	// check with arrays
	double vals0[5], vals1[5], valsn[5];
	int ind0[5], ind1[5], indn[5]; 

	// check with vectors
	// std::vector<double> vals0(5), vals1(5), valsn(5); // values
	// std::vector<int> ind0(5), ind1(5), indn(5); // columns

	// check matrix values
	if (rank == 0)
	{

		testMatrix.getRow(0, row0, vals0, ind0);
		testMatrix.getRow(1, row1, vals1, ind1);
		testMatrix.getRow(n, rown, valsn, indn);

		// row zero
		printf("row0: %d\n", row0);
		for(int i = 0; i < row0; i++)
		{
			printf("testMatrix[0][%d] = %f\n", ind0[i], vals0[i]);
		}

		// row one
		printf("row1: %d\n", row1);
		for(int i = 0; i < row1; i++)
		{
			printf("testMatrix[1][%d] = %f\n", ind1[i], vals1[i]);
		}

		// row n
		printf("rown: %d\n", rown);
		for(int i = 0; i < rown; i++)
		{
			printf("testMatrix[n][%d] = %f\n", indn[i], valsn[i]);
		}
	}

	// test linear algebra operations
	std::vector<double> ones, zeros, random1, random2;
	for(int i = 0; i < N; i++)
	{
		// make vectors
		zeros.push_back(0);
		ones.push_back(1);
		random1.push_back(rand()%20 - 10);
		random2.push_back(rand()%30 - 15);
	}

	// make Petsc vectors
	PetscVector x, b, x0;
	b.create(ones);
	x.create(random1);
	x0.create(random2); // initial guess

	// calculate norm
	double norm2x;
	x.norm2(norm2x);
	if (rank == 0) printf("norm of x: %f\n", norm2x);

	// dot product
	double testdot;
	b.dot(b, &testdot);
	if (rank == 0) printf("b dot b (should be %d): %f\n", N, testdot);

	// test 1-norm
	double norm1;
	b.norm1(norm1);
	if (rank == 0) printf("1-norm of b (should be %d): %f\n", N, norm1);

	// test 2-norm
	double norm2;
	b.norm2(norm2);
	if (rank == 0) printf("2-norm of b (should be %d): %f\n", n, norm2);

	// test inf-norm
	double norminf;
	b.normInf(norminf);
	if (rank == 0) printf("Inf-norm of b (should be %d): %f\n", 1, norminf);

	// matrix-vector multiplication
	testMatrix.multiply(x, b);

	// compute residual
	PetscVector r(b);
	testMatrix.residual(x, b, r);
	double normRes;
	r.normInf(normRes);
	if (rank == 0) printf("Inf-norm of f (should be %d): %f\n", 0, normRes);

	// create linear solver and parameters
	LinearSolverParameters params;
	PetscSolver solver(params);

	// Krylov solver
	params.verbosity = 1;
	params.solverType = "gmres";
	params.krylov.maxRestart = 30;
	params.krylov.tolerance = 1e-7;

	PetscVector solKrylov(b);
	double normKrylov, normTrue;

	solver.solve(PETSC_COMM_WORLD, testMatrix, solKrylov, b);
	solKrylov.norm2(normKrylov);
	x.norm2(normTrue);

	// x and solKrylov should be equal
	if(rank == 0) printf("Krylov solution. This should be zero: %f\n", std::fabs(normKrylov/normTrue - 1));
	if(rank == 0) printf("These should be the same: %f, %f\n", solKrylov.getElement(n), x.getElement(n));
	if(rank == 0) printf("These should be the same: %f, %f\n", solKrylov.getElement(2*n), x.getElement(2*n));


	// direct solver
	params.solverType = "direct";

	PetscVector solDirect(b);
	double normDirect;

	solver.solve(PETSC_COMM_WORLD, testMatrix, solDirect, b);
	solKrylov.norm2(normDirect);

	// x and solAMG should be equal
	if(rank == 0) printf("Direct solution. This should be zero: %f\n", std::fabs(normDirect/normTrue - 1));
	if(rank == 0) printf("These should be the same: %f, %f\n", solDirect.getElement(n), x.getElement(n));
	if(rank == 0) printf("These should be the same: %f, %f\n", solDirect.getElement(3*n), x.getElement(3*n));

	// AMG solver
  params.preconditionerType = "amg";
	params.amg.maxLevels = 10;
  params.amg.smootherType = "jacobi";
	params.amg.coarseType = "direct";

	PetscVector solAMG(b);
	double normAMG;

	solver.solve(PETSC_COMM_WORLD, testMatrix, solAMG, b);
	solAMG.norm2(normAMG);

	// x and solAMG should be equal
	if(rank == 0) printf("AMG solution. This should be zero: %f\n", std::fabs(normAMG/normTrue - 1));
	if(rank == 0) printf("These should be the same: %f, %f\n", solAMG.getElement(n), x.getElement(n));
	if(rank == 0) printf("These should be the same: %f, %f\n", solAMG.getElement(5), x.getElement(5));	
}

int main()
{

	// Hannah trials
	PetscInitializeNoArguments();

	int rank;
  MPI_Comm_rank(PETSC_COMM_WORLD, &rank);

	// test vector and matrix
	testPetscVector(rank);
	testPetscSparseMatrix(rank);

  // test computeIdentity
	PetscSparseMatrix mat1 = computeIdentity(3, rank);
	mat1.print();

	// test compute2DLaplaceOperator
	PetscSparseMatrix mat2 = compute2DLaplaceOperator(8, rank);
	mat2.print();

	// test solvers
	testSolvers(rank, 10);

	PetscFinalize();
  return 0;
}

/* compare to testLAOperations.cpp in GOESX:
computeIdentity() (same as above)
compute2DLaplaceOperator() (same as above)
testInterfaceSolvers()
  create matrix and vectors
	test norms (same as above)
	test solvers
	getRowCopy
  clearRow
testGEOSXSolvers()
testGEOSXBlockSolvers()
testMatrixMatrixOperations()
  test matrix-matrix multiply
testRectangularMatrixOperations()	
*/