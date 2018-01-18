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
#include <string>
#include <iostream>
#include <vector>
#include <errno.h>

#include "amqp.h"
#include "threading/MsgThread.h"
#include "threading/formatters/JSON.h"
#include "threading/formatters/Ascii.h"
#include "threading/SerialTypes.h"

using namespace logging;
using namespace writer;
using namespace std;

using threading::Value;
using threading::Field;

amqp::amqp(WriterFrontend* frontend) : WriterBackend(frontend) 
{
    json = new threading::formatter::JSON(this, threading::formatter::JSON::TS_EPOCH);
}

amqp::~amqp() 
{
    if (json) 
    {
        delete json;
    }

    DestroyAMQP();

}

std::string amqp::GetTableType(int arg_type, int arg_subtype) 
{
    string type;

    switch (arg_type) 
    {
        case TYPE_BOOL:
            type = "boolean";
            break;

        case TYPE_INT:
        case TYPE_COUNT:
        case TYPE_COUNTER:
        case TYPE_PORT:
            type = "integer";
            break;

        case TYPE_SUBNET:
        case TYPE_ADDR:
            type = "text";
            break;

        case TYPE_TIME:
        case TYPE_INTERVAL:
        case TYPE_DOUBLE:
            type = "double precision";
            break;

        case TYPE_ENUM:
        case TYPE_STRING:
        case TYPE_FILE:
        case TYPE_FUNC:
            type = "text";
            break;

        case TYPE_TABLE:
        case TYPE_VECTOR:
            type = "text";
            break;

        default:
            type = Fmt("%d", arg_type);
    }

    return type;
}

// returns true true in case of error
bool amqp::checkError(int code) 
{
    return false;
}

bool amqp::Init(void)
{
    if(connstr.empty() || exchange_name.empty() || queue_name.empty()) 
    {
        return false;
    }

    try 
    {
        amqp_conn = new AMQP(connstr);
        exchange = amqp_conn->createExchange(exchange_name);

        //TODO: add the ability to modify exchange and queue options
        /*******************
        ** Modify this block in order to change exchange and queue options
        *******************/
        exchange->Declare(exchange_name, "direct", AMQP_DURABLE);

        queue = amqp_conn->createQueue(queue_name);
        queue->Declare(queue_name, AMQP_DURABLE);
        queue->Bind(exchange_name, queue_name);

        // Force the disabling of the immediate flag. Yes.. it is misspelled
        // by the authors.
        exchange->setParam(~AMQP_IMMIDIATE);

        exchange->setHeader("Delivery-mode", 2);
        exchange->setHeader("Content-type", "application/json");
        exchange->setHeader("Content-encoding", "UTF-8");

        return true;
    } 
    catch (AMQPException e) 
    {
        MsgThread::Info(Fmt("PS_amqp - Init - AMQPException: %s", e.getMessage().c_str()));
        ReInit();
    } 
    catch (const std::exception &exc) 
    {
        MsgThread::Info(Fmt("PS_amqp - Init - std::exception: %s ", exc.what()));
        ReInit();
    } 
    catch(...) 
    {
        MsgThread::Info("PS_amqp - Init - Exception found");
        ReInit();
    }

    return false;
}

/* Gracefully teardown and delete AQMP */
void amqp::DestroyAMQP(void)
{
    if (queue)
        delete queue;
    
    if (exchange)
        delete exchange;
    
    if (amqp_conn)
        delete amqp_conn;
}

bool amqp::ReInit(void)
{
    if(connstr.empty() || exchange_name.empty() || queue_name.empty()) 
    {
        return false;
    }

    try 
    {
        MsgThread::Info("PS_amqp - Attempt to reinitialize");
        DestroyAMQP();
        sleep(AMQP_RETRY_INTERVAL);
        Init();
    } 
    catch(...) 
    {
        MsgThread::Info("PS_amqp - Exception found");
    }

    return true;
}

bool amqp::DoInit(const WriterInfo& info, int arg_num_fields,
		const Field* const * arg_fields)
{
    WriterInfo::config_map::const_iterator it;
    num_fields = arg_num_fields;
    fields = arg_fields;

    try 
    {
        it = info.config.find("connstr");
        if (it == info.config.end()) 
        {
            MsgThread::Info(Fmt("connstr configuration option not found"));
            return false;
        } 
        else 
        {
            connstr = it->second;
        }

        it = info.config.find("exchange");
        if (it == info.config.end()) 
        {
            MsgThread::Info(Fmt("exchange configuration option not found"));
            return false;
        } 
        else 
        {
            exchange_name = it->second;
        }

        it = info.config.find("queue");
        if (it == info.config.end()) 
        {
            MsgThread::Info(Fmt("queue configuration option not found"));
            return false;
        } 
        else 
        {
            queue_name = it->second;
        }

        info_path = info.path;

        return Init();
    } 
    catch (...) 
    {
        MsgThread::Info("PS_amqp - DoInit - Exception found");
    }

    return false;
}

bool amqp::odesc_to_string_writer(const ODesc &buffer, bool add_log_path) 
{
    string out_buf = string(reinterpret_cast<const char*>(buffer.Bytes()));
    out_buf = out_buf.insert(1, path);

    if (add_log_path) 
    {
        out_buf = out_buf.insert(1, Fmt("\"log\": \"%s\",", info_path.c_str()));
    }

    try 
    {
        if (!out_buf.empty() && !queue_name.empty()) 
        {
            exchange->Publish(out_buf, queue_name);
        }
    } 
    catch (AMQPException e) 
    {
        MsgThread::Info(Fmt("PS_amqp - odesc_to_string_writer(\"%s\") - AMQPException: %s", 
                            out_buf.c_str(), e.getMessage().c_str()));
        ReInit();
    } 
    catch (...) 
    {
        MsgThread::Info("PS_amqp - odesc_to_string_writer - Exception found");
    }

    return true;
}

bool amqp::DoWrite(int num_fields, const Field* const * fields, Value** vals) 
{
    try 
    {
        bool add_log_path = true;

        ODesc buffer;

        for ( int j = 0; j < num_fields; j++ ) 
        {
            const threading::Field* field = fields[j];
            if ( strncasecmp(field->name, "log", 3) == 0 ) 
            {
                add_log_path = false;
                break;
            }
        }

        json->Describe(&buffer, num_fields, fields, vals);
        odesc_to_string_writer(buffer, add_log_path);

    } 
    catch (...) 
    {
        MsgThread::Info("PS_amqp - DoWrite - Exception found");
    }
    return true;
}

bool amqp::DoRotate(const char* rotated_path, double open, double close,
		bool terminating) 
{
    if (!FinishedRotation("/dev/null", Info().path, open, close, terminating)) 
    {
        Error(Fmt("error rotating %s", Info().path));
        return false;
    }

    return true;
}
