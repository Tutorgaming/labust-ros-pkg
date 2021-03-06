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
#ifndef ROSBAGFILTER_HPP_
#define ROSBAGFILTER_HPP_
#include <ros/ros.h>
#include <labust/tools/RosbagUtilities.hpp>

namespace labust {
  namespace tools {
    class RosbagFilter {
      public:
        RosbagFilter();
        void setOutputBag(const std::string& out_filename); 
        void setInputBags(const std::vector<std::string>& in_bags, const std::vector<std::string>& nspace, const std::vector<double>& delay);
        void setTopics(const std::vector<std::string>& topics);
        void setTime(const ros::Time& start_t, const ros::Time& end_t);
        void start();
      private:
        RosbagWriter bag_writer;
        ros::Time time_start, time_end;
        std::string out_bag_name_;
        std::vector<std::string> in_bags_;
        std::vector<std::string> nspace_;
        std::vector<std::string> topics_;
        std::vector<double> delay_;

    };
  }
}

/* ROSBAGFILTER_HPP_ */
#endif
