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

#include "dcb-trace-application-helper.h"
#include "ns3/dc-topology.h"
#include "ns3/dcb-trace-application.h"
#include "ns3/node.h"
#include "ns3/dcb-net-device.h"
#include "ns3/rocev2-l4-protocol.h"
#include "ns3/nstime.h"

namespace ns3 {

TraceApplicationHelper::TraceApplicationHelper (Ptr<DcTopology> topo)
    : m_topology (topo), m_cdf (nullptr), m_flowMeanInterval (0.), m_dest (-1), m_sendEnabled (true)
{
}

void
TraceApplicationHelper::SetProtocolGroup (TraceApplication::ProtocolGroup protoGroup)
{
  m_protoGroup = protoGroup;
}

void
TraceApplicationHelper::SetCdf (const TraceApplication::TraceCdf &cdf)
{
  m_cdf = &cdf;
}

void
TraceApplicationHelper::SetLoad (Ptr<const DcbNetDevice> dev, double load)
{
  NS_ASSERT_MSG (m_cdf, "Must set CDF to TraceApplicationHelper before setting load.");
  NS_ASSERT_MSG (load >= 0. && load <= 1., "Load shoud be between 0 and 1.");
  double mean = CalculateCdfMeanSize (m_cdf);
  if (load <= 1e-6)
    {
      m_sendEnabled = false;
    }
  else
    {
      m_sendEnabled = true;
      m_flowMeanInterval = mean * 8 / (dev->GetDataRate ().GetBitRate () * load) * 1e6; // us
    }
}

void
TraceApplicationHelper::SetDestination (int32_t dest)
{
  m_dest = dest;
}

ApplicationContainer
TraceApplicationHelper::Install (Ptr<Node> node) const
{
  NS_ASSERT_MSG (m_cdf, "[TraceApplicationHelper] CDF not set, please call SetCdf ().");
  NS_ASSERT_MSG (m_flowMeanInterval > 0 || !m_sendEnabled,
                 "[TraceApplicationHelper] Load not set, please call SetLoad ().");
  return ApplicationContainer (InstallPriv (node));
}

Ptr<Application>
TraceApplicationHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<TraceApplication> app;
  if (m_dest < 0)
    { // random destination flows application
      app = CreateObject<TraceApplication> (m_topology, node->GetId ());
    }
  else
    { // fixed destination flows application
      app = CreateObject<TraceApplication> (m_topology, node->GetId (), m_dest);
    }

  if (m_sendEnabled)
    {
      app->SetFlowCdf (*m_cdf);
      app->SetFlowMeanArriveInterval (m_flowMeanInterval);
    }
  else
    {
      Ptr<TraceApplication> appt = DynamicCast<TraceApplication> (app);
      if (appt)
        {
          appt->SetSendEnabled (false);
        }
    }

  node->AddApplication (app);

  app->SetProtocolGroup (m_protoGroup);
  switch (m_protoGroup)
    {
    case TraceApplication::ProtocolGroup::RAW_UDP:
      break; // do nothing
    case TraceApplication::ProtocolGroup::TCP:
      break; // TODO: add support of TCP
    case TraceApplication::ProtocolGroup::RoCEv2:
      // must be called after node->AddApplication () becasue it needs to know the node
      app->SetInnerUdpProtocol (RoCEv2L4Protocol::GetTypeId ());
    };
  return app;
}

// static
double
TraceApplicationHelper::CalculateCdfMeanSize (const TraceApplication::TraceCdf *const cdf)
{
  double res = 0.;
  auto [ls, lp] = (*cdf)[0];
  for (auto [s, p] : (*cdf))
    {
      res += (s + ls) / 2.0 * (p - lp);
      ls = s;
      lp = p;
    }
  return res;
}

} //namespace ns3
