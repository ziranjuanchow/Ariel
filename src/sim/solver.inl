// Ariel: FLIP Fluid Simulator
// Written by Yining Karl Li
//
// File: solver.inl
// Breakout file for PCG solver

#ifndef SOLVER_INL
#define SOLVER_INL

#include <tbb/tbb.h>
#include "../grid/macgrid.inl"
#include "../grid/particlegrid.hpp"
#include "../grid/levelset.hpp"
#include "../utilities/utilities.h"
#include "../grid/gridutils.inl"

namespace fluidCore {
//====================================
// Struct and Function Declarations
//====================================

//Forward declarations for externed inlineable methods
extern inline void solve(macgrid& mgrid, const int& subcell, const bool& verbose);
inline void flipDivergence(macgrid& mgrid);
inline void buildPreconditioner(grid<float>* pc, macgrid& mgrid, int subcell);
inline float fluidRef(grid<int>* A, int i, int j, int k, int qi, int qj, int qk, 
					  glm::vec3 dimensions);
inline float preconditionerRef(grid<float>* p, int i, int j, int k, glm::vec3 dimensions);
inline float fluidDiag(grid<int>* A, grid<float>* L, int i, int j, int k, glm::vec3 dimensions, 
					   int subcell);
inline void solveConjugateGradient(macgrid& mgrid, grid<float>* pc, int subcell, 
								   const bool& verbose);
inline void computeAx(grid<int>* A, grid<float>* L, grid<float>* X, grid<float>* target, 
					  glm::vec3 dimensions, int subcell);
inline float xRef(grid<int>* A, grid<float>* L, grid<float>* X, glm::vec3 f, glm::vec3 p, 
				  glm::vec3 dimensions, int subcell);
inline void op(grid<int>* A, grid<float>* X, grid<float>* Y, grid<float>* target, float alpha, 
			   glm::vec3 dimensions);
inline float product(grid<int>* A, grid<float>* X, grid<float>* Y, glm::vec3 dimensions);
inline void applyPreconditioner(grid<float>* Z, grid<float>* R, grid<float>* P, grid<float>* L, 
								grid<int>* A, glm::vec3 dimensions);

//====================================
// Function Implementations
//====================================

//Takes a grid, multiplies everything by -1
void flipGrid(grid<float>* grid, glm::vec3 dimensions){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	tbb::parallel_for(tbb::blocked_range<unsigned int>(0,x),
		[=](const tbb::blocked_range<unsigned int>& r){
			for(unsigned int i=r.begin(); i!=r.end(); ++i){
		  		for(unsigned int j=0; j<y; ++j){
					for(unsigned int k=0; k<z; ++k){
						float flipped = -grid->getCell(i,j,k);
						grid->setCell(i,j,k,flipped);
					}
				}      	
	      	}
	    }
    );
}

//Helper for preconditioner builder
float ARef(grid<int>* A, int i, int j, int k, int qi, int qj, int qk, glm::vec3 dimensions){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	if( i<0 || i>x-1 || j<0 || j>y-1 || k<0 || k>z-1 || A->getCell(i,j,k)!=FLUID ){ //if not liquid
		return 0.0;
	} 
	//if not liquid
	if( qi<0 || qi>x-1 || qj<0 || qj>y-1 || qk<0 || qk>z-1 || A->getCell(qi,qj,qk)!=FLUID ){ 
		return 0.0;
	} 
	return -1.0;	
}

//Helper for preconditioner builder
float PRef(grid<float>* p, int i, int j, int k, glm::vec3 dimensions){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	if( i<0 || i>x-1 || j<0 || j>y-1 || k<0 || k>z-1 || p->getCell(i,j,k)!=FLUID ){ //if not liquid
		return 0.0f;
	} 
	return p->getCell(i,j,k);
}

//Helper for preconditioner builder
float ADiag(grid<int>* A, grid<float>* L, int i, int j, int k, glm::vec3 dimensions, int subcell){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	float diag = 6.0;
	if( A->getCell(i,j,k) != FLUID ){
		return diag;
	}
	int q[][3] = { {i-1,j,k}, {i+1,j,k}, {i,j-1,k}, {i,j+1,k}, {i,j,k-1}, {i,j,k+1} };
	for( int m=0; m<6; m++ ) {
		int qi = q[m][0];
		int qj = q[m][1];
		int qk = q[m][2];
		if( qi<0 || qi>x-1 || qj<0 || qj>y-1 || qk<0 || qk>z-1 || A->getCell(qi,qj,qk)==SOLID ){
			diag -= 1.0;
		}
		else if( A->getCell(qi,qj,qk)==AIR && subcell ) {
			diag -= L->getCell(qi,qj,qk)/glm::min(1.0e-6f,L->getCell(i,j,k));
		}
	}
	
	return diag;
}

//Does what it says
void buildPreconditioner(grid<float>* pc, macgrid& mgrid, int subcell){
	int x = (int)mgrid.dimensions.x; int y = (int)mgrid.dimensions.y; 
	int z = (int)mgrid.dimensions.z;
	float a = 0.25f;
	tbb::parallel_for(tbb::blocked_range<unsigned int>(0,x),
		[=](const tbb::blocked_range<unsigned int>& r){
			for(unsigned int i=r.begin(); i!=r.end(); ++i){
				for(unsigned int j=0; j<y; ++j){
					for(unsigned int k=0; k<z; ++k){
						if(mgrid.A->getCell(i,j,k)==FLUID){	
							float left = ARef(mgrid.A,i-1,j,k,i,j,k,mgrid.dimensions) * 
										 PRef(pc,i-1,j,k,mgrid.dimensions);
							float bottom = ARef(mgrid.A,i,j-1,k,i,j,k,mgrid.dimensions) * 
										   PRef(pc,i,j-1,k,mgrid.dimensions);
							float back = ARef(mgrid.A,i,j,k-1,i,j,k,mgrid.dimensions) * 
										 PRef(pc,i,j,k-1,mgrid.dimensions);
							float diag = ADiag(mgrid.A, mgrid.L,i,j,k,mgrid.dimensions,subcell);
							float e = diag - (left*left) - (bottom*bottom) - (back*back);
							if(diag>0){
								if( e < a*diag ){
									e = diag;
								}
								pc->setCell(i,j,k, 1.0f/glm::sqrt(e));
							}
						}
					}
				}
			}
		}
	);
}

//Helper for PCG solver: read X with clamped bounds
float xRef(grid<int>* A, grid<float>* L, grid<float>* X, glm::vec3 f, glm::vec3 p, 
		   glm::vec3 dimensions, int subcell){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	int i = glm::min(glm::max(0,(int)p.x),x-1); int fi = (int)f.x;
	int j = glm::min(glm::max(0,(int)p.y),y-1); int fj = (int)f.y;
	int k = glm::min(glm::max(0,(int)p.z),z-1); int fk = (int)f.z;
	if(A->getCell(i,j,k) == FLUID){
		return X->getCell(i,j,k);
	}else if(A->getCell(i,j,k) == SOLID){
		return X->getCell(fi,fj,fk);
	} 
	if(subcell){
		return L->getCell(i,j,k)/glm::min(1.0e-6f,L->getCell(fi,fj,fk))*X->getCell(fi,fj,fk);
	}else{
		return 0.0f;
	}
}

// target = X + alpha*Y
void op(grid<int>* A, grid<float>* X, grid<float>* Y, grid<float>* target, float alpha, 
		glm::vec3 dimensions){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	for(unsigned int j=0; j<y; ++j){
		for(unsigned int k=0; k<z; ++k){
			//this parallel loop has to be the inner loop or else MSVC will barf
			tbb::parallel_for(tbb::blocked_range<unsigned int>(0,x),
				[=](const tbb::blocked_range<unsigned int>& r){
					for(unsigned int i=r.begin(); i!=r.end(); ++i){
						if(A->getCell(i,j,k)==FLUID){
							float targetval = X->getCell(i,j,k)+alpha*Y->getCell(i,j,k);
							target->setCell(i,j,k,targetval);
						}else{
							target->setCell(i,j,k,0.0f);
						}				
					}
				}
			);
		}
	}
}

// ans = x^T * x
float product(grid<int>* A, grid<float>* X, grid<float>* Y, glm::vec3 dimensions){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	float result = 0.0f;
	for(unsigned int i=0; i<x; i++){
		for(unsigned int j=0; j<y; j++){
			for(unsigned int k=0; k<z; k++){
				if(A->getCell(i,j,k)==FLUID){
					result += X->getCell(i,j,k) * Y->getCell(i,j,k);
				}
			}
		}
	}
	return result;
}

//Helper for PCG solver: target = AX
void computeAx(grid<int>* A, grid<float>* L, grid<float>* X, grid<float>* target, 
			   glm::vec3 dimensions, int subcell){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	float n = (float)glm::max(glm::max(x,y),z);
	float h = 1.0f/(n*n);
	tbb::parallel_for(tbb::blocked_range<unsigned int>(0,x),
		[=](const tbb::blocked_range<unsigned int>& r){
			for(unsigned int i=r.begin(); i!=r.end(); ++i){
				for(unsigned int j=0; j<y; ++j){
					for(unsigned int k=0; k<z; ++k){
						if(A->getCell(i,j,k) == FLUID){
							float result = (6.0f*X->getCell(i,j,k)
											-xRef(A, L, X, glm::vec3(i,j,k), glm::vec3(i+1,j,k), 
												  dimensions, subcell)
											-xRef(A, L, X, glm::vec3(i,j,k), glm::vec3(i-1,j,k), 
												  dimensions, subcell)
											-xRef(A, L, X, glm::vec3(i,j,k), glm::vec3(i,j+1,k), 
												  dimensions, subcell)
											-xRef(A, L, X, glm::vec3(i,j,k), glm::vec3(i,j-1,k), 
												  dimensions, subcell)
											-xRef(A, L, X, glm::vec3(i,j,k), glm::vec3(i,j,k+1), 
												  dimensions, subcell)
											-xRef(A, L, X, glm::vec3(i,j,k), glm::vec3(i,j,k-1), 
												  dimensions, subcell)
											)/h;
							target->setCell(i,j,k,result);
						} else {
							target->setCell(i,j,k,0.0f);
						}
					}
				}
			}
		}
	);
}

void applyPreconditioner(grid<float>* Z, grid<float>* R, grid<float>* P, grid<float>* L, 
						 grid<int>* A, glm::vec3 dimensions){
	int x = (int)dimensions.x; int y = (int)dimensions.y; int z = (int)dimensions.z;
	grid<float>* Q = new grid<float>(dimensions, 0.0f);

	// LQ = R
	tbb::parallel_for(tbb::blocked_range<unsigned int>(0,x),
		[=](const tbb::blocked_range<unsigned int>& r){
			for(unsigned int i=r.begin(); i!=r.end(); ++i){
				for(unsigned int j=0; j<y; ++j){
					for(unsigned int k=0; k<z; ++k){
						if(A->getCell(i,j,k) == FLUID) {
							float left = ARef(A,i-1,j,k,i,j,k,dimensions)*
										 PRef(P,i-1,j,k,dimensions)*PRef(Q,i-1,j,k,dimensions);
							float bottom = ARef(A,i,j-1,k,i,j,k,dimensions)*
										   PRef(P,i,j-1,k,dimensions)*PRef(Q,i,j-1,k,dimensions);
							float back = ARef(A,i,j,k-1,i,j,k,dimensions)*
										 PRef(P,i,j,k-1,dimensions)*PRef(Q,i,j,k-1,dimensions);
							
							float t = R->getCell(i,j,k) - left - bottom - back;
							float qVal = t * P->getCell(i,j,k);
							Q->setCell(i,j,k,qVal);
						}
					}
				}
			}
		}
	);

	// L^T Z = Q
	for(int j=y-1; j>=0; j--){
		for(int k=z-1; k>=0; k--){
			//this parallel loop has to be the inner loop or else MSVC will barf
			tbb::parallel_for(tbb::blocked_range<int>(-1,x-1),
				[=](const tbb::blocked_range<int>& r){
					for(int i=r.end(); i!=r.begin(); i--){
						if(A->getCell(i,j,k) == FLUID){
							float right = ARef(A,i,j,k,i+1,j,k,dimensions)*
										  PRef(P,i,j,k,dimensions)*PRef(Z,i+1,j,k,dimensions);
							float top = ARef(A,i,j,k,i,j+1,k,dimensions)*
										PRef(P,i,j,k,dimensions)*PRef(Z,i,j+1,k,dimensions);
							float front = ARef(A,i,j,k,i,j,k+1,dimensions)*
										  PRef(P,i,j,k,dimensions)*PRef(Z,i,j,k+1,dimensions);
						
							float t = Q->getCell(i,j,k) - right - top - front;
							float zVal = t * P->getCell(i,j,k);
							Z->setCell(i,j,k,zVal);
						}
					}
				}
			);
		}
	}
	delete Q;
}

//Does what it says
void solveConjugateGradient(macgrid& mgrid, grid<float>* PC, int subcell, const bool& verbose){
	int x = (int)mgrid.dimensions.x; int y = (int)mgrid.dimensions.y; 
	int z = (int)mgrid.dimensions.z;

	grid<float>* R = new grid<float>(mgrid.dimensions, 0.0f);
	grid<float>* Z = new grid<float>(mgrid.dimensions, 0.0f);
	grid<float>* S = new grid<float>(mgrid.dimensions, 0.0f);

	//note: we're calling pressure "mgrid.P" instead of x

	computeAx(mgrid.A, mgrid.L, mgrid.P, Z, mgrid.dimensions, subcell);	// z = apply A(x)
	op(mgrid.A, mgrid.D, Z, R, -1.0f, mgrid.dimensions);                // r = b-Ax
	float error0 = product(mgrid.A, R, R, mgrid.dimensions);			// error0 = product(r,r)

	// z = f(r), aka preconditioner step
	applyPreconditioner(Z, R, PC, mgrid.L, mgrid.A, mgrid.dimensions);	

	//s = z. TODO: replace with VDB deep copy?

	for(unsigned int j=0; j<y; ++j ){
		for(unsigned int k=0; k<z; ++k ){
			//this parallel loop has to be the inner loop or else MSVC will barf
			tbb::parallel_for(tbb::blocked_range<unsigned int>(0,x),
				[=](const tbb::blocked_range<unsigned int>& r){
					for(unsigned int i=r.begin(); i!=r.end(); ++i){
						S->setCell(i,j,k,Z->getCell(i,j,k));
					}
				}
			);	
		}
	}


	float eps = 1.0e-2f * (x*y*z);
	float a = product(mgrid.A, Z, R, mgrid.dimensions);					// a = product(z,r)

	for( int k=0; k<x*y*z; k++){
		//Solve current iteration
		computeAx(mgrid.A, mgrid.L, S, Z, mgrid.dimensions, subcell);	// z = applyA(s)
		float alpha = a/product(mgrid.A, Z, S, mgrid.dimensions);		// alpha = a/(z . s)
		op(mgrid.A, mgrid.P, S, mgrid.P, alpha, mgrid.dimensions);		// x = x + alpha*s
		op(mgrid.A, R, Z, R, -alpha, mgrid.dimensions);					// r = r - alpha*z;
		float error1 = product(mgrid.A, R, R, mgrid.dimensions);		// error1 = product(r,r)
        error0 = glm::max(error0, error1);
        //Output progress
        float rate = 1.0f - glm::max(0.0f,glm::min(1.0f,(error1-eps)/(error0-eps)));
        if(verbose){
        	std::cout << "PCG Iteration " << k+1 << ": " << 100.0f*pow(rate,6) << "%% solved" 
        			  << std::endl;
        }
        if(error1<=eps){
        	break;
        }
        //Prep next iteration
        // z = f(r)
        applyPreconditioner(Z, R, PC, mgrid.L, mgrid.A, mgrid.dimensions);
        float a2 = product(mgrid.A, Z, R, mgrid.dimensions);				// a2 = product(z,r)
        float beta = a2/a;													// beta = a2/a
        op(mgrid.A, Z, S, S, beta, mgrid.dimensions);						// s = z + beta*s
        a = a2;
	}

	delete R;
	delete Z;
	delete S;
}

void solve(macgrid& mgrid, const int& subcell, const bool& verbose){

	//if in VDB mode, force to single threaded to prevent VDB write issues. 
	//this is a kludgey fix for now.
	// if(mgrid.type==VDB){
	// 	omp_set_num_threads(1);
	// }

	//flip divergence
	flipGrid(mgrid.D, mgrid.dimensions);

	//build preconditioner
	grid<float>* preconditioner = new grid<float>(mgrid.dimensions, 0.0f);
	buildPreconditioner(preconditioner, mgrid, subcell);

	//solve conjugate gradient
	solveConjugateGradient(mgrid, preconditioner, subcell, verbose);

	delete preconditioner;

	// if(mgrid.type==VDB){
	// 	omp_set_num_threads(omp_get_num_procs());
	// }
}
}

#endif
