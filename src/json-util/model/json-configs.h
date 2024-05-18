/*
 * Copyright (c) 2023
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Zhaochen Zhang (zhaochen.zhang@outlook.com)
 */
#ifndef JSON_CONFIGS_H
#define JSON_CONFIGS_H

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dc-topology.h"
#include "ns3/dcb-traffic-gen-application-helper.h"
#include "ns3/dcb-traffic-gen-application.h"
#include "ns3/error-model.h"
#include "ns3/flow-identifier.h"
#include "ns3/global-route-manager.h"
#include "ns3/global-value.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/real-time-application.h"
#include "ns3/traffic-control-module.h"

#include <boost/json.hpp>
#include <set>

namespace ns3
{

namespace json_util
{

boost::json::object ReadConfig(std::string config_file);

void SetDefault(boost::json::object& defaultObj);

/**
 * \brief Automatically set the global value according to the global object.
 *
 * In this function, global value is allocated on heap without being freed intentionally.
 * As ns3 expected, the global should be static variable to keep it alive during the whole
 * simulation. To make the global values automatically set, we allocate them on heap and
 * never free them. Please take care of this and never overuse this function.
 *
 * In addition, if you want to use MTP, you should better only use global value to set config and
 * never rewrite it.
 */
void SetGlobal(boost::json::object& globalObj);

void SetRandomSeed(boost::json::object& configJsonObj);

void SetRandomSeed(uint32_t seed);

void SetRuntime(boost::json::object& configJsonObj);

void SetStopTime(boost::json::object& configJsonObj);

} // namespace json_util

} // namespace ns3

#endif /* JSON_CONFIGS_H */
