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

#include "ns3/dcb-channel.h"
#include "ns3/double.h"
#include "ns3/fatal-error.h"
#include "ns3/integer.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/object-base.h"
#include "ns3/object.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rocev2-header.h"
#include "ns3/type-id.h"
#include "ns3/udp-based-l4-protocol.h"
#include "ns3/udp-header.h"

#include <vector>

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

uint32_t
DcTopology::GetNodeIdxFormIp(const Ipv4Address ip) const
{
    for (uint32_t i = 0; i < m_nodes.size(); i++)
    {
        Ptr<Ipv4> ipv4 = m_nodes[i].nodePtr->GetObject<Ipv4>();
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); j++)
        {
            if (ipv4->GetAddress(j, 0).GetLocal() == ip)
            {
                return i;
            }
        }
    }
    NS_FATAL_ERROR("ip " << ip << " is not found in the topology");
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

uint32_t
DcTopology::GetNHosts() const
{
    return m_nHosts;
}

uint32_t
DcTopology::GetNNodes() const
{
    return m_nodes.size();
}

void
DcTopology::AddDelay(const Ipv4Address src,
                     const Ipv4Address dst,
                     const uint32_t hops,
                     const Time delay)
{
    NS_LOG_FUNCTION(this << src << dst << hops << delay);
    m_delayMap[std::make_pair(src, dst)] = std::make_pair(hops, delay);
}

const Time
DcTopology::GetDelay(const Ipv4Address src, const Ipv4Address dst) const
{
    NS_LOG_FUNCTION(this << src << dst);
    auto it = m_delayMap.find(std::make_pair(src, dst));
    if (it == m_delayMap.end())
    {
        NS_FATAL_ERROR("Propogation delay from " << src << " to " << dst << " is not found");
    }
    return it->second.second;
}

const uint32_t
DcTopology::GetHops(const Ipv4Address src, const Ipv4Address dst) const
{
    NS_LOG_FUNCTION(this << src << dst);
    auto it = m_delayMap.find(std::make_pair(src, dst));
    if (it == m_delayMap.end())
    {
        NS_FATAL_ERROR("Propogation delay from " << src << " to " << dst << " is not found");
    }
    return it->second.first;
}

void
DcTopology::LogDelayMap() const
{
    NS_LOG_FUNCTION(this);
    for (auto it = m_delayMap.begin(); it != m_delayMap.end(); it++)
    {
        NS_LOG_DEBUG("From " << it->first.first << " to " << it->first.second
                             << ", propogation delay is " << it->second.second << " with "
                             << it->second.first << " hops");
    }
}

Ptr<Ipv4Route>
DcTopology::GetRoute(const Ptr<Ipv4GlobalRouting> route,
                     const Ipv4Address srcAddr,
                     const Ipv4Address dstAddr,
                     Socket::SocketErrno& sockerr) const
{
    NS_LOG_FUNCTION(this);
    // assert if n > 1
    // NS_ASSERT(route->GetNRoutes() == 1);
    Ipv4Header ipHdr;
    ipHdr.SetDestination(dstAddr);
    ipHdr.SetSource(srcAddr);

    // liuichangTODO: ecmp in route is affected by this; should be configed by config
    // ipHdr.SetProtocol(UdpL4Protocol::PROT_NUMBER);
    // create a packet
    Ptr<Packet> packet = Create<Packet>();
    // find the route by using RouteOutput
    Ptr<Ipv4Route> found = route->RouteOutput(packet, ipHdr, 0, sockerr);
    return found;
}

Ptr<Ipv4Route>
DcTopology::GetRoute(const Ptr<Ipv4GlobalRouting> route,
                     const Ipv4Address srcAddr,
                     const Ipv4Address dstAddr,
                     const uint32_t srcPort,
                     const uint32_t dstPort,
                     Socket::SocketErrno& sockerr) const
{
    NS_LOG_FUNCTION(this);

    // create a packet
    Ptr<Packet> packet = Create<Packet>();

    RoCEv2Header rocev2Header;
    rocev2Header.SetSrcQP(srcPort);
    rocev2Header.SetDestQP(dstPort);
    UdpHeader udpHeader;
    udpHeader.SetSourcePort(UdpBasedL4Protocol::PROT_NUMBER);
    udpHeader.SetDestinationPort(UdpBasedL4Protocol::PROT_NUMBER);
    packet->AddHeader(rocev2Header);
    packet->AddHeader(udpHeader);

    Ipv4Header ipHdr;
    ipHdr.SetDestination(dstAddr);
    ipHdr.SetSource(srcAddr);
    // liuichangTODO: ecmp in route is affected by this
    ipHdr.SetProtocol(UdpL4Protocol::PROT_NUMBER);

    // find the route by using RouteOutput
    Ptr<Ipv4Route> found = route->RouteOutput(packet, ipHdr, 0, sockerr);
    return found;
}

void
DcTopology::CreateDelayMap()
{
    NS_LOG_FUNCTION(this);
    for (HostIterator hostSrc = this->hosts_begin(); hostSrc != hosts_end(); hostSrc++)
    {
        for (DcTopology::HostIterator hostDst = this->hosts_begin(); hostDst != this->hosts_end();
             hostDst++)
        {
            if (hostSrc == hostDst)
            {
                continue;
            }
            Ptr<GlobalRouter> router = (*hostSrc)->GetObject<GlobalRouter>();
            // Support for multiple interfaces
            // the 0-th interface is loopbackNetDevice
            for (uint32_t srcIfIdx = 1; srcIfIdx < (*hostSrc)->GetNDevices(); srcIfIdx++)
            {
                for (uint32_t dstIfIdx = 1; dstIfIdx < (*hostDst)->GetNDevices(); dstIfIdx++)
                {
                    Ptr<Ipv4GlobalRouting> route = router->GetRoutingProtocol();
                    Ipv4Address srcAddr =
                        (*hostSrc)->GetObject<Ipv4>()->GetAddress(srcIfIdx, 0).GetLocal();
                    Ipv4Address dstAddr =
                        (*hostDst)->GetObject<Ipv4>()->GetAddress(dstIfIdx, 0).GetLocal();
                    Time delays = Seconds(0);
                    uint32_t hops = 0;
                    while (1)
                    {
                        Socket::SocketErrno theerrno;
                        // find the route by using RouteOutput
                        Ptr<Ipv4Route> found = GetRoute(route, srcAddr, dstAddr, theerrno);
                        if (theerrno == Socket::ERROR_NOTERROR)
                        {
                            // NS_LOG_DEBUG("found " << *found);
                            Ptr<DcbChannel> channel =
                                DynamicCast<DcbChannel>(found->GetOutputDevice()->GetChannel());
                            hops++;
                            delays += channel->GetDelay();
                            // traverse channel's devices and find another one
                            Ptr<NetDevice> dev = nullptr;
                            for (uint32_t i = 0; i < channel->GetNDevices(); i++)
                            {
                                dev = channel->GetDevice(i);
                                if (dev !=
                                    found->GetOutputDevice()) // find the other head of device
                                {
                                    route = dev->GetNode()
                                                ->GetObject<GlobalRouter>()
                                                ->GetRoutingProtocol();
                                    break;
                                }
                            }
                            // Arrive at the destination node, may be in other ports of dstNode
                            if (found->GetGateway() == dstAddr ||
                                (dev->GetNode() == (*hostDst).nodePtr))
                            {
                                break;
                            }
                        }
                        else
                        {
                            NS_LOG_DEBUG("not found");
                            break;
                        }
                    }
                    this->AddDelay(srcAddr, dstAddr, hops, delays);
                }
            }
        }
    }
}

uint32_t
DcTopology::GetOutDevIdx(const Ptr<Node> srcNode,
                         const Ipv4Address dstAddr,
                         const uint32_t srcPort,
                         const uint32_t dstPort)
{
    NS_LOG_FUNCTION(this);
    Ptr<GlobalRouter> router = srcNode->GetObject<GlobalRouter>();
    Ptr<Ipv4GlobalRouting> route = router->GetRoutingProtocol();
    Socket::SocketErrno theerrno;
    // find the route by using RouteOutput
    Ptr<Ipv4Route> found = GetRoute(route, Ipv4Address(), dstAddr, srcPort, dstPort, theerrno);
    if (theerrno == Socket::ERROR_NOTERROR)
    {
        return found->GetOutputDevice()->GetIfIndex();
    }
    else
    {
        NS_ASSERT_MSG(false, "couldn't find the outPort to dst!");
    }
    // uint32_t minHop = INT32_MAX;
    // std::vector<uint32_t> outDevs;
    // for (uint32_t srcIfIdx = 1; srcIfIdx < srcNode->GetNDevices(); srcIfIdx++)
    // {
    //     Ipv4Address srcAddr = srcNode->GetObject<Ipv4>()->GetAddress(srcIfIdx, 0).GetLocal();
    //     uint32_t currHops = GetHops(srcAddr, dstAddr);
    //     if (currHops < minHop)
    //     {
    //         minHop = currHops;
    //         outDevs.clear();
    //         outDevs.push_back(srcIfIdx);
    //     }
    //     else if (currHops == minHop)
    //     {
    //         outDevs.push_back(srcIfIdx);
    //     }
    // }
    // uint32_t outDevIdx = rand() % outDevs.size();
    // NS_ASSERT(outDevs[outDevIdx] != 0);
    // return outDevs[outDevIdx];
}

uint32_t
DcTopology::GetSwitchQueueIdx(const Ptr<Node> nxtNode,
                              const Ptr<NetDevice> nxtDev,
                              const Ptr<Packet> packet,
                              const Ipv4Header& ipHdr,
                              uint8_t priority)
{
    NS_LOG_FUNCTION(this << nxtNode << nxtDev << packet << priority);

    // get the dstAddr of this packet
    Ipv4Address dstAddr = ipHdr.GetDestination();

    // get the port number of switches in bcube
    uint32_t swPortNum = this->switches_begin()->nodePtr->GetNDevices();

    // if this packet is CNP then put this packet to the highest priority queue
    if (priority == Socket::SocketPriority::NS3_PRIO_INTERACTIVE)
    {
        uint32_t highestQ = (swPortNum - 1) + 1;
        return highestQ;
    }

    uint32_t queueIdx = 0;
    // check if the nxtNode is dstNode of packet
    for (uint32_t i = 1; i < nxtNode->GetNDevices(); i++)
    {
        if (dstAddr == GetInterfaceOfNode(nxtNode->GetId(), i).GetAddress())
        {
            // then the qIdx is equal to the number of ports on the switch
            queueIdx = swPortNum - 1;
            return queueIdx;
        }
    }
    // according to next to next node(a switch)
    Ptr<NetDevice> anotherDev;
    NS_ASSERT(nxtNode->GetNDevices() == 3); // only two dim for now in bcube
    for (uint32_t i = 1; i < nxtNode->GetNDevices(); i++)
    {
        if (nxtNode->GetDevice(i) != nxtDev)
        {
            anotherDev = nxtNode->GetDevice(i);
            break;
        }
    }
    Ptr<Channel> channel = anotherDev->GetChannel();
    Ptr<NetDevice> nxt2NxtDev;
    for (uint32_t i = 0; i < channel->GetNDevices(); i++)
    {
        if (channel->GetDevice(i) != anotherDev)
        {
            nxt2NxtDev = channel->GetDevice(i);
            break;
        }
    }
    Ptr<Node> nxt2NxtNode = nxt2NxtDev->GetNode();
    NS_ASSERT(IsSwitch(nxt2NxtNode->GetId()));
    Ptr<GlobalRouter> router = nxt2NxtNode->GetObject<GlobalRouter>();
    Ptr<Ipv4GlobalRouting> route = router->GetRoutingProtocol();
    // use route to get the output port in nxtNode
    Socket::SocketErrno sockerr;
    Ptr<Ipv4Route> found = route->RouteOutput(packet, ipHdr, 0, sockerr);
    if (sockerr == Socket::ERROR_NOROUTETOHOST)
    {
        NS_LOG_ERROR("can't find route to dst!");
    }
    uint32_t outDevIdx = found->GetOutputDevice()->GetIfIndex();
    queueIdx = outDevIdx - 1;

    return queueIdx;
}

uint32_t
DcTopology::GetHostQueueIdx(const Ptr<Node> currNode,
                            const Ptr<NetDevice> currDev,
                            const Ptr<Node> nxtNode,
                            const Ptr<Packet> packet,
                            const Ipv4Header& ipHdr,
                            uint8_t priority)
{
    NS_LOG_FUNCTION(this << nxtNode << packet);

    // get the srcAddr and dstAddr of this packet
    Ipv4Address srcAddr = ipHdr.GetSource();
    Ipv4Address dstAddr = ipHdr.GetDestination();

    // get the port number of switches in bcube
    uint32_t swPortNum = this->switches_begin()->nodePtr->GetNDevices();

    // if this packet is CNP then put this packet to the highest priority queue
    if (priority == Socket::SocketPriority::NS3_PRIO_INTERACTIVE)
    {
        uint32_t highestQ = (swPortNum - 1) + (swPortNum - 2) + 1;
        return highestQ;
    }

    // if the srcNode of this packet isn't this node then put this packet to RelayQ
    // if (GetInterfaceOfNode(currNode->GetId(), currDev->GetIfIndex()).GetAddress() != srcAddr)
    // {
    //     uint32_t relayQIdx = (swPortNum - 1) + (swPortNum - 2) + 1 + 1;
    //     return relayQIdx;
    // }
    bool isSrcNode = false;
    for (uint32_t i = 1; i < currNode->GetNDevices(); i++)
    {
        if (GetInterfaceOfNode(currNode->GetId(), currNode->GetDevice(i)->GetIfIndex())
                .GetAddress() == srcAddr)
        {
            isSrcNode = true;
            break;
        }
    }
    if (isSrcNode == false)
    {
        uint32_t relayQIdx = (swPortNum - 1) + (swPortNum - 2) + 1 + 1;
        return relayQIdx;
    }

    // get the Ipv4GlobalRouting in nxtNode
    Ptr<GlobalRouter> router = nxtNode->GetObject<GlobalRouter>();
    Ptr<Ipv4GlobalRouting> route = router->GetRoutingProtocol();

    uint32_t queueIdx = 0;
    // check the number of hops from srcNode to dstNode
    uint32_t hops = GetHops(srcAddr, dstAddr);

    // use route to get the output port in nxtNode
    Socket::SocketErrno sockerr;
    Ptr<Ipv4Route> found = route->RouteOutput(packet, ipHdr, 0, sockerr);

    if (sockerr == Socket::ERROR_NOROUTETOHOST)
    {
        NS_LOG_ERROR("can't find route to dst!");
    }
    uint32_t outDevIdx = found->GetOutputDevice()->GetIfIndex();

    // if hops is equal to 4, this packet should put to q outdevidx-1
    // if hops is equal to 2, this packet should put to q [swportnum-1] + [outdevidx-1]
    if (hops == 4) // the first type q (0 ~ swportnum-2)
    {
        queueIdx = outDevIdx - 1;
    }
    else if (hops == 2) // the second type q ([swportnum-1] ~ [swportnum-1]+[swportnum-2])
    {
        uint32_t qOffset = swPortNum - 1;
        queueIdx = qOffset + (outDevIdx - 1);
    }
    else
    {
        NS_LOG_ERROR("the wrong hops between srcAddr and dstAddr!");
    }
    return queueIdx;
}

const Ptr<UniformRandomVariable>
DcTopology::CreateRamdomHostChooser() const
{
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    rng->SetAttribute("Min", DoubleValue(0));
    // Note that the max value is included in the range
    rng->SetAttribute("Max", DoubleValue(m_nHosts - 1));
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
