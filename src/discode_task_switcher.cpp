#include "ros/ros.h"
#include "std_srvs/Empty.h"

#include "Network/TCPClient.hpp"

#include <cstring>
#include <iostream>

#include <boost/algorithm/string.hpp>


class DisCODeTaskSwitcher {
public:
	bool init(const std::string & host = "localhost", const std::string & port = "30000") {
		m_client.setServiceHook(boost::bind(&DisCODeTaskSwitcher::service, this, _1, _2));
		m_client.setCompletionHook(boost::bind(&DisCODeTaskSwitcher::check, this, _1, _2));
		return m_client.connect(host, port);
	}

	bool sendMessage(const std::string & msg) {
		buf[0] = msg.size() / 256;
	        buf[1] = msg.size() % 256;

        	strcpy((char*)buf+2, msg.c_str());
	        buf[msg.size()+2] = 0;

	        m_client.send(buf, msg.size()+3);

        	// Sent. Waiting for reply...
	        m_client.recv();

		return true;
	}

	std::vector<std::string> listSubtasks() {
		std::vector<std::string> ret;
		sendMessage("listSubtasks");
		boost::split( ret, reply, boost::algorithm::is_any_of("\n"), boost::token_compress_on );
		return ret;
	}

	bool startSubtask(const std::string & subtask, std_srvs::Empty::Request &req, std_srvs::Empty::Response &res) {
		std::string msg = "startSubtask:" + subtask;
		sendMessage(msg);
		return true;
	}

	bool stopSubtask(const std::string & subtask, std_srvs::Empty::Request &req, std_srvs::Empty::Response &res) {
		std::string msg = "stopSubtask:" + subtask;
		sendMessage(msg);
		return true;
	}


private:
	Common::TCPClient m_client;
	unsigned char buf[256];
	std::string reply;

	int check(const unsigned char * msg, int size) {
		return size > 1 ? msg[0] * 256 + msg[1] : 2;
	}

	int service(const unsigned char * msg, int size) {
		reply = std::string((const char*)msg+2);
		return 0;
	}
};

struct ServiceSet {
	ros::ServiceServer srv_start;
	ros::ServiceServer srv_stop;
};

int main(int argc, char* argv[])
{
	ros::init(argc, argv, "discode_task_switcher");
	ros::NodeHandle n;

	
	ros::Duration(3).sleep(); // sleep for a while

	DisCODeTaskSwitcher sw;
	while (!sw.init()) {
		ROS_ERROR("Connecting to DisCODe failed. Waiting 3 seconds before retry...");
		ros::Duration(3).sleep(); // sleep for a while
	}

	std::map<std::string, ServiceSet> srvs;

	std::vector<std::string> subtasks = sw.listSubtasks();
	for (int i = 0; i < subtasks.size(); ++i) {
		ServiceSet s;
		if (subtasks[i] == "") continue;
		std::cout << i << ": " << subtasks[i] << std::endl;
		s.srv_start = n.advertiseService<std_srvs::Empty::Request, std_srvs::Empty::Response>(subtasks[i]+"/start", boost::bind(&DisCODeTaskSwitcher::startSubtask, &sw, subtasks[i], _1, _2));
		s.srv_stop = n.advertiseService<std_srvs::Empty::Request, std_srvs::Empty::Response>(subtasks[i]+"/stop", boost::bind(&DisCODeTaskSwitcher::stopSubtask, &sw, subtasks[i], _1, _2));
		srvs[subtasks[i]] = s;
	}

	ros::spin();

	return 0;
}
