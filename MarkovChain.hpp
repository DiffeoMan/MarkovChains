/**
 * @summary : implementation of a Markov chain.
 * Note that any individual functions not written by the author here have source links in the comments above the declaration
 * @author : Zane Jakobs
 */
#ifndef EIGEN_USE_MKL_ALL
#define EIGEN_USE_MKL_ALL
#endif
#ifndef MarkovChain_hpp
#define MarkovChain_hpp
#include<mkl.h>
#include<vector>
#include<Eigen/Core>
#include<Eigen/Eigenvalues>
#include <Eigen/src/Core/util/Constants.h>
#include<Eigen/Dense>
#include<random>
#include<iostream>
#include<cmath>
#include<mkl.h>
#include<type_traits>
#include<complex>
using namespace std;

namespace Markov
{
    
    
    /**
     * Taken from https://software.intel.com/en-us/node/521147
     * @summary: C++ declaration of FORTRAN function dgeev
     *
     */
    extern "C" lapack_int LAPACKE_dgeev( int matrix_layout, char jobvl, char jobvr, lapack_int n, double* a, lapack_int lda, double* wr, double* wi, double* vl, lapack_int ldvl, double* vr, lapack_int ldvr );
    
    
    typedef std::complex<double> CD;
    
    typedef Eigen::Matrix<complex<double>,Eigen::Dynamic,Eigen::Dynamic> MatrixXcd;
    
    /**
     * @author: Zane Jakobs
     * @param mat: matrix to convert to LAPACKE form
     * @return: pointer to array containing contents of mat in column-major order
     */
    double* Eigen_to_LAPACKE(Eigen::MatrixXd& mat){
        int n = mat.cols();
        int m = mat.rows();
        if(m != n){
            throw "Error: matrix is not square.";
            return nullptr;
        }
        double *a = new double[n*n]; //array to store values
        for(int i = 0; i < n; i++){
            for(int j = 0; j < n; j++){
                a[i*n + j] = mat(j,i);
            }
        }
        return a;
    }
    /**
     * taken from print function in https://software.intel.com/sites/products/documentation/doclib/mkl_sa/11/mkl_lapack_examples/lapacke_sgeev_col.c.htm
     fills matrix where columns are eigenvectors in row major order
     * @param n: dimension of matrix
     * @param v: array of eigenvectors
     * @param ldv: dimension of array v
     */
    Eigen::MatrixXcd LAPACKE_evec_to_Eigen(MKL_INT n, double* wi, double* v, MKL_INT ldv){
        Eigen::MatrixXcd mat(n,n);
        
        MKL_INT j;
        for(MKL_INT i = 0; i < n; i ++){
            j = 0;
            while( j < n){
                if(wi[j] == 0.0){
                    CD temp(v[i + j*ldv],0.0);
                    mat(i,j) = temp;
                    j++;
                }
                else{
                    CD temp(v[i + j*ldv],v[i+(j+1)*ldv]);
                    mat(i,j) = temp;
                    CD temp2(v[i + j*ldv], -v[i+(j+1)*ldv]);
                    mat(i,j+1) = temp2;
                    j+=2;
                }
            }//end while
        }//end for
        return mat;
    }
    
    /**
     * taken from print function in https://software.intel.com/sites/products/documentation/doclib/mkl_sa/11/mkl_lapack_examples/lapacke_sgeev_col.c.htm
     fills vector with eigenvalues
     */
    Eigen::MatrixXcd LAPACKE_eval_to_Eigen(MKL_INT n, double* wr, double* wi){
        Eigen::MatrixXcd eval(1,n);
        for(MKL_INT j = 0; j < n; j++){
            CD temp(wr[j],wi[j]);
            eval(0,j) = temp;
        }
        return eval;
    }
    
    
    
    /**
     * @summary: solves eigen-problem
     * A * v(i) = lambda(i)* v(i)
     * @param A: nxn matrix whose eigenstuff we want
     * @param v: nxn matrix to hold eigenvectors
     * @param lambda: 1xn matrix (row vector)
     * @return: true for success, false for failure
     */
    bool eigen_problem(Eigen::MatrixXd& A, MatrixXcd& v,
                      MatrixXcd& lambda){
        //matrices in column major order (eigen default), compute right e-vecs
        //only
        
        int cls = A.cols();
        MKL_INT n = cls;
        MKL_INT lda = n, ldvl = n, ldvr = n, info;
        double wr[cls], wi[cls], vl[cls*cls], vr[cls*cls];
        double* a = Eigen_to_LAPACKE(A);
        info = LAPACKE_dgeev(LAPACK_COL_MAJOR, 'N', 'V', n, a, lda, wr,
                             wi, vl, ldvl, vr, ldvr);
        //delete memory allocated to a
        delete a;
        if(info > 0){
            //failure condition
            std::cout << "The solver failed to solve the eigen-problem." << endl;
            return false;
        }
        lambda = LAPACKE_eval_to_Eigen(n, wr, wi);
        v = LAPACKE_evec_to_Eigen(n, wi, vr, ldvr);
        return true;
    }
    /**
     *@author: Zane Jakobs
     *@summary: default template
     */
    
   
    Eigen::MatrixXd normalize_rows(Eigen::MatrixXd &mat){
        for(int i = 0; i < mat.rows(); i++){
            mat.row(i) = mat.row(i)/(mat.row(i).sum());
        }
        return mat;
    }
    /**
     *@author: Zane Jakobs
     *@param mat: matrix to raise to power
     *@param expon: power
     *@return: mat^expon
     */
     Eigen::MatrixXcd matrix_power(Eigen::MatrixXd &mat, int expon){
         int n = mat.cols();
        Eigen::MatrixXcd v(n,n);
        Eigen::MatrixXcd v2(n,n);
        Eigen::MatrixXcd lambda(1,n);
        bool success = eigen_problem(mat, v, lambda);
        Eigen::MatrixXd res(n,n);
        if(success){
            Eigen::MatrixXcd D = Eigen::MatrixXcd::Zero(n,n);
            for(int i = 0; i < n; i++){
                D(i,i) = std::pow(lambda(0,i), expon);
            }
            return (v*D*v.inverse()).real();
        }else{
            throw "Error: Solver failed.";
            return (v).real();
        }
    }
    template<typename M>
    M characteristic_polynomial(Eigen::MatrixXd &mat, M &x){
        int n = mat.cols();
        Eigen::MatrixXcd v(n,n);
        Eigen::MatrixXcd lambda(1,n);
        bool success = eigen_problem(mat, v, lambda);
        M res;
        if(success){
            res = (x-lambda(0,0));
            for(int i = 1; i < n; i++){
                res *= (x-lambda(0,i));
            }
        }else{
            res = x;
        }
        return res;
    }
    template<>
    Eigen::MatrixXcd characteristic_polynomial<Eigen::MatrixXcd>(Eigen::MatrixXd &mat, Eigen::MatrixXcd &x){
        int n = mat.cols();
        Eigen::MatrixXcd v(n,n);
        Eigen::MatrixXcd lambda(1,n);
        bool success = eigen_problem(mat, v, lambda);
        Eigen::MatrixXcd res;
        Eigen::MatrixXcd I = Eigen::MatrixXcd::Identity(n,n);;
        if(success){
            res = (x-lambda(0,0)*I);
            for(int i = 1; i < n; i++){
                res *= (x-lambda(0,i)*I);
            }
        } else{
            res = x;
        }
        return res;
    }
    
    template<>
    Eigen::MatrixXd characteristic_polynomial<Eigen::MatrixXd>(Eigen::MatrixXd &mat, Eigen::MatrixXd &x){
        int n = mat.cols();
        Eigen::MatrixXcd v(n,n);
        Eigen::MatrixXcd lambda(1,n);
        bool success = eigen_problem(mat, v, lambda);
        Eigen::MatrixXd lbda(1,n);
        lbda = lambda.real();
        Eigen::MatrixXcd res;
        Eigen::MatrixXcd I = Eigen::MatrixXd::Identity(n,n);;
        if(success){
            res = (x-lbda(0,0)*I);
            for(int i = 1; i < n; i++){
                res *= (x-lbda(0,i)*I);
            }
        } else{
            res = x;
        }
        return res.real();
    }
    
    
    typedef struct
    {
        std::vector<int> seq;
        void set_seq(std::vector<int> _seq){
            seq = _seq;
        }
    }Sequence;
    
    
class MarkovChain
{
    
protected:
    Eigen::MatrixXd _transition, _initial;
    int numStates;
    
public:
    
    friend std::ostream& operator<<(ostream &os, const MarkovChain &MC){
        os << "Probability Matrix:" <<endl;
        
        Eigen::MatrixXd mat =  MC.getTransition();
        os << "  || ";
        for(int i = 0; i < mat.cols(); i++){
            os << i << " ";
        }
        os <<endl << "  ";
        
        static const int widthMult = 6;
        for(int i = 0; i < widthMult*mat.cols(); i++){
            os << "=";
        }
        os << endl;
        for(int i = 0; i < mat.cols(); i++){
            os << i << " || ";
            for(int j = 0; j < mat.cols(); j++){
                os << mat(i,j) << " , ";
            }
            os << "||" <<endl;
        }
        return os;
    }
    
    
    /**
     * @name MarkovChain::setModel
     * @summary : setModel sets parameters
     * @param: transition: N x N transition matrix
     * @param: initial: 1 x N initial probability row vector
     * @source: https://www.codeproject.com/Articles/808292/Markov-chain-implementation-in-Cplusplus-using-Eig
     */
    
    void setModel( Eigen::MatrixXd transition, Eigen::MatrixXd initial, int _numStates){
        _transition = transition;
        _initial = initial;
        numStates = _numStates;
    }
    
    void setTransition(Eigen::MatrixXd transition){
        _transition = transition;
        numStates = transition.cols();
}
    
    void setInitial(Eigen::MatrixXd initial){
        _initial = initial;
    }
    void setNumStates(int _num){
        numStates = _num;
    }
    
    Eigen::MatrixXd getTransition(void) const { return _transition; }
    Eigen::MatrixXd getInit() const { return _initial; }
    int getNumStates(void) const {return numStates;}
    
    
    
    
    
    
    MarkovChain() {};
    
    MarkovChain( Eigen::MatrixXd transition, Eigen::MatrixXd initial, int _numStates){
        _transition = transition;
        _initial = initial;
        numStates = _numStates;
    }
    ~MarkovChain(){
        _transition.resize(0,0);
        _initial.resize(0,0);
        numStates = 0;
    }
    
    
    
    
    /**
     * @author: Zane Jakobs
     * @summary: counts transitions
     * @param dat: the data
     * @param oversize: upper bound on the number of states in the chain
     * @return: estimate of transition matrix, with zero entries where they should be to
     * preserve communicating classes
     */
    static Eigen::MatrixXd countMat(vector<Sequence> df, int oversize = 100){
        Eigen::MatrixXd mat = Eigen::MatrixXd::Zero(oversize,oversize);
        
        //loop through data, entering values
        int largest = 0; //number of unique states
        for(auto it = df.begin(); it != df.end(); it++){
            for(auto jt = (*it).seq.begin(); jt++ != (*it).seq.end(); jt++){
                auto pos = jt;
                int s1 = *pos;
                int s2 = *(pos++);
                mat(s1,s2) = mat(s1,s2) +1;
                if(s1 > largest || s2 > largest){
                    s1 > s2 ? largest = s1 : largest = s2;
                }
            }
        }
        largest++;
        /*
         set matrix size to largest x largest, preserving values
         */
        mat.conservativeResize(largest,largest);
        mat = normalize_rows(mat);
        return mat;
    }
    /**
     * @author: Zane Jakobs
     * @summary: computes transition matrix based on empirical distribution of values. In
     * particular, attempts to find which entries should be zero
     * @param dat: the data
     * @param oversize: upper bound on the number of states in the chain
     * @return: maximum likelihood estimator of _transition
     */
   static Eigen::MatrixXd MLE(vector<Sequence> df, int oversize = 100){
       Eigen::MatrixXd mat = countMat(df, oversize);
        mat = normalize_rows(mat);
        return mat;
    }
    
    
    /**
     * @name MarkovChain::randTransition
     * @summary: initialRand: generates random state
     * @param matrix: transition matrix
     * @param index: current state
     * @param u: random uniform between 0 and 1
     * @return index corresponding to the transition we make
     */
     static int randTransition(Eigen::MatrixXd& mat, int ncols, int index, double u){
        double s = mat(index,0);
        if(u < s){
            return 0;
        }
        int i = 1;
    
        for(i = 1; i < ncols; i++ ){
            s += mat(index,i);
            if(u < s){
                return i;
            }
        }
        return (i);
    }
    
    
    /**
     3/9/19: FUNCTION IS NOT WORKING CORRECTLY. NO NOT USE UNTIL THIS COMMENT IS CHANGED
     * @name MarkovChain::generateSequence
     * @summary: generateSequence generates a sequence of length n from the Markov chain
     * @param n: length of sequence
     * @return: vector of ints representing the sequence
     */
    vector<int> generateSequence(int n, int nStates, Eigen::MatrixXd matT, Eigen::MatrixXd initialDist){
        
        std::vector<int> sequence(n);
        int i, id;
        i = 0;
        id = 0;
        double randNum;
        //set random seed
        random_device rd;
        //init Mersenne Twistor
        mt19937 gen(rd());
        // unif(0,1)
        uniform_real_distribution<> dis(0.0,1.0);
        
        //generate initial state via transition from 0 through initial distribution
        try{
            randNum = dis(gen);
            int init = randTransition(initialDist,numStates, 0, randNum);
            sequence[i] = init;
            id = init;
        }catch(const char* msg){
            cerr << msg << endl;
        }
        
        for(i = 1; i<n; i++){
            try{
                randNum = dis(gen);
                id = randTransition(matT,nStates, id, randNum);
                sequence[i] = id;
            }catch(const char* msg){
                cerr << msg << endl;
            }
            
        }
        return sequence;
        
    }
    
    /**
     * @name MarkovChain::stationaryDistributions
     * @summary: stationaryDistributions returns the last stationary distributions of the
     * Markov Chain.
     * @return: vector of doubles corresponding to the last stationary distribution (by order of the eigenvalues)
     */
    Eigen::Matrix<complex<double>,Eigen::Dynamic,Eigen::Dynamic> stationaryDistributions(){
        //instantiate eigensolver
        Eigen::EigenSolver<Eigen::MatrixXd> es;
        //transpose transition matrix to find left eigenvectors
        Eigen::MatrixXd pt = _transition.transpose();
        //compute evecs, evals
        es.compute( pt, true);
        //store evals
        Eigen::Matrix<complex<double>,Eigen::Dynamic,Eigen::Dynamic> evals = es.eigenvalues();
        
        Eigen::Matrix<complex<double>,Eigen::Dynamic,Eigen::Dynamic> evecs = es.eigenvectors();
        Eigen::Matrix<complex<double>,1,2> dotmat;
        dotmat(0,0) = 1;
        dotmat(0,1) = 0;
        int count = 0;
        int index;
        for(int i = 0; i< 2; i++){
            if(abs((evals(i)*dotmat).sum()) <= 1.01 && abs((evals(i)*dotmat).sum()) >= 0.99){
                count++;
                index = i;
            }
        }
        
        std::cout << endl << count << " eigenvalue(s) is (are) 1, and the last stationary distribution, (stateN,0), is " << (evecs.col(index).transpose())/(evecs.col(index).sum()) <<endl;
        return (evecs.col(index).transpose())/(evecs.col(index).sum());
    }
    
    /**
     * @name MarkovChain::limitingDistribution
     * @summary: limitingDistribution computes the limiting distribution of the Markov chain
     * @param expon: computes 10^expon powers of _transition
     * @return limmat.row(0): limiting distribution
     */
    Eigen::MatrixXd limitingDistribution(int expon){
        Eigen::MatrixXd limmat;
        limmat = _transition.pow(expon);
        return limmat.row(0);
    }
    
    /**
     * @name MarkovChain::limitingMat
     * @summary: limitingMat computes the infinite-limit matrix of the Markov chain
     * @param expon: computes 10^expon powers of _transition
     * @return limmat: limiting distribution matrix
     */
    Eigen::MatrixXd limitingMat(int expon){
        Eigen::MatrixXd limmat;
        limmat = _transition.pow(expon);
        return limmat;
    }
    
    /**
     * @summary: does mat contain key?
     * @return: answer to above question, true or false for yes or no
     */
    bool contains(Eigen::MatrixXd mat, double key){
        int n = mat.cols();
        for(int i = 0; i<n; i++){
            for(int j = 0; j < n; j++){
                if(mat(i,j) == key)  return true;
            }
        }
        return false;
    }
    /**
     * @author: Zane Jakobs
     * @return: matrix of number of paths of length n from state i to state j
     */
    Eigen::MatrixXd numPaths(int n){
        Eigen::MatrixXd logicMat(numStates,numStates);
        for(int i = 0; i< numStates; i++){
            for(int j = 0; j < numStates; j++){
                if(_transition(i,j) != 0){
                    logicMat(i,j) = 1;
                }
                else{
                    logicMat(i,j) = 0;
                }
            }//end inner for
        }
        if(n > 1){
            Eigen::MatrixXd powmat = logicMat.pow(n);
            return powmat;
        }
        
        return logicMat;
    }
    
    
    /**
     * @author: Zane Jakobs
     * @return 1 or 0 for if state j can be reached from state i
     */
    Eigen::MatrixXd isReachable(void){
        Eigen::MatrixXd R(numStates,numStates);
        Eigen::MatrixXd Rcomp = Eigen::MatrixXd::Zero(numStates,numStates);
    
        for(int i = 1; i <= numStates; i++){
            Rcomp += _transition.pow(i);
        }
        for(int i = 0; i< numStates; i++){
            for(int j = 0; j<numStates; j++){
                if(Rcomp(i,j) > 0){
                    R(i,j) = 1;
                }
                else{
                    
                    R(i,j) = 0;
                }
            }
        }
        return R;
    }
    
    
    bool isInVec(std::vector<int> v, int key){
        for(std::vector<int>::iterator it = v.begin(); it != v.end(); it++){
            if( *it == key){
                return true;
            }
        }
        return false;
    }
    /**
     * @author: Zane Jakobs
     * @return matrix where each row has a 1 in the column of each element in that communicating class (one row = one class)
     */
    Eigen::MatrixXd communicatingClasses(void){
        Eigen::MatrixXd CC(numStates,numStates);
        Eigen::MatrixXd reachable = isReachable();
        
        for(int i = 0; i< numStates; i++){
            for(int j = 0; j < numStates; j++){
                if(reachable(i,j) == 1 && reachable(j,i) == 1){
                    CC(i,j) = 1;
                }
                else{
                    CC(i,j) = 0;
                }
            }//end inner for
        }//end outer for
        
        /*
        int count = 0; //how many classes?
        int findId;
        bool found;
        Eigen::MatrixXd uniqueC = Eigen::MatrixXd::Zero(numStates,numStates);
        Eigen::MatrixXd temp = uniqueC;
        for(int i = 0; i< numStates; i++ ){
            found = false;
            for(int j = 0; j < numStates; j++){
                if(CC.row(j) == CC.row(i) && j != i){
                    found = true;
                    findId = i;
                }
            }
            if(!found){
                uniqueC.row(count) = CC.row(i);
                count++;
            }
            else{
                found = false;
                for(int i = 0; i < count && !found; i++){
                    if(uniqueC.row(i) == CC.row(findId)){
                        found = true;
                    }
                }
                if(!found){
                    uniqueC.row(count) = CC.row(findId);
                }
            }//end else
        }//end outer for
        Eigen::MatrixXd finC(count, numStates);
        for(int i = 0; i < count; i++){
            finC.row(i) = uniqueC.row(i);
        }
        return finC;
        */
        return CC;
    }
    /**
     * @author: Zane Jakobs
     * @return: expected value of (T = min n >= 0 s.t. X_n  = sh) | X_0 = s0
     * @param s0: initial state
     * @param sh: target state
     */
    double expectedHittingTime(int s0, int sh){
        /**
         Algorithm: let u_i = E[T|X_0 = i]. Set up system of equations. Solve for u_s0. We have the equation Au = c, where u = (u_0, u_1, ..., u_n)^T, c = (-1,-1,...,0,-1,...-1)^T, with 0 in the spot corresponding to sh, and
         
                            [p00-1, p01, p02, ... p0n]
                            [p10, p11-1, p12, ... p1n]
                    A =     [p20, p21, p22-1, ... p1n]
                                      ...
                            [0, 0, 0, ... 1, 0, ... 0]
                            [p(sh+1)0, ..., p(sh+1)n]
                            [pn0-1, pn1, pn2, ... pnn-1]
         
         where the 1 in the sh-th row is in the sh position.
         */
        Eigen::MatrixXd A(numStates, numStates);
        Eigen::VectorXd u(numStates);
        Eigen::VectorXd c(numStates);
        
        for(int i = 0; i<numStates; i++){
            //fill c
            if(i == sh){
                c(i) = 0;
            }
            else{
                c(i) = -1;
            }
        //fill A
        for(int j = 0; j< numStates; j++){
            //if we're in the row of 0's and a 1
            if(i == sh){
                if(j == sh){
                    A(i,j) = 1;
                }
                else{
                    A(i,j) = 0;
                }
            }//end if
            else{
                (j == i) ? (A(i,j) = _transition(i,j)-1) : (A(i,j) == _transition(i,j));
            }
        }//end inner for
    }//end outer for
    
    /*choose linear solver based on matrix size. Using info from Eigen docs at
     https://eigen.tuxfamily.org/dox/group__TutorialLinearAlgebra.html
     */
        if(numStates < 500){
            //for smaller matrices, rank-revealing Householder QR decomposition with column pivoting
            Eigen::ColPivHouseholderQR<Eigen::MatrixXd> CPH(A);
            u = CPH.solve(c);

        }
        else{
            //for big ones, use regular Householder QR
            Eigen::HouseholderQR<Eigen::MatrixXd> HQR(A);
            u = HQR.solve(c);
        }
        const double err_tol = 1.0e-4; //error tolerance on solutions
        double relative_error = (A*u - c).norm() / c.norm();
        if(relative_error < err_tol){
            return u(s0);
        }
        else{
            return -1; //return -1 if not in error bound
        }
    }
    
    /**
     * @author: Zane Jakobs
     * @param s0: initial state
     * @param sInt: intermediate state
     * @param sEnd: end state
     * @return: mean time the chain spends in sInt, starting at s0, before returning to s0
     */
    double meanTimeInStateBeforeReturn(int s0, int sInt){
        /*
         Algorithm: We want to solve Aw = c, where c = (0,0,...,-1,0,...,0)^T, with the -1 in the sInt position, and w = (w0, w1, ..., wsInt, ... ,wn)^T. We want ws0. We also define
         
                [p00-1, p01, ..., 0, ...,p0n]
                [p10, p11-1, ..., 0, ...,p1n]
         A =    [p20, p21, ..., 0, ...,  p2n]
                            ...
                [p(sEnd+1)0, p(sEnd+1)1, ...]
                [pn0, pn1, ..., 0, ..., pnn-1]
         
         where the 0's are in the s0 column
         */
        Eigen::MatrixXd A(numStates, numStates);
        Eigen::VectorXd w(numStates);
        Eigen::VectorXd c(numStates);
        
        for(int i = 0; i<numStates; i++){
            //fill c
            if(i == sInt){
                c(i) = -1;
            }
            else{
                c(i) = 0;
            }
            //fill A
            for(int j = 0; j< numStates; j++){
                //if we're in column s0
                if(j == s0){
                    (i == s0) ? A(i,j) = -1 : A(i,j) = 0;
                }
                else{
                    (j == i) ? A(i,j) = _transition(i,j) -1 : A(i,j) = _transition(i,j);
                }
            }//end inner for
        }//end outer for
        
        /*choose linear solver based on matrix size. Using info from Eigen docs at
         https://eigen.tuxfamily.org/dox/group__TutorialLinearAlgebra.html
         */
        if(numStates < 500){
            //for smaller matrices, rank-revealing Householder QR decomposition with column pivoting
            Eigen::ColPivHouseholderQR<Eigen::MatrixXd> CPH(A);
            w = CPH.solve(c);
        }
        else{
            //for big ones, use regular Householder QR
            Eigen::HouseholderQR<Eigen::MatrixXd> HQR(A);
            w = HQR.solve(c);
        }
        const double err_tol = 1.0e-4; //error tolerance on solutions
        double relative_error = (A*w - c).norm() / c.norm();
        if(relative_error < err_tol){
            return w(s0);
        }
        else{
            return -1; //return -1 if not in error bound
        }
    }
    /**
     * @author: Zane Jakobs
     * @param s0: initial state
     * @param sInt: intermediate state
     * @param sEnd: end state
     * @return: mean time the chain spends in sInt, starting at s0, before hitting sEnd
     */
    double meanTimeInStateBeforeHit(int s0, int sInt, int sEnd){
        /*
         Algorithm: We want to solve Aw = c, where c = (0,0,...,-1,0,...,0)^T, with the -1 in the sInt position, and w = (w0, w1, ..., wsInt, ... ,wn)^T. We want ws0. We also define
         
                [p00-1, p01, ..., 0, ...,p0n]
                [p10, p11-1, ..., 0, ...,p1n]
         A =    [p20, p21, ..., 0, ...,  p2n]
                            ...
                [0, 0, 0, ... , 0, ..., 0, 0]
                [p(sEnd+1)0, p(sEnd+1)1, ...]
                [pn0, pn1, ..., 0, ..., pnn-1]
         
         where the 0's are in the sEnd row and column
         */
        Eigen::MatrixXd A(numStates, numStates);
        Eigen::VectorXd c(numStates);
        Eigen::VectorXd w(numStates);
        //initialize c
        for(int i = 0; i < numStates; i++){
            if(i == sInt){
                c(i) = -1;
            }
            else{
                c(i) = 0;
            }
        }//end for
        
        //initialize A
        for(int i = 0; i< numStates; i++){
            if(i == sEnd){
                for(int j = 0; j< numStates; j++){
                    A(i,j) = 0;
                }
            }//end if
            else{
                for(int j = 0; j< numStates; j++){
                    (j== sEnd) ? A(i,j) = 0 : A(i,j) = _transition(i,j);
                    if(j == i){
                        A(i,j) -= 1.0;
                    }
                }
            }//else
        }//end outer for
        
        /*choose linear solver based on matrix size. Using info from Eigen docs at
         https://eigen.tuxfamily.org/dox/group__TutorialLinearAlgebra.html
         */
        if(numStates < 500){
            //for smaller matrices, rank-revealing Householder QR decomposition with column pivoting
            Eigen::ColPivHouseholderQR<Eigen::MatrixXd> CPH(A);
            w = CPH.solve(c);
        }
        else{
            //for big ones, use regular Householder QR
            Eigen::HouseholderQR<Eigen::MatrixXd> HQR(A);
            w = HQR.solve(c);
            
        }
        const double err_tol = 1.0e-4; //error tolerance on solutions
        double relative_error = (A*w - c).norm() / c.norm();
        if(relative_error < err_tol){
            return w(s0);
        }
        else{
            return -1; //return -1 if not in error bound
        }
        
        
    }//end function
    
    /**
     * @author: Zane Jakobs
     * @param t: time difference
     * @return: cov(X_s,X_{s+t}), taken from HMM for Time Series: an Intro Using R page 18
     */
    double cov(int t){
        const int expon = 15;
        Eigen::MatrixXd pi = limitingDistribution(expon);
        Eigen::MatrixXd V = Eigen::MatrixXd::Zero(numStates,numStates);
        Eigen::MatrixXd vectV(1,numStates);
        Eigen::MatrixXd cmat;
        for(int i = 0; i < numStates; i++){
            V(i,i) = i;
            vectV(0,i) = i;
        }
        if(t > 0){
            cmat = pi * V * (_transition.pow(t)) * (vectV.transpose());
        }
        else{
            cmat = pi * V* (vectV.transpose());
        }
        Eigen::MatrixXd dv = pi * (vectV.transpose());
        
        cmat -= dv*dv;
        //cov is a 1x1 from line 590 onwards;
        return cmat(0,0);
        
    }
    
    /**
     * @author: Zane Jakobs
     * @param t: time difference
     * @return: corr(X_s,X_{s+t}), taken from HMM for Time Series: an Intro Using R page 18
     */
    double corr(int t){
        double correlation = cov(t)/cov(0);
        return correlation;
    }
    
    double log_likelihood(vector<Sequence> df, int oversize = 100){
        Eigen::MatrixXd f = countMat(df, oversize);
        double ll = 0;
        for(int i = 0; i < numStates; i++){
            for(int j = 0; j < numStates; j++){
                ll += f(i,j) * log(_transition(i,j));
            }
        }
        return ll;
    }
    

};
    
}

#endif

