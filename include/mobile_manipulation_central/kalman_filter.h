#pragma once

#include <Eigen/Eigen>

namespace mm {

struct GaussianEstimate {
    Eigen::VectorXd x;
    Eigen::MatrixXd P;
};

// Prediction step: predict next state given a priori model and input
GaussianEstimate kf_predict(const GaussianEstimate& e, const Eigen::MatrixXd& A,
                            const Eigen::MatrixXd& Q,
                            const Eigen::VectorXd& v) {
    GaussianEstimate prediction;
    prediction.x = A * e.x + v;
    prediction.P = A * e.P * A.transpose() + Q;
    return prediction;
}

// Correction step: fuse measured output with predicted state
GaussianEstimate kf_correct(const GaussianEstimate& e, const Eigen::MatrixXd& C,
                            const Eigen::MatrixXd& R,
                            const Eigen::VectorXd& y) {
    // Innovation covariance
    // Use Cholesky decomposition instead of computing the matrix inverse
    // in Kalman gain directly
    Eigen::MatrixXd CP = C * e.P;
    Eigen::MatrixXd S = CP * C.transpose() + R;
    Eigen::LLT<Eigen::MatrixXd> LLT = S.llt();

    // Correct using measurement model
    GaussianEstimate correction;
    correction.P = e.P - CP.transpose() * LLT.solve(CP);
    correction.x = e.x + CP.transpose() * LLT.solve(y - C * e.x);
    return correction;
}

// Compute normalized innovation squared, which can be used for chi-squared
// hypothesis testing (i.e. reject bad measurements)
double kf_nis(const GaussianEstimate& e, const Eigen::MatrixXd& C,
              const Eigen::MatrixXd& R, const Eigen::VectorXd& y) {
    // Innovation
    Eigen::VectorXd z = y - C * e.x;

    // Innovation covariance
    Eigen::MatrixXd S = C * e.P * C.transpose() + R;
    Eigen::LLT<Eigen::MatrixXd> LLT = S.llt();

    return z.dot(LLT.solve(z));
}

}  // namespace mm
