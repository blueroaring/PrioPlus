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
#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dc-topology.h"
#include "ns3/dcb-trace-application-helper.h"
#include "ns3/dcb-trace-application.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/global-value.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <boost/json.hpp>

namespace ns3
{

namespace json_util
{

boost::json::object ReadConfig(std::string config_file);

void SetDefault(boost::json::object defaultObj);

void PrettyPrint(std::ostream& os, const boost::json::value& jv, std::string* indent = nullptr);

void SetRandomSeed(boost::json::object& configJsonObj);

void SetRandomSeed(uint32_t seed);

void InstallApplications(const boost::json::object& conf, Ptr<DcTopology> topology);

} // namespace json_util

} // namespace ns3

#endif /* JSON_UTIL_H */
