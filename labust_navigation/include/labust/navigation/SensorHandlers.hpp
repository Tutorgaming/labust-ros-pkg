/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010, LABUST, UNIZG-FER
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
#ifndef SENSORHANDLERS_HPP_
#define SENSORHANDLERS_HPP_
#include <labust/math/NumberManipulation.hpp>

#include <sensor_msgs/NavSatFix.h>
#include <sensor_msgs/Imu.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float64.h>
#include <geometry_msgs/TwistStamped.h>
#include <underwater_msgs/USBLFix.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>

#include <GeographicLib/Geocentric.hpp>
#include <GeographicLib/LocalCartesian.hpp>

#include <labust/diagnostics/StatusHandler.h>

#include <ros/ros.h>

#include <Eigen/Dense>

namespace labust
{
	namespace navigation
	{

		struct NewArrived
		{
			NewArrived():isNew(false){};

			inline bool newArrived()
			{
				if (isNew)
				{
					isNew=false;
					return true;
				}
				return false;
			}

		protected:
			bool isNew;
		};
		/**
		 * The GPS handler.
		 */
		class GPSHandler : public NewArrived
		{
		public:
			GPSHandler():listener(buffer),originh(0),tf_prefix(""),status_handler_("GPS","gps"){};
			void configure(ros::NodeHandle& nh);

			inline const std::pair<double, double>&
			position() const {return posxy;}

			inline const std::pair<double, double>&
			origin() const {return originLL;}

			inline const double origin_h() const {return originh;}

			inline const std::pair<double, double>&
			latlon() const	{return posLL;}
			
			void setRotation(const Eigen::Quaternion<double>& quat){this->rot = quat;};

		private:
			void onGps(const sensor_msgs::NavSatFix::ConstPtr& data);
			std::pair<double, double> posxy, originLL, posLL;
			tf2_ros::Buffer buffer;
			tf2_ros::TransformListener listener;
			ros::Subscriber gps;
			//The ENU frame
			GeographicLib::LocalCartesian proj;
			Eigen::Quaternion<double> rot;
			double originh;
			std::string tf_prefix;
			labust::diagnostic::StatusHandler status_handler_;
		};

		/**
		 * The imu handler.
		 */
		class ImuHandler : public NewArrived
		{
		public:
			enum {roll=0, pitch, yaw};
			enum {p=0,q,r};
			enum {ax,ay,az};

			ImuHandler():listener(buffer),gps(0),magdec(0),tf_prefix(""),status_handler_("IMU","imu"){};
			
			void setGpsHandler(GPSHandler* gps){this->gps = gps;};

			void configure(ros::NodeHandle& nh);

			inline const double* orientation() const{return rpy;}

			inline const double* rate() const{return pqr;}

			inline const double* acc() const{return axyz;}

		private:
			void onImu(const sensor_msgs::Imu::ConstPtr& data);
			void onMagDec(const std_msgs::Float64::ConstPtr& data){magdec = data->data;}
			tf2_ros::Buffer buffer;
			tf2_ros::TransformListener listener;
			labust::math::unwrap unwrap;
			ros::Subscriber imu, mag_dec;
			double rpy[3], pqr[3], axyz[3], magdec;
			GPSHandler* gps;
			std::string tf_prefix;
			labust::diagnostic::StatusHandler status_handler_;

		};

		class DvlHandler : public NewArrived
		{
		public:
			enum {u=0,v,w};

			DvlHandler():r(0),listener(buffer),bottom_lock(false),tf_prefix(""),status_handler_("DVL","dvl"){};

			void configure(ros::NodeHandle& nh);

			inline const double* body_speeds() const {return uvw;};

			inline void current_r(double yaw_rate) {r = yaw_rate;};

			inline bool has_bottom_lock() const {return bottom_lock;};

		private:
			void onDvl(const geometry_msgs::TwistStamped::ConstPtr& data);
			void onBottomLock(const std_msgs::Bool::ConstPtr& data);
			double uvw[3], r;
			bool bottom_lock;
			ros::Subscriber nu_dvl, dvl_bottom;
			tf2_ros::Buffer buffer;
			tf2_ros::TransformListener listener;
			std::string tf_prefix;
			labust::diagnostic::StatusHandler status_handler_;
		};

		class iUSBLHandler : public NewArrived
		{
		public:
			enum {x=0,y,z};

			iUSBLHandler():depth(0),listener(buffer),tf_prefix(""),fix_arrived(false),status_handler_("iUSBL","iusbl"),
					remote_arrived(false){};

			void configure(ros::NodeHandle& nh);

			inline const double* position() const {return pos;};

		private:
			void onUSBL(const underwater_msgs::USBLFix::ConstPtr& data);
			void onSurfacePos(const auv_msgs::NavSts::ConstPtr& data);

			inline void current_z(double z) { depth = z;};

			void merge();

			double pos[3], depth;

			ros::Subscriber usbl_sub, remote_pos_sub;
			tf2_ros::Buffer buffer;
			tf2_ros::TransformListener listener;
			std::string tf_prefix;
			auv_msgs::NavSts remote_position;
			underwater_msgs::USBLFix fix;
			bool fix_arrived;
			bool remote_arrived;
			labust::diagnostic::StatusHandler status_handler_;
		};
	}
}
/* SENSORHANDLERS_HPP_ */
#endif
