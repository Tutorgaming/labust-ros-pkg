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
 *
 *  Author : Dula Nad
 *  Created: 26.03.2013.
 *********************************************************************/
#include <labust/navigation/KFCore.hpp>
#include <labust/navigation/XYModel.hpp>
#include <labust/math/uBlasOperations.hpp>
#include <labust/math/NumberManipulation.hpp>
#include <labust/tools/GeoUtilities.hpp>
#include <labust/tools/rosutils.hpp>

#include <kdl/frames.hpp>
#include <auv_msgs/NavSts.h>
#include <auv_msgs/BodyForceReq.h>
#include <sensor_msgs/NavSatFix.h>
#include <sensor_msgs/Imu.h>

#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>

#include <ros/ros.h>

#include <boost/bind.hpp>
#include <sstream>

typedef labust::navigation::KFCore<labust::navigation::XYModel> KFNav;
tf::TransformListener* listener;

ros::Time t;

void handleTau(KFNav::vector& tauIn, const auv_msgs::BodyForceReq::ConstPtr& tau)
{
	tauIn(KFNav::X) = tau->wrench.force.x;
	tauIn(KFNav::Y) = tau->wrench.force.y;
	tauIn(KFNav::N) = tau->wrench.torque.z;
};

void handleGPS(KFNav::vector& xy, const sensor_msgs::NavSatFix::ConstPtr& data)
{
	enum {x,y,newMsg};
	//Calculate to X-Y tangent plane
	tf::StampedTransform transformDeg, transformLocal;
	try
	{
		listener->lookupTransform("local", "gps_frame", ros::Time(0), transformLocal);
		listener->lookupTransform("/worldLatLon", "local", ros::Time(0), transformDeg);

		std::pair<double,double> posxy =
				labust::tools::deg2meter(data->latitude - transformDeg.getOrigin().y(),
						data->longitude - transformDeg.getOrigin().x(),
						transformDeg.getOrigin().y());

		//correct for lever arm shift during pitch and roll
		tf::Vector3 pos = tf::Vector3(posxy.first, posxy.second,0);

		//xy(x) = pos.x();
		//xy(y) = pos.y();
		xy(x) = posxy.first;
		xy(y) = posxy.second;
		xy(newMsg) = 1;
	}
	catch(tf::TransformException& ex)
	{
		ROS_ERROR("%s",ex.what());
	}
};

void handleImu(KFNav::vector& rpy, const sensor_msgs::Imu::ConstPtr& data)
{
	enum {r,p,y,newMsg};
	double roll,pitch,yaw;

	tf::StampedTransform transform;
	try
	{
		t = data->header.stamp;
		listener->lookupTransform("base_link", "imu_frame", ros::Time(0), transform);
		tf::Quaternion meas(data->orientation.x,data->orientation.y,
				data->orientation.z,data->orientation.w);
		tf::Quaternion result = meas*transform.getRotation();

		KDL::Rotation::Quaternion(result.x(),result.y(),result.z(),result.w()).GetEulerZYX(yaw,pitch,roll);
		/*labust::tools::eulerZYXFromQuaternion(
				Eigen::Quaternion<float>(result.x(),result.y(),
						result.z(),result.w()),
				roll,pitch,yaw);*/
		ROS_INFO("Received RPY:%f,%f,%f",roll,pitch,yaw);

		rpy(r) = roll;
		rpy(p) = pitch;
		rpy(y) = yaw;
		rpy[newMsg] = 1;
	}
	catch (tf::TransformException& ex)
	{
		ROS_ERROR("%s",ex.what());
	}
};

void configureNav(KFNav& nav, ros::NodeHandle& nh)
{
	ROS_INFO("Configure navigation.");

	KFNav::ModelParams surge,sway,yaw;

	//Get model parameters
	std::string modelName("default");
	nh.param("model_name",modelName,modelName);

	//Inertia and added mass
	double mass(1);
	nh.param(modelName+"/dynamics/mass",mass,mass);
	surge.alpha = mass;
	sway.alpha = mass;

	double Ts(0.1);
	nh.param(modelName+"/dynamics/period",Ts,Ts);
	nav.setTs(Ts);

	XmlRpc::XmlRpcValue modelParams;
	nh.getParam(modelName+"/dynamics/inertia_matrix", modelParams);
	ROS_ASSERT(modelParams.getType() == XmlRpc::XmlRpcValue::TypeArray);
	yaw.alpha = static_cast<double>(modelParams[8]);

	modelParams.clear();
	nh.getParam(modelName+"/dynamics/added_mass", modelParams);
	ROS_ASSERT(modelParams.getType() == XmlRpc::XmlRpcValue::TypeArray);
	surge.alpha += static_cast<double>(modelParams[0]);
	sway.alpha += static_cast<double>(modelParams[1]);
	yaw.alpha += static_cast<double>(modelParams[5]);

	//Linear damping
	modelParams.clear();
	nh.getParam(modelName+"/dynamics/damping", modelParams);
	ROS_ASSERT(modelParams.getType() == XmlRpc::XmlRpcValue::TypeArray);
	surge.beta = static_cast<double>(modelParams[0]);
	sway.beta = static_cast<double>(modelParams[1]);
	yaw.beta = static_cast<double>(modelParams[5]);

	//Quadratic damping
	modelParams.clear();
	nh.getParam(modelName+"/dynamics/quadratic_damping", modelParams);
	ROS_ASSERT(modelParams.getType() == XmlRpc::XmlRpcValue::TypeArray);
	surge.betaa = static_cast<double>(modelParams[0]);
	sway.betaa = static_cast<double>(modelParams[1]);
	yaw.betaa = static_cast<double>(modelParams[5]);

	nav.setParameters(surge,sway,yaw);

	std::string sQ,sW,sV,sR,sP,sx0;
	nh.getParam(modelName+"/navigation/Q", sQ);
	nh.getParam(modelName+"/navigation/W", sW);
	nh.getParam(modelName+"/navigation/V", sV);
	nh.getParam(modelName+"/navigation/R", sR);
	nh.getParam(modelName+"/navigation/P", sP);
	nh.getParam(modelName+"/navigation/x0", sx0);
	KFNav::matrix Q,W,V,R,P;
	KFNav::vector x0;
	boost::numeric::ublas::matrixFromString(sQ,Q);
	boost::numeric::ublas::matrixFromString(sW,W);
	boost::numeric::ublas::matrixFromString(sV,V);
	boost::numeric::ublas::matrixFromString(sR,R);
	boost::numeric::ublas::matrixFromString(sP,P);
	std::stringstream ss(sx0);
	boost::numeric::ublas::operator >>(ss,x0);

	std::cout<<P<<std::endl;

	nav.setStateParameters(W,Q);
	nav.setMeasurementParameters(V,R);
	nav.setStateCovariance(P);
	nav.initModel();
	nav.setState(x0);
}

//consider making 2 corrections based on arrived measurements (async)
int main(int argc, char* argv[])
{
	ros::init(argc,argv,"pladypos_nav");
	ros::NodeHandle nh;
	KFNav nav;
	//Configure the navigation
	configureNav(nav,nh);

	double outlierR;
	nh.param("outlier_radius",outlierR,1.0);

	//Publishers
	ros::Publisher stateHat = nh.advertise<auv_msgs::NavSts>("stateHat",1);
	ros::Publisher stateMeas = nh.advertise<auv_msgs::NavSts>("meas",1);
	ros::Publisher currentTwist = nh.advertise<geometry_msgs::TwistStamped>("currentsHat",1);
	ros::Publisher bodyFlowFrame = nh.advertise<geometry_msgs::TwistStamped>("body_flow_frame_twist",1);
	//Subscribers
	KFNav::vector tau(KFNav::zeros(KFNav::inputSize)),xy(KFNav::zeros(2+1)),rpy(KFNav::zeros(3+1));

	tf::TransformBroadcaster broadcast;
	listener = new tf::TransformListener();

	ros::Subscriber tauAch = nh.subscribe<auv_msgs::BodyForceReq>("tauAch", 1, boost::bind(&handleTau,boost::ref(tau),_1));
	ros::Subscriber navFix = nh.subscribe<sensor_msgs::NavSatFix>("gps", 1, boost::bind(&handleGPS,boost::ref(xy),_1));
	ros::Subscriber imu = nh.subscribe<sensor_msgs::Imu>("imu", 1, boost::bind(&handleImu,boost::ref(rpy),_1));

	ros::Rate rate(10);

	auv_msgs::NavSts meas,state;
	geometry_msgs::TwistStamped current, flowspeed;
	meas.header.frame_id = "local";
	state.header.frame_id = "local";
	current.header.frame_id = "local";

	labust::math::unwrap unwrap;

	while (ros::ok())
	{
		nav.predict(tau);

		if (rpy(3))
		{
			double yaw = unwrap(rpy(2));

			bool outlier = false;
			double x(nav.getState()(KFNav::xp)), y(nav.getState()(KFNav::yp));
			double inx(0),iny(0);
			nav.calculateXYInovationVariance(nav.getStateCovariance(),inx,iny);
			outlier = sqrt(pow(x-xy(0),2) + pow(y-xy(1),2)) > outlierR*sqrt(inx*inx + iny*iny);
			if (outlier)
			{ 
			   ROS_INFO("Outlier rejected: meas(%f, %f), estimate(%f,%f), inovationCov(%f,%f)",xy(0),xy(1),x,y,inx,iny);	
			   xy(2) = 0;
			}

			if (xy(2) == 1 && !outlier)
			{
			   ROS_INFO("XY correction: meas(%f, %f), estimate(%f,%f), inovationCov(%f,%f,%d)",xy(0),xy(1),x,y,inx,iny,nav.getInovationCovariance().size1());
			   nav.correct(nav.fullUpdate(xy(0),xy(1),yaw));
			   xy(2) = 0;
			}
			else
			{
				ROS_INFO("Heading correction:%f",yaw);
				nav.correct(nav.yawUpdate(yaw));
			}
			rpy(3) = 0;
		}

		meas.orientation.roll = rpy(0);
		meas.orientation.pitch = rpy(1);
		meas.orientation.yaw = rpy(2);
		meas.position.north = xy(0);
		meas.position.east = xy(1);
		stateMeas.publish(meas);

		const KFNav::vector& estimate = nav.getState();
		state.body_velocity.x = estimate(KFNav::u);
		state.body_velocity.y = estimate(KFNav::v);
		state.orientation_rate.yaw = estimate(KFNav::r);
		state.position.north = estimate(KFNav::xp);
		state.position.east = estimate(KFNav::yp);
		current.twist.linear.x = estimate(KFNav::xc);
		current.twist.linear.y = estimate(KFNav::yc);

		const KFNav::matrix& covariance = nav.getStateCovariance();
		state.position_variance.north = covariance(KFNav::xp, KFNav::xp);
		state.position_variance.east = covariance(KFNav::yp, KFNav::yp);
		state.orientation_variance.yaw =  covariance(KFNav::psi, KFNav::psi);

		try
		{
			tf::StampedTransform transformDeg;
			listener->lookupTransform("/worldLatLon", "local", ros::Time(0), transformDeg);

			std::pair<double, double> diffAngle = labust::tools::meter2deg(state.position.north,
					state.position.east,
					//The latitude angle
					transformDeg.getOrigin().y());
			state.global_position.latitude = transformDeg.getOrigin().y() + diffAngle.first;
			state.global_position.longitude = transformDeg.getOrigin().x() + diffAngle.second;
		}
		catch(tf::TransformException& ex)
		{
			ROS_ERROR("%s",ex.what());
		}

		state.orientation.yaw = labust::math::wrapRad(estimate(KFNav::psi));

		tf::StampedTransform transform;
		transform.setOrigin(tf::Vector3(estimate(KFNav::xp), estimate(KFNav::yp), 0.0));
		Eigen::Quaternion<float> q;
		labust::tools::quaternionFromEulerZYX(rpy(0),rpy(1),estimate(KFNav::psi),q);
		transform.setRotation(tf::Quaternion(q.x(),q.y(),q.z(),q.w()));
		broadcast.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "local", "base_link"));

		//Calculate the flow frame (instead of heading use course)
		double xdot,ydot;
		nav.getNEDSpeed(xdot,ydot);
		labust::tools::quaternionFromEulerZYX(rpy(0),rpy(1),atan2(ydot,xdot),q);
		transform.setRotation(tf::Quaternion(q.x(),q.y(),q.z(),q.w()));
		broadcast.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "local", "base_link_flow"));

		flowspeed.twist.linear.x = xdot;
		flowspeed.twist.linear.y = ydot;
		bodyFlowFrame.publish(flowspeed);
		currentTwist.publish(current);
		state.header.stamp = t;
		stateHat.publish(state);

		rate.sleep();
		ros::spinOnce();
	}

	return 0;
}
