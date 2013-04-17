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
 *  Created: 23.01.2013.
 *********************************************************************/
#include <labust/tools/rosutils.hpp>

#include <std_msgs/String.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/NavSatFix.h>
#include <tf/transform_broadcaster.h>
#include <ros/ros.h>

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include <iostream>

struct SharedData
{
	enum {msg_size = 120,
		data_offset=1,
		checksum = 119};
	ros::Publisher imuPub, gpsPub;
	tf::Transform imuPos, gpsPos;
	tf::TransformBroadcaster broadcast;
	char buffer[30*4];
};

void start_receive(SharedData& shared,
		boost::asio::serial_port& port);
void handleIncoming(SharedData& shared,
		boost::asio::serial_port& port,
		const boost::system::error_code& error, const size_t& transferred);

void sync(SharedData& shared,
		boost::asio::serial_port& port,
		const boost::system::error_code& error, const size_t& transferred)
{
	if (!error)
	{
		if (shared.buffer[0] == 0xFF)
		{
			boost::asio::async_read(port, boost::asio::buffer(&shared.buffer[SharedData::data_offset],
					SharedData::msg_size-SharedData::data_offset),
						boost::bind(&handleIncoming,
								boost::ref(shared),
								boost::ref(port),_1,_2));
		}
		else
		{
			start_receive(shared,port);
		}
	}
}

void handleIncoming(SharedData& shared,
		boost::asio::serial_port& port,
		const boost::system::error_code& error, const size_t& transferred)
{
	if (!error && (transferred == SharedData::msg_size))
	{
    char calc = 0;
    for (size_t i=0; i<SharedData::msg_size-1; ++i){calc^=shared.buffer[i];};

    if (calc != shared.buffer[SharedData::checksum])
    {
    	ROS_ERROR("Wrong checksum for imu data.");
    	return;
    }

    float* data(reinterpret_cast<float*>(shared.buffer[SharedData::data_offset]));
  	enum {time = 0,
  		lat, lon, hdop,
  		accel_x, accel_y, accel_z,
  		gyro_x, gyro_y, gyro_z,
  		mag_x, mag_y, mag_z,
  	  roll,pitch,yaw};

    //Send Imu stuff
  	sensor_msgs::Imu::Ptr imu(new sensor_msgs::Imu());
  	imu->header.stamp = ros::Time::now();
  	imu->header.frame_id = "imu_frame";
  	imu->linear_acceleration.x = data[accel_x];
  	imu->linear_acceleration.y = data[accel_y];
  	imu->linear_acceleration.z = data[accel_z];
  	imu->angular_velocity.x = data[gyro_x];
  	imu->angular_velocity.y = data[gyro_y];
  	imu->angular_velocity.z = data[gyro_z];
  	tf::Quaternion quat = tf::createQuaternionFromRPY(data[roll],data[pitch],data[yaw]);
  	imu->orientation.x = quat.x();
  	imu->orientation.y = quat.y();
  	imu->orientation.z = quat.z();
  	imu->orientation.w = quat.w();
  	shared.broadcast.sendTransform(tf::StampedTransform(shared.imuPos, ros::Time::now(), "base_link", "imu_frame"));

  	//Send GPS stuff
  	sensor_msgs::NavSatFix::Ptr gps(new sensor_msgs::NavSatFix());

  	gps->latitude = data[lat];
  	gps->longitude = data[lon];
  	gps->position_covariance[0] = data[hdop];
  	gps->position_covariance[4] = data[hdop];
  	gps->position_covariance[9] = 9999;
  	gps->header.frame_id = "world";
  	gps->header.stamp = ros::Time::now();

  	shared.broadcast.sendTransform(tf::StampedTransform(shared.gpsPos, ros::Time::now(), "base_link", "gps_frame"));
	}
	start_receive(shared,port);
}

/*void start_receive(SharedData& shared,
		boost::asio::streambuf& sbuffer,
		boost::asio::serial_port& port)
{
	boost::asio::async_read_until(port, sbuffer, boost::regex("\n"),
				boost::bind(&handleIncoming,
						boost::ref(shared),
						boost::ref(sbuffer),
						boost::ref(port),_1,_2));
}*/

void start_receive(SharedData& shared,
		boost::asio::serial_port& port)
{
	boost::asio::async_read(port, boost::asio::buffer(shared.buffer,1),
					boost::bind(&sync,
							boost::ref(shared),
							boost::ref(port),_1,_2));
}

int main(int argc, char* argv[])
{
	ros::init(argc,argv,"imu_node");
	ros::NodeHandle nh,ph("~");

	std::string portName("/dev/ttyUSB0");
	int baud(115200);

	ph.param("PortName",portName,portName);
	ph.param("Baud",baud,baud);

	namespace ba=boost::asio;
	ba::io_service io;
	ba::serial_port port(io);

	port.open(portName);
	port.set_option(ba::serial_port::baud_rate(baud));
	port.set_option(ba::serial_port::flow_control(
			ba::serial_port::flow_control::none));

	if (!port.is_open())
	{
		std::cerr<<"Cannot open port."<<std::endl;
		exit(-1);
	}

	SharedData shared;
	shared.imuPub = nh.advertise<sensor_msgs::Imu>("imu",1);
	shared.gpsPub = nh.advertise<sensor_msgs::NavSatFix>("fix",1);

	//Configure Imu and GPS position relative to vehicle center of mass
	Eigen::Vector3d origin(Eigen::Vector3d::Zero()),
			orientation(Eigen::Vector3d::Zero());

	labust::tools::getMatrixParam(nh, "imu_origin", origin);
	labust::tools::getMatrixParam(nh, "imu_orientation", orientation);
	shared.imuPos.setOrigin(tf::Vector3(origin(0),origin(1),origin(2)));
	shared.imuPos.setRotation(tf::createQuaternionFromRPY(orientation(0),
			orientation(1),orientation(2)));

	origin = origin.Zero();
	orientation = orientation.Zero();
	labust::tools::getMatrixParam(nh, "gps_origin", origin);
	labust::tools::getMatrixParam(nh, "gps_orientation", orientation);
	shared.gpsPos.setOrigin(tf::Vector3(origin(0),origin(1),origin(2)));
	shared.gpsPos.setRotation(tf::createQuaternionFromRPY(orientation(0),
			orientation(1),orientation(2)));

	//boost::asio::streambuf sbuffer;
	//start_receive(shared,sbuffer,port);
	start_receive(shared,port);
	boost::thread t(boost::bind(&ba::io_service::run,&io));

	ros::spin();
	io.stop();
	t.join();

	return 0;
}

