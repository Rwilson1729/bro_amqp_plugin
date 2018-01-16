/*
	This file is part of bro_amqp_plugin.

	Copyright (c) 2015, Packetsled. All rights reserved.

	tcplog is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	tcplog is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with tcplog.  If not, see <http://www.gnu.org/licenses/>.

 */
// Aaron Eppert - PacketSled - 2015
#ifndef PS_AMQP_MESSAGE_BUS_HPP
#define PS_AMQP_MESSAGE_BUS_HPP

#include <string>
#include "threading/SerialTypes.h"
#include "AMQPcpp.h"

using namespace std;

namespace plugin {
	namespace PS_amqp {
		class message_bus_publisher {
			public:
				message_bus_publisher(std::string connStr, std::string _exchange, std::string _queue);
				~message_bus_publisher();
				void initialize();
				void publish(std::string msg);
			private:
				AMQP amqp;
				AMQPExchange *ex;
				AMQPQueue * qu2;
				std::string exchange;
				std::string queue;
		};
	}
}
#endif // PS_AMQP_MESSAGE_BUS_HPP
