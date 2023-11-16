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

#include "dc-topology.h"

#include "ns3/double.h"
#include "ns3/fatal-error.h"
#include "ns3/integer.h"
#include "ns3/ipv4.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/object-base.h"
#include "ns3/object.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/random-variable-stream.h"
#include "ns3/type-id.h"

/**
 * \file
 * \ingroup protobuf-topology
 * ns3::DcTopology implementation
 */
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcTopology");

NS_OBJECT_ENSURE_REGISTERED(DcTopology);

TypeId
DcTopology::GetTypeId()
{
    static TypeId tid = TypeId("ns3::DcTopology").SetParent<Object>().SetGroupName("DcEnv");
    return tid;
}

DcTopology::DcTopology(uint32_t nodeNum)
    : m_nHosts(0)
{
    NS_LOG_FUNCTION(this);
    m_nodes.resize(nodeNum);
    m_links.resize(nodeNum);
}

DcTopology::~DcTopology()
{
    NS_LOG_FUNCTION(this);
}

void
DcTopology::InstallNode(const uint32_t index, const TopoNode node)
{
    NS_LOG_FUNCTION(this << index);

    if (index >= m_nodes.size())
    {
        NS_FATAL_ERROR("node index " << index << " is out of bound, since there are "
                                     << m_nodes.size() << " nodes initialized.");
    }
    m_nodes[index] = node;
    m_nHosts += (node.type == TopoNode::NodeType::HOST);
}

void
DcTopology::InstallLink(const uint32_t node1, const uint32_t node2)
{
    NS_LOG_FUNCTION(this << node1 << node2);

    m_links[node1].push_back(node2);
    m_links[node2].push_back(node1);
}

const DcTopology::TopoNode&
DcTopology::GetNode(const uint32_t index) const
{
    if (index >= m_nodes.size())
    {
        NS_FATAL_ERROR("node index " << index << " is out of bound, since there are "
                                     << m_nodes.size() << " nodes initialized.");
    }
    return m_nodes[index];
}

const uint32_t
DcTopology::GetNodeIndex(const Ptr<Node> node) const
{
    const uint32_t n = m_nodes.size();
    for (uint32_t i = 0; i < n; i++)
    {
        if (m_nodes[i].nodePtr == node)
        {
            return i;
        }
    }
    NS_FATAL_ERROR("node " << node << " is not found in the topology");
}

const Ptr<NetDevice>
DcTopology::GetNetDeviceOfNode(const uint32_t nodei, const uint32_t devi) const
{
    const uint32_t ndev = GetNode(nodei)->GetNDevices();
    if (devi >= ndev)
    {
        NS_FATAL_ERROR("port index " << devi << " is out of bound, since there are " << ndev
                                     << " devices installed");
    }
    return StaticCast<NetDevice>(GetNode(nodei)->GetDevice(devi));
}

const Ipv4InterfaceAddress
DcTopology::GetInterfaceOfNode(const uint32_t nodei, uint32_t intfi) const
{
    Ptr<Ipv4> ipv4 = GetNode(nodei)->GetObject<Ipv4>();
    const uint32_t nintf = ipv4->GetNInterfaces();
    if (intfi > nintf)
    {
        NS_FATAL_ERROR("interface index " << intfi << " is out of bound, since there are " << nintf
                                          << " devices installed");
    }
    return std::move(ipv4->GetAddress(intfi, 0)); // TODO: just return the first address for now
}

bool
DcTopology::IsHost(const uint32_t index) const
{
    return GetNode(index).type == TopoNode::NodeType::HOST;
}

bool
DcTopology::IsSwitch(const uint32_t index) const
{
    return GetNode(index).type == TopoNode::NodeType::SWITCH;
}

const Ptr<UniformRandomVariable>
DcTopology::CreateRamdomHostChooser() const
{
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    rng->SetAttribute("Min", DoubleValue(0));
    // Note that the max value is included in the range
    rng->SetAttribute("Max", DoubleValue(m_nHosts));
    return rng;
}

void
DcTopology::Print(std::ostream& os) const
{
    os << "Topology:" << std::endl;
    const uint32_t n = m_nodes.size();
    for (uint32_t i = 0; i < n; i++)
    {
        std::string name1 = IsHost(i) ? "host" : "switch";
        for (uint32_t j : m_links[i])
        {
            if (i < j)
            {
                std::string name2 = IsHost(j) ? "host" : "switch";
                os << name1 << i << "<->" << name2 << j << std::endl;
            }
        }
    }
}

} // namespace ns3
