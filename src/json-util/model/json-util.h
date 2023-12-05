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

void SetDefault(boost::json::object& defaultObj);

/**
 * \brief Automatically set the global value according to the global object.
 *
 * In this function, global value is allocated on heap without being freed intentionally.
 * As ns3 expected, the global should be static variable to keep it alive during the whole
 * simulation. To make the global values automatically set, we allocate them on heap and
 * never free them. Please take care of this and never overuse this function.
 */
void SetGlobal(boost::json::object& globalObj);

void PrettyPrint(std::ostream& os, const boost::json::value& jv, std::string* indent = nullptr);

void SetRandomSeed(boost::json::object& configJsonObj);

void SetRandomSeed(uint32_t seed);

void SetStopTime(boost::json::object& configJsonObj);

std::shared_ptr<TraceApplicationHelper> ConstructTraceAppHelper(const boost::json::object& conf,
                                                                Ptr<DcTopology> topology);

ApplicationContainer InstallApplications(const boost::json::object& conf, Ptr<DcTopology> topology);

/**
 * \brief Constract the CDF from CDF config.
 *
 * This function will call trace application helper to read the CDF.
 *
 * \param conf The CDF config.
 */
std::unique_ptr<TraceApplication::TraceCdf> ConstructCdf(const boost::json::object& conf);

// Convert boost::json::value to number.
// To use these functions, make sure you know the type of the value.
double ConvertToDouble(const boost::json::value& v);
int64_t ConvertToInt(const boost::json::value& v);
uint64_t ConvertToUint(const boost::json::value& v);

/**
 * \brief Write statistics to the file specified in the config.
 * 
 * This function will also write the config to the file to keep the mapping from config to stats.
 * 
 * \param conf The CDF config in a json object.
 * \param apps The trace application container.
 * \param topology The topology.
 */
void OutputStats(boost::json::object& conf,
                 ApplicationContainer& apps,
                 Ptr<DcTopology> topology);

/**
 * \brief Store the app stats into a json object and return it.
 * 
 * \return A json object containing the app stats, use shared_ptr as it is a heavy object.
 */
std::shared_ptr<boost::json::object> ConstructAppStatsObj(ApplicationContainer& apps);

} // namespace json_util

} // namespace ns3

#endif /* JSON_UTIL_H */
