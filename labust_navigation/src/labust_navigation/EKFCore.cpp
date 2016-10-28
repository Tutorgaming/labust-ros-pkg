/*
 * EKFCore.cpp
 *
 *  Created on: Sep 1, 2016
 *      Author: filip
 */

#include <labust/navigation/EKFCore.h>

using namespace labust::navigation;

/**
 * Perform the prediction step based on the system input.
 *
 * \param u Input vector.
 */
template<class Model>
void EKFCore<Model>::predict(typename Model::inputref u =
		typename Model::input_type()) {
	assert((this->Ts) && "KFCore: Sampling time set to zero.");
	/***
	 * Forward the model in time and update the state vector x_:
	 * x_(k) = f(x(k-1),u(k),0)
	 ***/
	this->processModelStep(x_, u);
	xk1_ = x_;
	/***
	 * Update the innovation matrix:
	 * P_(k) = A(k)*P(k-1)*A'(k) + W(k)*Q(k-1)*W'(k)
	 ***/
	this->derivativeAWX(x_, u, A, W);
	this->P = this->A * this->P * this->A.transpose() +
	//this can be optimized for constant noise models
			this->W * this->Q * this->W.transpose();
}

template<class Model>
const typename Model::output_type& EKFCore<Model>::update(
		typename Model::vectorref measurements,
		typename Model::vectorref newMeas) {
	std::vector<size_t> arrived;
	std::vector<double> dataVec;

	for (size_t i = 0; i < newMeas.size(); ++i) {
		if (newMeas(i)) {
			arrived.push_back(i);
			dataVec.push_back(measurements(i));
			newMeas(i) = 0;
		}
	}

	this->measurementModelStep(x_, y_);
	this->derivativeHX(x_, Hnl);


	measurement.resize(arrived.size());
	H = matrix::Zero(arrived.size(), stateNum);
	y = vector::Zero(arrived.size());
	R = matrix::Zero(arrived.size(), arrived.size());
	V = matrix::Zero(arrived.size(), arrived.size());

	for (size_t i = 0; i < arrived.size(); ++i) {
		measurement(i) = dataVec[i];

		H.row(i) = Hnl.row(arrived[i]);
		y(i) = ynl(arrived[i]);

		for (size_t j = 0; j < arrived.size(); ++j) {
			R(i, j) = R0(arrived[i], arrived[j]);
			V(i, j) = V0(arrived[i], arrived[j]);
		}
	}

	return measurement;
}

/**
 * State estimate correction based on the available measurements.
 * Note that the user has to handle the measurement processing
 * and populate the y_meas vector.
 *
 * \return Corrected state estimate.
 */
template<class Model>
typename Model::vectorref EKFCore<Model>::correct(
		typename Model::outputref y_meas) {
	/***
	 * Calculate the Kalman gain matrix
	 *
	 * K(k) = P_(k)*H(k)'*inv(H(k)*P(k)*H(k)' + V(k)*R(k)*V(k)');
	 */
	typename Model::matrix PH = this->P * this->H.transpose();
	this->innovationCov = this->H * PH;
	typename Model::matrix VR = this->V * this->R;
	//This can be optimized for constant R, V combinations
	this->innovationCov += VR * this->V.transpose();
	//this->K = PH*this->innovationCov.inverse();
	this->Kgain = PH
			* this->innovationCov.ldlt().solve(
					Model::matrix::Identity(this->innovationCov.rows(),
							this->innovationCov.cols()));
	/*
	 * Correct the state estimate
	 *
	 * x(k) = x_(k) + K(k)*(measurement - y(k));
	 */
	typename Model::output_type y;
	this->estimate_y(y);
	this->innovation = y_meas - y;
	this->x_ += this->Kgain * this->innovation;
	/**
	 * Update the estimate covariance matrix.
	 * P(k) = (I - K(k)*H(k))*P(k);
	 */
	typename Model::matrix IKH = Model::matrix::Identity(this->P.rows(),
			this->P.cols());
	IKH -= this->Kgain * this->H;
	this->P = IKH * this->P;

	return this->xk1_ = this->x_;
}

/**
 * Update the matrices V and H to accommodate for available measurements.
 * Perform per element outlier rejection and correct the state with the
 * remaining validated measurements.
 *
 * \return Corrected state estimate
 */
template<class Model, class NewMeasVector>
typename Model::vectorref EKFCore<Model>::correct(
		typename Model::outputref measurements,
		typename Model::vectorref newMeas, bool reject_outliers = true) {
	//Check invariant.
	assert(
			measurements.size() == Model::stateNum
					&& newMeas.size() == Model::stateNum);

	std::vector<size_t> arrived;
	std::vector<typename Model::numericprecission> dataVec;

	for (size_t i = 0; i < newMeas.size(); ++i) {
		//Outlier rejection
		if (newMeas(i) && reject_outliers) {
			double dist = fabs(this->x(i) - measurements(i));
			newMeas(i) = (dist <= sqrt(this->P(i, i)) + sqrt(this->R0(i, i)));

			///\todo Remove this after debugging
			if (!newMeas(i)) {
				std::cerr << "Outlier rejected: x(i)=" << this->x(i);
				std::cerr << ", m(i)=" << measurements(i);
				std::cerr << ", r(i)="
						<< sqrt(this->P(i, i)) + sqrt(this->R0(i, i));
			}
		}

		if (newMeas(i)) {
			arrived.push_back(i);
			dataVec.push_back(measurements(i));
			newMeas(i) = 0;
		}
	}

	typename Model::output_type y_meas(arrived.size());
	this->H = Model::matrix::Zero(arrived.size(), Model::stateNum);
	this->V = Model::matrix::Zero(arrived.size(), Model::stateNum);

	for (size_t i = 0; i < arrived.size(); ++i) {
		y_meas(i) = dataVec[i];
		this->H.row(i) = this->H0.row(arrived[i]);
		this->V.row(i) = this->V0.row(arrived[i]);
	}

	return this->correct(y_meas);
}
