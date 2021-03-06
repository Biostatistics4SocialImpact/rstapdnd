#include <cmath>
#include <cstdlib>
#include <Eigen/Core>
#include <Rcpp.h>
#include <RcppEigen.h>
#include <random>
#include <vector>
#ifndef M_PI
#define M_PI REAL(3.1415926535897932384626433832795029)
#endif

double GaussianNoise_scalar(std::mt19937 &rng){
    static std::normal_distribution<double> _z(0,1);
    return(_z(rng));
}

Eigen::VectorXd GaussianNoise(const int q, std::mt19937 &rng){
    Eigen::VectorXd out(q);
    for(int i=0;i<q;i++){
        out(i) = GaussianNoise_scalar(rng);
    }
    return(out);
}

void print_progress(const int &iter_ix, const int &warm_up, const int &iter_max, const int &chain){

  if(iter_max > 20){
      if((iter_ix) % (int)round(.1 * iter_max) == 0 || iter_ix == 1 || iter_ix == (warm_up + 1) ){
          int progress = (int)round(iter_ix * 100 / iter_max);
          std::string str = (iter_ix) <= (warm_up) ? "\t [Warmup]" : "\t [Sampling]";
          Rcpp::Rcout << "[Chain " << chain << "] Beginning of iteration: " << (iter_ix) << " / " << iter_max << " (" << progress << "%)" << str  << std::endl;
      }
  }
  else{
          int progress = (int)round(iter_ix * 100 / iter_max);
          std::string str = (iter_ix) <= (warm_up) ? "\t [Warmup]" : "\t [Sampling]";
          Rcpp::Rcout << "[Chain " << chain << "] Beginning of iteration: " << (iter_ix) << " / " << iter_max << " (" << progress << "%)" << str  << std::endl;
  }

}
