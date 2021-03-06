STAP::STAP(Eigen::ArrayXXd &input_dists,
           Eigen::ArrayXXi &input_ucrs,
           Eigen::ArrayXXd &input_times,
	   Eigen::ArrayXXi &input_utcrs,
           Eigen::MappedSparseMatrix<double> &input_subj_array,
           Eigen::MatrixXd &input_subj_n,
           Eigen::MatrixXd &input_Z,
           Eigen::VectorXd &input_y,
           const bool& input_diagnostics){

    dists = input_dists;
    u_crs = input_ucrs;
    times = input_times;
    u_tcrs= input_utcrs;
    subj_array = input_subj_array;
    subj_n = input_subj_n;
    Z = input_Z;
    y = input_y;
    X = Eigen::MatrixXd::Zero(y.size(),dists.rows()); 
    X_prime = Eigen::MatrixXd::Zero(y.size(),dists.rows());
	X_tprime = Eigen::MatrixXd::Zero(y.size(),dists.rows());
    diagnostics = input_diagnostics;

}

double STAP::calculate_ll(SV& sv){

    double out = 0;
    calculate_X_diff(sv.theta(0),sv.theta_t(0));
    this->calculate_eta(sv);

    out +=  -y.size() * sv.sigma ; 

    // likelihood kernel
    out += - .5 * sv.precision_transformed() * (y-eta).dot(y-eta);

    return(out);

}

void STAP::calculate_eta(SV& sv){

    eta = sv.get_alpha_vector() + X_diff * sv.beta + X_mean * sv.beta_bar + Z * sv.delta;

}

double STAP::calculate_total_energy(SV& sv){
            
     if(diagnostics){
        Rcpp::Rcout << " Energy Calculation \n " << "------------------" << std::endl;
        sv.print_pars();
    }
    
    double out = 0;
	out += calculate_ll(sv);

    if(diagnostics)
        Rcpp::Rcout << "likelihood" << out << std::endl;
    
    // alpha ~N(25,5)  prior
    out += R::dnorm(sv.alpha,25,5,TRUE);
  
    // delta ~ N(0,3)
    if(sv.spc(1) != 0)
        out += - sv.delta.size() / 2.0 * log(M_PI * 2 * 9) - .5 * 1.0 / 9.0  * sv.delta.dot(sv.delta); 

    // beta ~ N(0,3) prior
    if(sv.spc(1) != 0)
        out += - sv.beta.size() / 2.0 * log(M_PI * 2 * 9) - .5 * 1.0 / 9.0  * sv.beta.dot(sv.beta); 

    // beta_bar ~ N(0,3) prior
    if(sv.spc(3) != 0)
        out += - sv.beta_bar.size() / 2.0 * log(M_PI * 2 * 9) - .5 * 1.0 / 9.0  * sv.beta_bar.dot(sv.beta_bar); 

    if(diagnostics)
        Rcpp::Rcout << "bb prior " << out << std::endl;

    // log(theta) ~ N(0,1) prior 
    out +=  R::dlnorm(sv.theta_transformed()(0),1,1,TRUE);
    if(diagnostics)
        Rcpp::Rcout << "theta prior " << out << std::endl;

    out += R::dlnorm(sv.theta_t_transformed()(0),0,1,TRUE) + sv.theta_t(0) ;

    // sigma ~ C(0,5)
    out +=  R::dcauchy(sv.sigma_transformed(),0,5,TRUE);
    if(diagnostics)
        Rcpp::Rcout << "sigma prior " << out << std::endl;

    // theta constraints jacobian adjustment 
    out += log(10) - log(1 + exp(-sv.theta(0))) + log(1.0 - 1.0 / (1 + exp(-sv.theta(0))));

    if(diagnostics)
        Rcpp::Rcout << "jacobian I" << out << std::endl;
    
    // sigma jacobian
    out += sv.sigma;

    if(diagnostics){
        Rcpp::Rcout << "jacobian II" << out << std::endl;
        Rcpp::Rcout << "------------------ \n " << std::endl;
    }

    // Incorporate Kinetic Energy
    out -= sv.kinetic_energy();
    if(diagnostics)
        Rcpp::Rcout << "Kinetic Energy " << out << std::endl;

    out = (isinf(-out) || isnan(out)) ? (-1 * DBL_MAX) : out;

    if(diagnostics)
        Rcpp::Rcout << "Energy out " << out << std::endl;
   
    return(out);

}

double STAP::sample_u(SV& sv, std::mt19937& rng){

    if(diagnostics)
        Rcpp::Rcout << "Sample U Energy Calculation" << std::endl;
    double energy = this->calculate_total_energy(sv);
    std::uniform_real_distribution<double> runif(0.0,1.0);
    double log_z = log(runif(rng));
    return(energy + log_z);
}
/*
void STAP::calculate_X(double& theta){
    
    int start_col;
    int range_len;
    int col_lim = X.cols();
    for(int bef_ix = 0; bef_ix < col_lim; bef_ix ++){
        for(int subj_ix = 0; subj_ix < u_crs.rows(); subj_ix ++){
            start_col = u_crs(subj_ix,bef_ix);
            range_len = u_crs(subj_ix,bef_ix+1) - start_col + 1;
            if(range_len==0)
                X(subj_ix,bef_ix) = 0;
            else
                X(subj_ix,bef_ix) = (exp(- dists.block(bef_ix,start_col,1,range_len) / theta  )).sum();
        }
    }
}
*/

void STAP::calculate_X(double &theta_s, double &theta_t){

    int start_col;
    int range_len;
    int col_lim = X.cols();
    for(int bef_ix = 0; bef_ix < col_lim; bef_ix ++){
        for(int subj_ix = 0; subj_ix < u_crs.rows(); subj_ix ++){
            start_col = u_crs(subj_ix,bef_ix);
            range_len = u_crs(subj_ix,bef_ix+1) - start_col + 1;
            if(range_len==0)
                X(subj_ix,bef_ix) = 0;
            else
                X(subj_ix,bef_ix) = ( exp(- dists.block(bef_ix,start_col,1,range_len) / theta_s  ) * 
                    (1 - exp( -times.block(bef_ix,start_col,1,range_len) / theta_t )) ).sum();
        }
    }

}
/*
void STAP::calculate_X_diff(double& theta){
    
  // calculate exposure matrix
    double transformed_theta = 10.0 / (1 + exp(-theta));
    this->calculate_X(transformed_theta);
    this->calculate_X_mean();
    X_diff = X - X_mean;

    // calculate exposure column means
    X_global_mean = (X_mean).colwise().mean();

    // center average exposure matrix by column
    X_mean = (X_mean).rowwise() - X_global_mean.transpose();
}*/

void STAP::calculate_X_diff(double& theta,double &theta_t){
    
  // calculate exposure matrix
    double transformed_theta = 10.0 / (1 + exp(-theta));
    double transformed_theta_t = exp(theta_t);
    this->calculate_X(transformed_theta,transformed_theta_t);
    this->calculate_X_mean();
    X_diff = X - X_mean;

    // calculate exposure column means
    X_global_mean = (X_mean).colwise().mean();

    // center average exposure matrix by column
    X_mean = (X_mean).rowwise() - X_global_mean.transpose();
}

void STAP::calculate_X_mean(){

    X_mean =  (subj_array.transpose() * ((subj_array * X).array() * subj_n.array() ).matrix() );
}

/*
void STAP::calculate_X_prime(double& theta_tilde,double& theta){ 

    int start_col;
    int range_len;
    int col_lim = X.cols();
    for(int bef_ix = 0; bef_ix < col_lim ; bef_ix ++){
        for(int subj_ix = 0; subj_ix < u_crs.rows(); subj_ix ++){
            start_col = u_crs(subj_ix,bef_ix);
            range_len = u_crs(subj_ix,bef_ix+1) - start_col + 1;
            if(range_len==0){
              X(subj_ix,bef_ix) = 0;
              X_prime(subj_ix,bef_ix) = 0;
            }
            else{
              X(subj_ix,bef_ix) = (exp(- dists.block(bef_ix,start_col,1,range_len) / theta_tilde  )).sum();
              X_prime(subj_ix,bef_ix) = (( 10.0 / (1+exp(-theta))) * (1 - 1.0 /(1.0 + exp(-theta)) ) ) *
                                        ( pow(theta_tilde,- 2  )) *  (dists.block(bef_ix,start_col,1,range_len) * 
                                            exp(- dists.block(bef_ix,start_col,1,range_len) / theta_tilde  ) ).sum();
            }
        }
    }

}*/

void STAP::calculate_X_prime(double& theta_tilde,double& theta,double &theta_t){ 

    int start_col;
    int range_len;
    int col_lim = X.cols();
    for(int bef_ix = 0; bef_ix < col_lim ; bef_ix ++){
        for(int subj_ix = 0; subj_ix < u_crs.rows(); subj_ix ++){
            start_col = u_crs(subj_ix,bef_ix);
            range_len = u_crs(subj_ix,bef_ix+1) - start_col + 1;
            if(range_len==0){
              X(subj_ix,bef_ix) = 0;
              X_prime(subj_ix,bef_ix) = 0;
			  X_tprime(subj_ix,bef_ix) = 0;
            }
            else{
              X(subj_ix,bef_ix) = (exp(- dists.block(bef_ix,start_col,1,range_len) / theta_tilde  )  *
                                  (1- exp(-times.block(bef_ix,start_col,1,range_len) / theta_t ) )   ).sum();
              X_prime(subj_ix,bef_ix) =  sigmoid_transform_derivative(theta,0.0,10.0) * pow(theta_tilde,- 2  ) *
                                            (dists.block(bef_ix,start_col,1,range_len) * 
                                            exp(- dists.block(bef_ix,start_col,1,range_len) / theta_tilde  ) *
											(1- exp( - times.block(bef_ix,start_col,1,range_len) / theta_t ) ) ).sum();
			  X_tprime(subj_ix,bef_ix) = - (times.block(bef_ix,start_col,1,range_len) * 
										   exp(-times.block(bef_ix,start_col,1,range_len) / theta_t - 
											   dists.block(bef_ix,start_col,1,range_len) / theta_tilde) ).sum() / theta_t;
			}
        }
    }

}

void STAP::calculate_X_mean_prime(){ 

    X_mean =  (subj_array.transpose() * ( (subj_array * X).array() * subj_n.array() ).matrix() );
    X_mean_prime = (subj_array.transpose() * ((subj_array * X_prime).array() * subj_n.array()  ).matrix() );
	X_mean_tprime = (subj_array.transpose() * ((subj_array * X_tprime).array() * subj_n.array()  ).matrix() );

}

/*
void STAP::calculate_X_prime_diff(double& theta_tilde, double& theta){

    // calculate exposure matrices, derivative exposure matrices
    this->calculate_X_prime(theta_tilde,theta);
    this->calculate_X_mean_prime();
    // calculate differences
    X_prime_diff = (X_prime - X_mean_prime);
    X_diff = (X - X_mean);

    // calculate exposure column means
    X_global_mean = (X_mean).colwise().mean();
    X_mean_prime_global_mean = (X_mean_prime).colwise().mean();

    // center exposure matrices (by column) 
    X_mean = (X_mean).rowwise() - X_global_mean.transpose();
    X_mean_prime = (X_mean_prime).rowwise() - X_mean_prime_global_mean.transpose();
}*/

void STAP::calculate_X_prime_diff(double& theta_tilde, double &theta, double &theta_t){

    calculate_X_prime(theta_tilde,theta,theta_t);
    calculate_X_mean_prime();

    X_prime_diff = (X_prime - X_mean_prime);
	X_tprime_diff = X_tprime - X_mean_tprime;
    X_diff = (X- X_mean);

    X_global_mean = X_mean.colwise().mean();
    X_mean_prime_global_mean = (X_mean_prime).colwise().mean();
	X_mean_tprime_global_mean = X_mean_tprime.colwise().mean();

    X_mean = X_mean.rowwise() - X_global_mean.transpose();
    X_mean_prime = X_mean_prime.rowwise() - X_mean_prime_global_mean.transpose();
	X_mean_tprime = X_mean_tprime.rowwise() - X_mean_tprime_global_mean.transpose();
}


void STAP::calculate_gradient(SV& sv){

    double theta = sv.theta(0);
    double theta_transformed = 10 / (1 + exp(- theta));
    double theta_t = exp(sv.theta_t(0));
    double precision = sv.precision_transformed();
    this->calculate_X_prime_diff(theta_transformed,theta,theta_t); // also calculates X
    this->calculate_eta(sv);

    sg.delta_grad =  precision * ((y - eta ).transpose() * Z).transpose();

    sg.alpha_grad = sv.spc(0) == 0 ? 0 : precision * (y - eta ).sum();

    sg.beta_grad = (precision * ( y - eta).transpose() * X_diff).transpose();

    sg.beta_bar_grad = precision * ((y - eta).transpose() * X_mean ).transpose();

    sg.sigma_grad = precision * (pow((y - eta ).array(),2) ).sum() - y.size();

    sg.theta_grad = precision * ((y - eta).transpose() * (X_prime_diff * sv.beta + X_mean_prime * sv.beta_bar) ).transpose();

    sg.theta_t_grad = precision * ((y-eta).transpose() * (X_tprime_diff * sv.beta + X_mean_tprime * sv.beta_bar) ).transpose();

    // prior/jacobian components
    sg.alpha_grad += -1.0 / 25 * sv.alpha ; 
    sg.delta_grad = sg.delta_grad - 1.0 / 9.0 * sv.delta;
    sg.beta_grad = sg.beta_grad - 1.0 / 9.0 * sv.beta;
    sg.beta_bar_grad = sg.beta_bar_grad - 1.0 / 9.0 * sv.beta_bar;
    sg.theta_grad(0) += - pow(theta_transformed,-1) * sigmoid_transform_derivative(sv.theta(0),0,10);
    sg.theta_grad(0) += - (log(theta_transformed)-1) * pow(theta_transformed,-1) * sigmoid_transform_derivative(sv.theta(0),0,10);
    sg.theta_grad(0)  += log_sigmoid_transform_derivative(sv.theta(0),0,10); 
    sg.theta_t_grad(0) += - (sv.theta_t(0));
    sg.sigma_grad += - (2 * sv.sigma_transformed()) / (25 + sv.sigma_sq_transformed()) + 1;

    if(sv.spc(1) == 0 )
        sg.delta_grad = Eigen::VectorXd::Zero(1);
    if(sv.spc(2) == 0)
        sg.beta_grad = Eigen::VectorXd::Zero(1);
    if(sv.spc(3) == 0)
        sg.beta_bar_grad = Eigen::VectorXd::Zero(1);
}

double STAP::FindReasonableEpsilon(SV& sv, std::mt19937& rng){

    if(diagnostics)
        Rcpp::Rcout << "Find Reasonable Epsilon Start \n " << std::endl;
    double epsilon = 1.0;
    int a;
    SV sv_prop(sv.spc,rng,diagnostics);
    double ratio,initial_energy,propose_energy;
    initial_energy = this->calculate_total_energy(sv);
    this->calculate_gradient(sv);
    sv_prop.momenta_leapfrog_other(sv,epsilon,sg);
    sv_prop.momenta_leapfrog_position(sv,epsilon);
    this->calculate_gradient(sv_prop);
    sv_prop.momenta_leapfrog_self(epsilon,sg);
    propose_energy = this->calculate_total_energy(sv_prop);
    ratio =  propose_energy - initial_energy;
    if(diagnostics)
        Rcpp::Rcout << "ratio calc" << propose_energy << " " << initial_energy << " " << propose_energy - initial_energy << std::endl;
    ratio = isinf(-ratio) ? -DBL_MAX: ratio ;
    a = ratio > log(.5) ? 1 : -1;
    if(diagnostics){
        Rcpp::Rcout << "a: " << a << std::endl;
        Rcpp::Rcout << "ratio: " << ratio << std::endl;
        Rcpp::Rcout << "a * ratio: " << a * ratio << std::endl;
        Rcpp::Rcout << "-a * log(2): " << -a * log(2)  << std::endl;
    }
    int cntr = 0;
    while ( a * ratio > -a * log(2)){
        epsilon = pow(2,a) * epsilon;
        if(diagnostics)
            Rcpp::Rcout << "epsilon for loop in Find Reasonable Epsilon" << epsilon << std::endl;
        this->calculate_gradient(sv);
        sv_prop.momenta_leapfrog_other(sv,epsilon,sg);
        sv_prop.momenta_leapfrog_position(sv,epsilon);
        this->calculate_gradient(sv_prop);
        sv_prop.momenta_leapfrog_self(epsilon,sg);
        propose_energy = this->calculate_total_energy(sv_prop);
        ratio =  propose_energy - initial_energy;
        cntr ++;
        if(cntr > 50)
            break;
    }
    
    if(diagnostics)
        Rcpp::Rcout << "Find Reasonable Epsilon End with epsilon =  " <<  epsilon << "\n \n \n " << std::endl;
    return(epsilon);
}
