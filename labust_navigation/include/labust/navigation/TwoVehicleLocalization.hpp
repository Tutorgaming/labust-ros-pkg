/*********************************************************************
 * EKF_3D_USBL.cpp
 *
 *  Created on: Mar 26, 2013
 *      Author: Dula Nad
 *      
 *  Modified on: Mar 3, 2015
 *  	 Author: Filip Mandic
 *
 ********************************************************************/
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, LABUST, UNIZG-FER
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the LABUST nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/
#ifndef TWOVEHICLELOCALIZATION_HPP_
#define TWOVEHICLELOCALIZATION_HPP_
#include <labust/navigation/KFCore.hpp>
#include <labust/navigation/TwoVehicleLocalizationModel.hpp>
#include <labust/navigation/SensorHandlers.hpp>
#include <labust/math/NumberManipulation.hpp>
#include <navcon_msgs/ModelParamsUpdate.h>
#include <labust/tools/OutlierRejection.hpp>


#include <tf2_ros/transform_broadcaster.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Bool.h>
#include <auv_msgs/BodyForceReq.h>
#include <underwater_msgs/USBLFix.h>
#include <underwater_msgs/SonarFix.h>
#include <navcon_msgs/RelativePosition.h>


#include <auv_msgs/NED.h>
#include <std_msgs/Bool.h>

#include <stack>
#include <deque>


namespace labust
{
	namespace navigation
	{
		/**
	 	 * The 3D state estimator for ROVs and AUVs. Extended with altitude and pitch estimates.
	 	 *
	 	 * \todo Extract the lat/lon part into the llnode.
	 	 */
		class Estimator3D
		{
			enum{X=0,Y,Z,K,M,N, DoF};
			typedef labust::navigation::KFCore<labust::navigation::TwoVehicleLocalizationModel> KFNav;
		public:
			/**
			 * Main constructor.
			 */
			Estimator3D();

			/**
			 * Initialize the estimation filter.
			 */
			void onInit();
			/**
			 * Start the estimation loop.
			 */
			void start();

			struct FilterState{

				FilterState(){}

				~FilterState(){}

				KFNav::vector input;
				KFNav::vector meas;
				KFNav::vector newMeas;
				KFNav::vector state;
				KFNav::matrix Pcov;
				KFNav::matrix Rcov;
			};

		private:
			/**
			 * Helper function for navigation configuration.
			 */
			void configureNav(KFNav& nav, ros::NodeHandle& nh);
			/**
			 * Handle the local stateHat measurement.
			 */
			void onLocalStateHat(const auv_msgs::NavSts::ConstPtr& data);
			/**
			 * Handle the second vehicle position measurement.
			 */
			void onSecond_navsts(const auv_msgs::NavSts::ConstPtr& data);
			/**
			 * Handle the USBL measurement.
			 */
			void onSecond_usbl_fix(const underwater_msgs::USBLFix::ConstPtr& data);
			/**
			 * Handle the sonar measurement.
			 */
			void onSecond_sonar_fix(const navcon_msgs::RelativePosition::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onSecond_camera_fix(const navcon_msgs::RelativePosition::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onUSBLbearningOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onUSBLrangeOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onCameraBearningOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onCameraRangeOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onSonarBearningOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onSonarRangeOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onDepthOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onCameraHeadingOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Handle the camera measurement.
			 */
			void onDivernetHeadingOffset(const std_msgs::Float32::ConstPtr& data);
			/**
			 * Helper method to process measurements.
			 */
			void processMeasurements();
			/**
			 * Helper method to publish the navigation state.
			 */
			void publishState();
			/**
			 * Handle the state reset.
			 */
			void onReset(const std_msgs::Bool::ConstPtr& reset);
			/**
			 * Calculate covariance matrix condition number
			 */
			void calculateConditionNumber();
			/**
			 * Calculate measurement delay in time steps
			 */
			int calculateDelaySteps(double measTime, double arrivalTime);
			/**
			 * The navigation filter.
			 */
			KFNav nav;
			/**
			 * The input vector.
			 */
			KFNav::vector tauIn;
			/**
			 * The measurements vector and arrived flag vector.
			 */
			KFNav::vector measurements, newMeas, measDelay;
			/**
			 * Heading unwrapper.
			 */
			labust::math::unwrap unwrap;
			/**
			 * Sonar bearing unwrapper.
			 */
			labust::math::unwrap course_unwrap;
			labust::math::unwrap bearing_unwrap;
			labust::math::unwrap camera_bearing_unwrap;
			labust::math::unwrap sonar_bearing_unwrap;

			labust::math::unwrap hdgb_unwrap;
			labust::math::unwrap hdgb_camera_unwrap;


			/**
			 * Estimated and measured state publisher.
			 */
			ros::Publisher pubLocalStateHat, pubSecondStateHat, pubLocalStateMeas, pubSecondStateMeas, pubSecondRelativePosition;
			ros::Publisher pubRange, pubBearing, pubRangeFiltered, pubwk;
			ros::Publisher pubCondP, pubCondPxy, pubCost;
			ros::Publisher pub_usbl_range, pub_usbl_bearing, pub_sonar_range, pub_sonar_bearing, pub_camera_range, pub_camera_bearing;
			ros::Publisher pub_diver_course, pub_usbl_relative_bearing;


			/**
			 * Sensors and input subscribers.
			 */
			ros::Subscriber subLocalStateHat, resetTopic, subSecond_navsts;
			ros::Subscriber subSecond_heading, subSecond_position, subSecond_speed, subSecond_usbl_fix, subSecond_sonar_fix, subSecond_camera_fix;
			ros::Subscriber sub_usbl_bearing_offset, sub_usbl_range_offset, sub_camera_range_offset, sub_sonar_range_offset, sub_camera_bearing_offset, sub_sonar_bearing_offset;
			ros::Subscriber sub_depth_offset, sub_camera_heading_offset, sub_divernet_heading_offset;
			/**
			 * The transform broadcaster.
			 */
			tf2_ros::TransformBroadcaster broadcaster;
			/**
			 * Model parameters
			 */
			KFNav::ModelParams params[DoF];
			/**
			 * Model parameters
			 */
			ros::Time measurement_timeout;
			/**
			 * The DVL model selector.
			 */
			int dvl_model;
			/**
			 *  Current time in seconds
			 */
			double currentTime;
			/**
			 *  Fixed time delay for USBL navigation
			 */
			double delay_time;
			/**
			 *  USBL measurements enable flags
			 */
			bool enableDelay, enableRange, enableBearing, enableElevation, enableRejection, alternate_outlier, enable_camera_heading;

			KFNav::matrix Pstart, Rstart;

			double sonar_offset, usbl_offset, cov_limit, usbl_bearing_offset, depth_offset, meas_timeout_limit, camera_offset, camera_bearing_offset, sonar_bearing_offset;
			double diver_camera_heading_offset, divernet_heading_offset;
			std::deque<FilterState> pastStates;

			double range_estimate, bearing_estimate;

			labust::tools::OutlierRejection OR, OR_b;

			Eigen::Matrix2d P_rng_bear_relative;

            int display_counter;

		};
	}
}
/* EKF3D_HPP_ */
#endif
