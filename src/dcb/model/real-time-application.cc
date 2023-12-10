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

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RealTimeApplication");
NS_OBJECT_ENSURE_REGISTERED(RealTimeApplication);

TypeId
RealTimeApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RealTimeApplication").SetParent<TraceApplication>().SetGroupName("Dcb");
    return tid;
}

RealTimeApplication::RealTimeApplication(Ptr<DcTopology> topology,
                                         uint32_t nodeIndex,
                                         int32_t destIndex /* = -1 */)
    : TraceApplication(topology, nodeIndex, destIndex),
      m_pktSeq(0)
{
    NS_LOG_FUNCTION(this);

    // This will replace the base class's m_stats
    m_stats = std::make_shared<Stats>();
}

RealTimeApplication::RealTimeApplication(Ptr<DcTopology> topology,
                                         Ptr<Node> node,
                                         InetSocketAddress destAddr)
    : TraceApplication(topology, node, destAddr),
      m_pktSeq(0)
{
    NS_LOG_FUNCTION(this);

    // This will replace the base class's m_stats
    m_stats = std::make_shared<Stats>();
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
    packet->AddPacketTag(tag);

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
            NS_LOG_LOGIC("TraceApplication: At time "
                         << Simulator::Now().As(Time::S) << " client received " << packet->GetSize()
                         << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                         << " port " << InetSocketAddress::ConvertFrom(from).GetPort());
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            NS_LOG_LOGIC("TraceApplication: At time "
                         << Simulator::Now().As(Time::S) << " client received " << packet->GetSize()
                         << " bytes from " << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
                         << " port " << Inet6SocketAddress::ConvertFrom(from).GetPort());
        }
        // socket->GetSockName (localAddress);
        // m_rxTrace (packet);
        // m_rxTraceWithAddresses (packet, from, localAddress);

        // Real time statistics, record them from RealTimeStatsTag
        RealTimeStatsTag tag;
        if (packet->PeekPacketTag(tag))
        {
            uint32_t pktSeq = tag.GetPktSeq();
            m_stats->nMaxRecvSeq = std::max(m_stats->nMaxRecvSeq, pktSeq);

            m_stats->vArriveDelay.push_back(Simulator::Now() - Time(tag.GetTArriveNs()));
            m_stats->vTxDelay.push_back(Simulator::Now() - Time(tag.GetTTxNs()));

            m_stats->nTotalRecvBytes += packet->GetSize();
            m_stats->nTotalRecvPkts++;
            m_stats->tFirstPktArrive = std::min(m_stats->tFirstPktArrive, Time(tag.GetTArriveNs()));
            m_stats->tFirstPktRecv = std::min(m_stats->tFirstPktRecv, Simulator::Now());
            m_stats->tLastPktRecv = std::max(m_stats->tLastPktRecv, Simulator::Now());

            if (m_stats->bDetailedStats)
            {
                m_stats->vRecvPkt.push_back(std::make_pair(Simulator::Now(), packet->GetSize()));
            }
        }
    }
}

std::shared_ptr<TraceApplication::Stats>
RealTimeApplication::GetStats() const
{
    m_stats->CollectAndCheck(m_flows);
    return m_stats;
}

RealTimeApplication::Stats::Stats()
    : nMaxRecvSeq(0),
      nPktLoss(0),
      nTotalRecvPkts(0),
      nTotalRecvBytes(0),
      tFirstPktArrive(Time::Max()),
      tFirstPktRecv(Time::Max()),
      tLastPktRecv(Time::Min())
{
}

void
RealTimeApplication::Stats::CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows)
{
    // Call the base class's CollectAndCheck
    TraceApplication::Stats::CollectAndCheck(flows);

    // Check the packet loss
    if (flows.size() != 1)
    {
        NS_FATAL_ERROR("RealTimeApplication only support one flow now");
    }
    /* XXX
     * The packet loss is calculated as the difference between the max sequence number and the
     * total number of received packets. However, the max sequence number is not the total number
     * of sent packets, so the packet loss is not accurate.
     * The max seq is plus one because the seq starts from 0.
     */
    nPktLoss = (nMaxRecvSeq + 1) - nTotalRecvPkts;

    // Calculate the average rate
    rAvgRateFromArrive = DataRate(nTotalRecvBytes * 8.0 / (tLastPktRecv - tFirstPktArrive).GetSeconds());
    rAvgRateFromRecv = DataRate(nTotalRecvBytes * 8.0 / (tLastPktRecv - tFirstPktRecv).GetSeconds());
}

} // namespace ns3
