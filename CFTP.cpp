#ifndef EIGEN_USE_MKL_ALL
#define EIGEN_USE_MKL_ALL
#endif
#include "CFTP.hpp"
#include "Eigen/Core"
#include "MatrixFunctions.hpp"
#include<deque>
#include<valarray>
#include<random>
#include<algorithm>
using namespace std;
using namespace Eigen;
//has the chain coalesced? If so, returns sample; else, returns -1



Eigen::MatrixXd matPow(Eigen::MatrixXd &mat, int _pow){
    Eigen::MatrixXd limmat;
    limmat = mat.pow(_pow);
    return limmat;
}

//matrix power, for threading
//void *threadedMatPow(Eigen::MatrixXd &mat, int _pow){
  //  mat = mat.pow(_pow);
//}
//variation distance between two distributions
double variation_distance(Eigen::MatrixXd dist1, Eigen::MatrixXd dist2){
    int n = dist1.cols();
    int m = dist2.cols();
    int nr = dist1.rows();
    int mr = dist2.rows();
    
    if(n != m || nr != mr){
        return -1;
    }
    if(n == 1){
        dist1 = dist1.transpose();
    }
    if(m == 1){
        dist2 = dist2.transpose();
    }
    double sum = 0;
    int k;
    (n == 1) ? k  = nr : k = n; //length of distribution.
    for(int i = 0; i < k; i++){
        sum += std::abs(dist1(0,i) - dist2(0,i));
    }
    sum /= 2;
    return sum;
}

double k_stationary_variation_distance(Eigen::MatrixXd trans, int k){
    const int matexp = 15;
    Eigen::MatrixXd pi = (matPow(trans,matexp)).row(0);
    double distance;
    double max = 0;
    int cols = trans.cols();
    if(k > 1){
        Eigen::MatrixXd mat = matPow(trans, k);
    }
    for(int i = 0; i < cols; i++){
            distance = variation_distance(trans.row(i),pi);
            if(distance > max){
                max = distance;
            }
    }
    return max;
}

int mixing_time(Eigen::MatrixXd &trans){
    const double tol = 0.36787944117144; // 1/e to double precision
    const int max_mix_time = 100; //after this time, we abandon our efforts, as it takes too long to mix
    for(int i = 1; i < max_mix_time; i++){
        if(k_stationary_variation_distance(trans, i) < tol){
            return i;
        }
    }
    return -1; //in case of failure
}

int isCoalesced(Eigen::MatrixXd &mat){
    int ncols = mat.rows();
    int sample  = mat(0,0);
    for(int i = 1; i < ncols; i++){
        if(mat(i,0) != sample){
            return -1;
        }
    }
    return sample;
}
//taken from MarkovChain.cpp
int random_transition(Eigen::MatrixXd &mat, int init_state, double r){
    double s = mat(init_state,0);
    int i = 0;
    while(r > s && (i < mat.cols()-1)){
        i++;
        s += mat(init_state,i);
    }
    
    return i;
}

/**
 * @author: Zane Jakobs
 * @summary: voter CFTP algorithm to perfectly sample from the Markov chain with transition matrix mat. Algorithm from https://pdfs.semanticscholar.org/ef02/fd2d2b4d0a914eba5e4270be4161bcae8f81.pdf
 * @return: perfect sample from matrix's distribution
 */
int voter_CFTP(Eigen::MatrixXd &mat){
    int nStates = mat.cols();
    std::deque<double> R; //random samples
    
    Eigen::MatrixXd M(nStates,1);
    bool coalesced = false;
    for(int i = 0; i < nStates; i++){
        M(i,0) = i;
    }
    //set random seed
    random_device rd;
    //init Mersenne Twistor
    mt19937 gen(rd());
    // unif(0,1)
    uniform_real_distribution<> dis(0.0,1.0);
    while(not coalesced){
        
        
        double r = dis(gen);
        
        R.push_front(r); // R(T) ~ U(0,1)
        //T -= 1, starting at T = 0 above while loop;
        M.conservativeResize(M.rows(), M.cols()+1);
        //move every element one to the right
        for(int i = M.cols()-1; i > 0; i--){
            Eigen::MatrixXd temp = M.col(i-1);
            M.col(i) = temp;
               
        }
        //reinitialize first column
        for(int i = 0; i < nStates; i++){
            int randState = random_transition(mat, i,r);
            M(i,0) = M(randState,1);
        }
        int sample = isCoalesced(M);
        if(sample != -1){
            coalesced = true;
            return sample;
        }
    }
    return -1;
}

/**
 *@author: Zane Jakobs
 * @param trans: transition matrix
 * @param gen, dis: random number generator stuff
 * @param R: deque to hold random numbers
 * @param M: matrix to represent voter CFTP process
 * @param nStates: how many states?
 * @param coalesced: has the chain coalesced?
 * @return : distribution
 */
int iteratedVoterCFTP( std::mt19937 &gen, std::uniform_real_distribution<> &dis, Eigen::MatrixXd &mat, std::deque<double> &R, Eigen::MatrixXd &M, Eigen::MatrixXd &temp, int &nStates, bool coalesced = false){
        //resize M
        M.resize(nStates,1);
        //clear R
        R.clear();
    

        for(int i = 0; i < nStates; i++){
            M(i,0) = i;
        }
        while(not coalesced){
            
            double r = dis(gen);
            
            R.push_front(r); // R(T) ~ U(0,1)
            //T -= 1, starting at T = 0 above while loop;
            M.conservativeResize(M.rows(), M.cols()+1);
            //move every element one to the right
            for(int i = M.cols()-1; i > 0; i--){
                temp = M.col(i-1);
                M.col(i) = temp;
                
            }
            //reinitialize first column
            int randState;
            for(int i = 0; i < nStates; i++){
                randState = random_transition(mat, i,r);
                M(i,0) = M(randState,1);
            }
            int sample = isCoalesced(M);
            if(sample != -1){
                coalesced = true;
                return sample;
            }
        }
        return -1;
}


/**
 * @author: Zane Jakobs
 * @param mat: matrix to sample from
 * @param n: how many samples
 * @return: vector where i-th entry is the number of times state i appeared
 */
std::valarray<int> sampleVoterCFTP(Eigen::MatrixXd &mat, int n){
    int cls = mat.cols();
    //set random seed
    random_device rd;
    //init Mersenne Twistor
    mt19937 gen(rd());
    // unif(0,1)
    uniform_real_distribution<> dis(0.0,1.0);
    
    valarray<int> arr(cls);
    int sample;
    std::deque<double> R;
    Eigen::MatrixXd M;
    Eigen::MatrixXd temp;
    for(int i = 0; i< n; i++){
        sample = iteratedVoterCFTP( gen, dis, mat, R, M, temp, cls);
        arr[sample]++;
    }
    return arr;
}

/**
 * @author: Zane Jakobs
 * @param mat: matrix to sample from
 * @param n: how many samples
 * @return: VectorXd where i-th entry is the density of state i
 */
Eigen::VectorXd voterCFTPDistribution(Eigen::MatrixXd &mat, int n){
    std::valarray<int> counts = sampleVoterCFTP(mat, n);
    Eigen::VectorXd res(mat.cols());
    double sum = double(counts.sum());
    for(int i = 0; i < mat.cols(); i++){
        res(i) = counts[i]/sum;
    }
    return res;
}


