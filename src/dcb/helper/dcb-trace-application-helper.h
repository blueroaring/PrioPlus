/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Universita' di Firenze, Italy
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
 * Author: Pavinberg (pavin0702@gmail.com)
 */

#ifndef TRACE_APPLICATION_HELPER_H
#define TRACE_APPLICATION_HELPER_H

#include "ns3/application-container.h"
#include "ns3/dc-topology.h"
#include "ns3/dcb-net-device.h"
#include "ns3/dcb-trace-application.h"
#include "ns3/net-device.h"
#include "ns3/object-factory.h"

namespace ns3 {

class DcbNetDevice;

class TraceApplicationHelper
{
public:
  TraceApplicationHelper (Ptr<DcTopology> topology);
  
  ApplicationContainer Install (Ptr<Node> node) const;

  void SetProtocolGroup (TraceApplication::ProtocolGroup protoGroup);
  void SetCdf (const TraceApplication::TraceCdf& cdf);
  void SetLoad (Ptr<const DcbNetDevice> dev, double load);
  void SetDestination (int32_t dest);

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  // void SetAttribute (std::string name, const AttributeValue &value);

private:

  /**
   * Install an ns3::UdpEchoClient on the node configured with all the
   * attributes set with SetAttribute.
   *
   * \param node The node on which an UdpEchoClient will be installed.
   * \returns Ptr to the application installed.
   */
  Ptr<Application> InstallPriv (Ptr<Node> node) const;

  static double CalculateCdfMeanSize (const TraceApplication::TraceCdf * const cdf);

  Ptr<DcTopology> m_topology;
  TraceApplication::ProtocolGroup m_protoGroup;
  TraceApplication::TraceCdf* m_cdf;
  double m_flowMeanInterval;
  int32_t m_dest;
  bool m_sendEnabled;

}; // class TraceApplicationHelper

} // namespace ns3

#endif
