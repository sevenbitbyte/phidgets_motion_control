/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Phidgets based wheel odometry for a differential drive system
 *  For use with a pair of Phidgets high speed encoders
 *  See http://www.ros.org/wiki/navigation/Tutorials/RobotSetup/Odom
 *  Copyright (c) 2010, Bob Mottram
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

#include <stdio.h>
#include <math.h>
#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>
#include "phidgets_motion_control/encoder_params.h"

// used to prevent callbacks from accessing variables
// before they are initialised
bool initialised = false;

// index numbers of left and right encoders for the case
// where two or more encoders exist on the same Phidget device
int encoder_index_left=-1;
int encoder_index_right=-1;

// should we announce every encoder count that arrives?
bool verbose = false;

// normally on a differential drive system to when moving
//  forwards the wheels are rotating in opposite directions
int encoder_direction_left = -1;
int encoder_direction_right = 1;

// distance between the wheel centres
double wheelbase_mm = 400;

// encoder counts
int current_encoder_count_left = 0;
int current_encoder_count_right = 0;
int previous_encoder_count_left = 0;
int previous_encoder_count_right = 0;

// keep track if the initial encoder values so that relative
// movement can be reported
int start_encoder_count_left = 0;
int start_encoder_count_right = 0;

// encoder counts per millimetre
double left_encoder_counts_per_mm = 0;
double right_encoder_counts_per_mm = 0;

ros::Subscriber left_encoder_sub;
ros::Subscriber right_encoder_sub;
ros::Subscriber encoders_sub;

// pose estimate
double x = 0.0;
double y = 0.0;
double theta = 0.0;

// velocity estimate
double delta_x = 0.1;
double delta_y = -0.1;
double delta_theta = 0.1;

double rotation_offset=0;

// Update the left encoder count
void update_encoder_left(int count)
{
	current_encoder_count_left = count * encoder_direction_left;
	if (start_encoder_count_left == 0) {
		start_encoder_count_left = current_encoder_count_left;
	}
	if (verbose) {
		ROS_INFO("Left Encoder Count %d",
				current_encoder_count_left -
				start_encoder_count_left);
	}
}

// Update the right encoder count
void update_encoder_right(int count)
{
	current_encoder_count_right =
		count * encoder_direction_right;
	if (start_encoder_count_right == 0) {
		start_encoder_count_right = current_encoder_count_right;
	}
	if (verbose) {
		ROS_INFO("Right Encoder Count %d",
				current_encoder_count_right -
				start_encoder_count_right);
	}
}

/*!
 * \brief callback when the left or right encoder count changes
 * \param ptr encoder parameters
 */
void encoderCallback(const phidgets_motion_control::encoder_params::ConstPtr& ptr)
{
	if (initialised) {
		phidgets_motion_control::encoder_params e = *ptr;
		if (e.index == encoder_index_left) {
			update_encoder_left(e.count);
		}
		if (e.index == encoder_index_right) {
			update_encoder_right(e.count);
		}
	}
}

/*!
 * \brief callback when the left encoder count changes
 * \param ptr encoder parameters
 */
void leftEncoderCallback(const phidgets_motion_control::encoder_params::ConstPtr& ptr)
{
	if (initialised) {
		phidgets_motion_control::encoder_params e = *ptr;
		update_encoder_left(e.count);
	}
}

/*!
 * \brief callback when the right encoder count changes
 * \param ptr encoder parameters
 */
void rightEncoderCallback(const phidgets_motion_control::encoder_params::ConstPtr& ptr)
{
	if (initialised) {
		phidgets_motion_control::encoder_params e = *ptr;
		update_encoder_right(e.count);
	}
}

/*!
 * \param connects to a phidgets high speed encoder
 *        device which contains two or more encoders
 */

/* TODO we might not need the below function, leaving for reference for now
   bool subscribe_to_encoders_by_index()
   {
   bool success = true;
   ros::NodeHandle n;
   ros::NodeHandle nh("~");
   std::string topic_path = "phidgets/";
   nh.getParam("topic_path", topic_path);
   std::string encodername = "encoder";
   nh.getParam("encodername", encodername);
   std::string encoder_topic_base = topic_path + encodername;
   nh.getParam("encoder_topic", encoder_topic_base);
   int serial_number = -1;
   nh.getParam("serial", serial_number);
   if (serial_number==-1) {
   nh.getParam("serial_number", serial_number);
   }

// get the topic name
std::string encoder_topic = encoder_topic_base;
if (serial_number > -1) {
char serial_number_str[10];
sprintf(serial_number_str,"%d", serial_number);
encoder_topic += "/";
encoder_topic += serial_number_str;
}

encoders_sub =
n.subscribe(encoder_topic, 1, encoderCallback);

return(success);
}
 */

/*!
 * \param connects to the two phidgets high
 *        speed encoder devices
 */
bool subscribe_to_encoders()
{
	bool success = true;
	ros::NodeHandle n;
	ros::NodeHandle nh("~");
	std::string topic_path = "phidgets/";
	nh.getParam("topic_path", topic_path);
	std::string encodername = "encoder";
	nh.getParam("encodername", encodername);
	std::string encoder_topic_base = topic_path + encodername;
	nh.getParam("encoder_topic", encoder_topic_base);
	for (int enc = 0; enc < 2; enc++) {

		// get encoder phidget serial number
		std::string encoder_param_name = "serialleft";
		if (enc > 0) {
			encoder_param_name = "serialright";
		}
		int serial_number = -1;
		//TODO remove below line and parameterize or remove need of serial all together
		serial_number = 342515;
		nh.getParam(encoder_param_name, serial_number);
		if (serial_number == -1) {
			success = false;
			ROS_WARN("You must specify serial numbers " \
					"for the encoder Phidget devices");
			break;
		}

		// get the topic name
		std::string encoder_topic = encoder_topic_base;
		if (serial_number > -1) {
			char serial_number_str[10];
			sprintf(serial_number_str,"%d", serial_number);
			encoder_topic += "/";
			encoder_topic += serial_number_str;
		}

		// subscribe to the topic
		if (enc == 0) {
			ROS_INFO("Left encoder: %s", encoder_topic.c_str());
			left_encoder_sub =
				n.subscribe(encoder_topic, 1,
						leftEncoderCallback);
		}
		else {
			ROS_INFO("Right encoder: %s",
					encoder_topic.c_str());
			right_encoder_sub =
				n.subscribe(encoder_topic, 1,
						rightEncoderCallback);
		}
	}
	return(success);
}


int main(int argc, char** argv)
{

	ros::init(argc, argv, "phidgets_odometry");

	ros::NodeHandle n;
	ros::NodeHandle nh("~");

	std::string name = "odom";
	nh.getParam("name", name);

	std::string reset_topic = "odometry/reset";
	nh.getParam("reset_topic", reset_topic);

	nh.getParam("rotation", rotation_offset);

	n.setParam(reset_topic, false);

	// TODO paramaterize num to keep in buffer
	ros::Publisher odom_pub =
		n.advertise<nav_msgs::Odometry>(name, 50);
	tf::TransformBroadcaster odom_broadcaster;

	// get encoder indexes
	nh.getParam("encoderindexleft", encoder_index_left);
	nh.getParam("encoderindexright", encoder_index_right);

	nh.getParam("encoderdirectionleft", encoder_direction_left);
	nh.getParam("encoderdirectionright", encoder_direction_right);


	// connect to the encoders
	if (subscribe_to_encoders()) {

		// Setup nav and odom stuff
		std::string base_link = "base_link";
		nh.getParam("base_link", base_link);
		std::string frame_id = "odom";
		nh.getParam("frame_id", frame_id);
		nh.getParam("countspermmleft",
				left_encoder_counts_per_mm);
		nh.getParam("countspermmright",
				right_encoder_counts_per_mm);

		nh.getParam("wheelbase", wheelbase_mm);

		// Get verbosity level
		nh.getParam("verbose", verbose);

		int frequency = 100;
		nh.getParam("frequency", frequency);

		geometry_msgs::TransformStamped odom_trans;
		odom_trans.header.frame_id = frame_id;
		odom_trans.child_frame_id = base_link;

		ros::Time current_time, last_time;
		current_time = ros::Time::now();
		last_time = ros::Time::now();

		initialised = true;

		ros::Rate update_rate(frequency);
		while(ros::ok()){

			// Handle and update time
			current_time = ros::Time::now();
			//TODO remove test section below
			double dt = (current_time - last_time).toSec();
			double delta_x = ( 0.4 * cos(theta) - 0.0 * sin(theta) ) * dt;
			double delta_y = ( .4 * sin(theta) + 0.0 * cos(theta) ) * dt;
			double delta_th = 0.4 * dt;

			// reset the pose
			bool reset = false;
			n.getParam(reset_topic, reset);

			if (reset) {
				x = 0;
				y = 0;
				theta = 0;
				delta_x = 0;
				delta_y = 0;
				delta_theta = 0;
				n.setParam(reset_topic, false);
				start_encoder_count_left = 0;
				start_encoder_count_right = 0;
			}

			// update the velocity estimate based upon
			// TODO encoder values
			//update_velocities(dt);

			// compute odometry in a typical way given
			// the velocities of the robot
			x += delta_x;
			y += delta_y;
			theta += delta_theta;

			// get Quaternion from rpy
			geometry_msgs::Quaternion odom_quat =
				tf::createQuaternionMsgFromYaw(theta);

			// first, we'll publish the transform over tf
			odom_trans.header.stamp = current_time;

			odom_trans.transform.translation.x = x;
			odom_trans.transform.translation.y = y;
			odom_trans.transform.translation.z = 0.0;
			odom_trans.transform.rotation = odom_quat;

			// send the transform
			//odom_broadcaster.sendTransform(odom_trans);

			// publish the odometry
			nav_msgs::Odometry odom;
			odom.header.stamp = current_time;
			odom.header.frame_id = frame_id;

			// set position
			odom.pose.pose.position.x = x;
			odom.pose.pose.position.y = y;
			odom.pose.pose.position.z = 0.0;
			odom.pose.pose.orientation = odom_quat;

			// set the velocity
			odom.child_frame_id = base_link;
			odom.twist.twist.linear.x = delta_x;
			odom.twist.twist.linear.y = delta_y;
			odom.twist.twist.linear.z = 0.0;
			odom.twist.twist.angular.x = 0.0;	
			odom.twist.twist.angular.y = 0.0;
			odom.twist.twist.angular.z = delta_theta;

			// publish odom
			odom_pub.publish(odom);

			last_time = current_time;

			update_rate.sleep();
		}

		return 0;
	} else { 
		ROS_ERROR("Unable to subscribe to encoder topic" );
		return 1;
	}
	return 0;
}


