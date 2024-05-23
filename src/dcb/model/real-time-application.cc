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

#include "real-time-application.h"

#include "ns3/global-value.h"
#include "ns3/tcp-socket-base.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RealTimeApplication");
NS_OBJECT_ENSURE_REGISTERED(RealTimeApplication);

TypeId
RealTimeApplication::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RealTimeApplication")
                            .SetParent<DcbTrafficGenApplication>()
                            .SetGroupName("Dcb")
                            .AddConstructor<RealTimeApplication>();
    return tid;
}

RealTimeApplication::RealTimeApplication(Ptr<DcTopology> topology,
                                         uint32_t nodeIndex)
    : DcbTrafficGenApplication(topology, nodeIndex),
      m_pktSeq(0)
{
    NS_LOG_FUNCTION(this);

    // This will replace the base class's m_stats
    m_stats = std::make_shared<Stats>(this);
}

RealTimeApplication::RealTimeApplication()
    : DcbTrafficGenApplication(),
      m_pktSeq(0)
{
    NS_LOG_FUNCTION(this);

    // This will replace the base class's m_stats
    m_stats = std::make_shared<Stats>(this);
}

RealTimeApplication::~RealTimeApplication()
{
    NS_LOG_FUNCTION(this);
}

void
RealTimeApplication::SendNextPacket(Flow* flow)
{
    const uint32_t packetSize = std::min(flow->remainBytes, MSS);
    Ptr<Packet> packet = Create<Packet>(packetSize);

    // Add real time stats tag to the packet
    // Calculate the pktSeq from the flow's totalBytes and remainBytes
    // uint32_t pktSeq = (flow->totalBytes - flow->remainBytes) / MSS;
    RealTimeStatsTag tag(m_pktSeq++, Simulator::Now().GetNanoSeconds());
    // packet->AddPacketTag(tag);
    packet->AddByteTag(tag);

    int actual = flow->socket->Send(packet);
    if (actual == static_cast<int>(packetSize))
    {
        m_totBytes += packetSize;
        Time txTime = m_socketLinkRate.CalculateBytesTxTime(packetSize + m_headerSize);
        // XXX We do not need flow truncate!!!
        if (true)
        {
            if (flow->remainBytes > MSS)
            { // Schedule next packet
                flow->remainBytes -= MSS;
                Simulator::Schedule(txTime, &RealTimeApplication::SendNextPacket, this, flow);
                return;
            }
            else
            {
                // flow sending completes
                Ptr<UdpBasedSocket> udpSock = DynamicCast<UdpBasedSocket>(flow->socket);
                if (udpSock)
                {
                    udpSock->FinishSending();
                }
                // TODO: do some trace here
                // ...
            }
        }
        // m_flows.erase (flow);
        // flow->Dispose ();
        // Dispose will delete the struct leading to flow a dangling pointer.
        // flow = nullptr;
    }
    else
    {
        // NS_FATAL_ERROR ("Unable to send packet; actual " << actual << " size " << packetSize <<
        // ";"); retry later
        Time txTime = m_socketLinkRate.CalculateBytesTxTime(packetSize + m_headerSize);
        Simulator::Schedule(txTime, &RealTimeApplication::SendNextPacket, this, flow);
    }
}

void
RealTimeApplication::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    // Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_LOGIC("DcbTrafficGenApplication: At time "
                         << Simulator::Now().As(Time::S) << " client received " << packet->GetSize()
                         << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                         << " port " << InetSocketAddress::ConvertFrom(from).GetPort());
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            NS_LOG_LOGIC("DcbTrafficGenApplication: At time "
                         << Simulator::Now().As(Time::S) << " client received " << packet->GetSize()
                         << " bytes from " << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
                         << " port " << Inet6SocketAddress::ConvertFrom(from).GetPort());
        }
        // socket->GetSockName (localAddress);
        // m_rxTrace (packet);
        // m_rxTraceWithAddresses (packet, from, localAddress);

        // Real time statistics, record them from RealTimeStatsTag
        RealTimeStatsTag tag;
        if (packet->FindFirstMatchingByteTag(tag))
        {
            // Construct the flow identifier
            InetSocketAddress inetFrom = InetSocketAddress::ConvertFrom(from);
            Ipv4Address srcAddr = inetFrom.GetIpv4(), dstAddr;
            uint32_t srcPort = inetFrom.GetPort(), dstPort = 0;
            if (m_protoGroup == ProtocolGroup::RoCEv2)
            {
                Ptr<RoCEv2Socket> roceSocket =
                    DynamicCast<RoCEv2Socket>(m_receiverSocket); // XXX Not a good way
                dstAddr = roceSocket->GetLocalAddress();
                dstPort = 100; // XXX Static for now
            }
            else if (m_protoGroup == ProtocolGroup::TCP)
            {
                Ptr<TcpSocketBase> tcpSocket =
                    DynamicCast<TcpSocketBase>(m_receiverSocket); // XXX Not a good way
                Address local;
                tcpSocket->GetSockName(local);

                // Get the address the socket is bound to
                Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
                Ipv4Address addr =
                    ipv4->GetAddress(ipv4->GetInterfaceForDevice(GetOutboundNetDevice()), 0)
                        .GetLocal();
                NS_LOG_INFO("Receiver is bound to " << addr);

                dstAddr = addr;
                dstPort = InetSocketAddress::ConvertFrom(local).GetPort();
            }
            FlowIdentifier flowId(srcAddr, dstAddr, srcPort, dstPort);

            // Get the flow stats, if not exist, create one
            std::shared_ptr<Stats::FlowStats> flowStats;
            auto it = m_stats->mflowStats.find(flowId);
            if (it == m_stats->mflowStats.end())
            {
                flowStats = std::make_shared<Stats::FlowStats>();
                m_stats->mflowStats[flowId] = flowStats;
            }
            else
            {
                flowStats = it->second;
            }

            uint32_t pktSeq = tag.GetPktSeq();
            flowStats->nMaxRecvSeq = std::max(flowStats->nMaxRecvSeq, pktSeq);

            flowStats->vArriveDelay.push_back(Simulator::Now() - NanoSeconds(tag.GetTArriveNs()));
            flowStats->vTxDelay.push_back(Simulator::Now() - NanoSeconds(tag.GetTTxNs()));

            flowStats->nTotalRecvBytes += packet->GetSize();
            flowStats->nTotalRecvPkts++;
            flowStats->tFirstPktArrive =
                std::min(flowStats->tFirstPktArrive, NanoSeconds(tag.GetTArriveNs()));
            flowStats->tFirstPktRecv = std::min(flowStats->tFirstPktRecv, Simulator::Now());
            flowStats->tLastPktRecv = std::max(flowStats->tLastPktRecv, Simulator::Now());

            if (flowStats->bDetailedSenderStats)
            {
                flowStats->vRecvPkt.push_back(std::make_pair(Simulator::Now(), packet->GetSize()));
            }
        }
    }
}

std::shared_ptr<DcbBaseApplication::Stats>
RealTimeApplication::GetStats() const
{
    m_stats->CollectAndCheck(m_flows);
    return m_stats;
}

RealTimeApplication::Stats::Stats(Ptr<RealTimeApplication> app)
    : DcbTrafficGenApplication::Stats(app),
      m_app(app),
      isCollected(false)
{
}

RealTimeApplication::Stats::FlowStats::FlowStats()
    : nMaxRecvSeq(0),
      nPktLoss(0),
      nTotalRecvPkts(0),
      nTotalRecvBytes(0),
      tFirstPktArrive(Time::Max()),
      tFirstPktRecv(Time::Max()),
      tLastPktRecv(Time::Min()),
      rAvgRateFromArrive(0),
      rAvgRateFromRecv(0),
      bDetailedSenderStats(false)
{
    BooleanValue bv;
    if (GlobalValue::GetValueByNameFailSafe("detailedSenderStats", bv))
        bDetailedSenderStats = bv.Get();
    else
        bDetailedSenderStats = false;
}

void
RealTimeApplication::Stats::CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows)
{
    // Call the base class's CollectAndCheck
    DcbTrafficGenApplication::Stats::CollectAndCheck(flows);

    // Check if the stats is collected
    if (isCollected)
    {
        return;
    }
    isCollected = true;

    for (auto& it : mflowStats)
    {
        it.second->CollectAndCheck();
    }
}

void
RealTimeApplication::Stats::FlowStats::CollectAndCheck()
{
    /* XXX
     * The packet loss is calculated as the difference between the max sequence number and the
     * total number of received packets. However, the max sequence number is not the total number
     * of sent packets, so the packet loss is not accurate.
     * The max seq is plus one because the seq starts from 0.
     */
    nPktLoss = (nMaxRecvSeq + 1) - nTotalRecvPkts;

    // Calculate the average rate
    rAvgRateFromArrive =
        DataRate(nTotalRecvBytes * 8.0 / (tLastPktRecv - tFirstPktArrive).GetSeconds());
    rAvgRateFromRecv =
        DataRate(nTotalRecvBytes * 8.0 / (tLastPktRecv - tFirstPktRecv).GetSeconds());
}

} // namespace ns3
