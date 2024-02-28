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
#ifndef JSON_APPLICATION_HELPER_H
#define JSON_APPLICATION_HELPER_H

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

std::shared_ptr<DcbTrafficGenApplicationHelper> ConstructTraceAppHelper(const boost::json::object& conf,
                                                                Ptr<DcTopology> topology);

ApplicationContainer InstallApplications(const boost::json::object& conf, Ptr<DcTopology> topology);

void SetGroupAttributes(const boost::json::object& gConf,
                        std::shared_ptr<DcbTrafficGenApplicationHelper> helper,
                        uint32_t groupSize,
                        uint32_t idx);

/**
 * \brief Constract the CDF from CDF config.
 *
 * This function will call trace application helper to read the CDF.
 *
 * \param conf The CDF config.
 */
std::unique_ptr<DcbTrafficGenApplication::TraceCdf> ConstructCdf(const boost::json::object& conf);

/**
 * \brief Get a set containing a specific number of random hosts.
 * \param num The number of hosts.
 * \param max The max host index.
 */
std::set<uint32_t> GetRandomHosts(uint32_t num, uint32_t max);

} // namespace json_util

} // namespace ns3

#endif /* JSON_APPLICATION_HELPER_H */
