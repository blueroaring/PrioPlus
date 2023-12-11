/*
 * Copyright (c) 2008 INRIA
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
 * Author: Pavinberg <pavin0702@gmail.com>
 */

#include "rocev2-socket.h"

#include "dcb-net-device.h"
#include "dcqcn.h"
#include "rocev2-l4-protocol.h"
#include "udp-based-l4-protocol.h"
#include "udp-based-socket.h"

#include "ns3/assert.h"
#include "ns3/csv-writer.h"
#include "ns3/data-rate.h"
#include "ns3/fatal-error.h"
#include "ns3/global-value.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-route.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <fstream>
#include <tuple>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RoCEv2Socket");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Socket);

TypeId
RoCEv2Socket::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2Socket")
                            .SetParent<UdpBasedSocket>()
                            .SetGroupName("Dcb")
                            .AddConstructor<RoCEv2Socket>();
    return tid;
}

RoCEv2Socket::RoCEv2Socket()
    : UdpBasedSocket(),
      m_senderNextPSN(0),
      m_psnEnd(0)
{
    NS_LOG_FUNCTION(this);
    m_stats = std::make_shared<Stats>();
    m_sockState = CreateObject<RoCEv2SocketState>();
    m_ccOps = CreateObject<DcqcnCongestionOps>(m_sockState);
    m_flowStartTime = Simulator::Now();
}

RoCEv2Socket::~RoCEv2Socket()
{
    NS_LOG_FUNCTION(this);
}

void
RoCEv2Socket::DoSendTo(Ptr<Packet> payload, Ipv4Address daddr, Ptr<Ipv4Route> route)
{
    NS_LOG_FUNCTION(this << payload << daddr << route);

    RoCEv2Header rocev2Header = CreateNextProtocolHeader();
    m_buffer.Push(rocev2Header.GetPSN(), std::move(rocev2Header), payload, daddr, route);

    // Record the statistics
    if (m_stats->tStart == Time(0))
    {
        m_stats->tStart = Simulator::Now();
    }
    m_stats->nTotalSizePkts++;
    m_stats->nTotalSizeBytes += payload->GetSize();

    SendPendingPacket();
}

void
RoCEv2Socket::SendPendingPacket()
{
    NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsRunning())
    {
        // Already scheduled, wait for the scheduled sending.
        // XXX Logic problem. In the case of low rate before and high rate now, the new
        // sending rate will be applied after the scheduled low-rate sending.
        return;
    }

    if (!CheckQueueDiscAvaliable(GetPriority()))
    {
        // The queue disc is unavaliable, wait for the next sending.

        // The interval serve as the interval of spinning when the qdisc is unavaliable.
        uint32_t sz = 1000; // XXX A typical packet size
        Time interval =
            m_deviceRate.CalculateBytesTxTime(sz * 100 / m_sockState->GetRateRatioPercent());
        m_sendEvent = Simulator::Schedule(interval, &RoCEv2Socket::SendPendingPacket, this);
        // XXX If all the sender's send rate is line rate, this will cause unfairness,
        // however, it is unlikely to happen.

        return;
    }

    if (m_buffer.GetSizeToBeSent() == 0)
    {
        // No packet to send.
        return;
        // Do not need to schedule next sending here.
        // When a new packet is pushed into the buffer, the sending will be scheduled at other
        // place.
    }

    // rateRatio is controled by congestion control
    // rateRatio = sending rate calculated by CC / line rate, which is between [0.0., 100.0]
    const double rateRatio =
        m_sockState->GetRateRatioPercent(); // in percentage, i.e., maximum is 100.0
    // Record the rate
    DataRate rate = m_deviceRate * (rateRatio / 100.00);
    m_stats->RecordCcRate(rate);
    // XXX Why? Do not have minimum rate ratio?
    // TODO Add minimum rate ratio
    // TODO Add window constraint
    if (rateRatio > 1e-6)
    {
        // [[maybe_unused]] const auto& [_, rocev2Header, payload, daddr, route] =
        //     m_buffer.GetNextShouldSent();
        const DcbTxBuffer::DcbTxBufferItem& item = m_buffer.GetNextShouldSent();
        const uint32_t sz = item.m_payload->GetSize() + 8 + 20 + 14;
        DoSendDataPacket(item);

        // Control the send rate by interval of sending packets.
        Time interval = m_deviceRate.CalculateBytesTxTime(sz * 100 / rateRatio);
        m_sendEvent = Simulator::Schedule(interval, &RoCEv2Socket::SendPendingPacket, this);
    }
}

void
RoCEv2Socket::DoSendDataPacket(const DcbTxBuffer::DcbTxBufferItem& item)
{
    NS_LOG_FUNCTION(this);

    [[maybe_unused]] const auto& [_, rocev2Header, payload, daddr, route] = item;
    // DCQCN is a rate based CC.
    // Send packet and delay a bit time to control the sending rate.
    m_ccOps->UpdateStateSend(payload);
    Ptr<Packet> packet = payload->Copy(); // do not modify the payload in the buffer
    packet->AddHeader(rocev2Header);
    m_innerProto->Send(packet,
                       route->GetSource(),
                       daddr,
                       m_endPoint->GetLocalPort(),
                       m_endPoint->GetPeerPort(),
                       route);

    // Record the statistics
    m_stats->nTotalSentPkts++;
    m_stats->nTotalSentBytes += payload->GetSize();
    // TODO Only payload size is recorded now
    m_stats->RecordSentPkt(payload->GetSize());
}

void
RoCEv2Socket::ForwardUp(Ptr<Packet> packet,
                        Ipv4Header header,
                        uint32_t port,
                        Ptr<Ipv4Interface> incomingInterface)
{
    RoCEv2Header rocev2Header;
    packet->RemoveHeader(rocev2Header);

    switch (rocev2Header.GetOpcode())
    {
    case RoCEv2Header::Opcode::RC_ACK:
        HandleACK(packet, rocev2Header);
        break;
    case RoCEv2Header::Opcode::CNP:
        m_ccOps->UpdateStateWithCNP();
        // Record the CNP
        m_stats->RecordRecvEcn();
        // NS_LOG_DEBUG("DCQCN: Received CNP and rate decreased to "
        //              << m_sockState->GetRateRatioPercent() << "% at time "
        //              << Simulator::Now().GetMicroSeconds() << "us. " << header.GetSource() << ":"
        //              << rocev2Header.GetSrcQP() << "->" << header.GetDestination() << ":"
        //              << rocev2Header.GetDestQP());
        break;
    default:
        HandleDataPacket(packet, header, port, incomingInterface, rocev2Header);
    }
}

void
RoCEv2Socket::HandleACK(Ptr<Packet> packet, const RoCEv2Header& roce)
{
    NS_LOG_FUNCTION(this << packet);

    AETHeader aeth;
    packet->RemoveHeader(aeth);

    switch (aeth.GetSyndromeType())
    {
    case AETHeader::SyndromeType::FC_DISABLED: { // normal ACK
        // XXX Why only pop out the first packet?
        uint32_t psn = m_buffer.Pop().m_psn; // packet acked, pop out from buffer
        if (psn != roce.GetPSN())
        {
            NS_FATAL_ERROR("RoCEv2 socket receive an ACK with PSN not expected");
        }
        if (psn + 1 == m_psnEnd)
        { // last ACk received, flow finshed
            NotifyFlowCompletes();
            // a delay to handle remaining packets (e.g., CNP)

            // FIXME: do not use magic number
            // NS_LOG_DEBUG("RoCEv2Socket will close at "
            //              << (Simulator::Now() + MicroSeconds(50)).GetMicroSeconds() << "us node "
            //              << Simulator::GetContext() << " qp " << roce.GetDestQP());
            // Simulator::Schedule(MicroSeconds(50), &RoCEv2Socket::Close, this);

            // Filter the orphan CNP packets at UdpBasedL4Protocol, thus the socket can be closed
            // immediately
            Close();

            // Record the statistics
            m_stats->tFinish = Simulator::Now();
            // Set the stop time of CC's timer
            m_ccOps->SetStopTime(Simulator::Now());
            NS_LOG_DEBUG("Finish a flow at time " << Simulator::Now().GetNanoSeconds() << "ns.");
        }
        break;
    }
    case AETHeader::SyndromeType::NACK: {
        GoBackN(roce.GetPSN());
        break;
    }
    default: {
        NS_FATAL_ERROR("Unexpected AET header Syndrome. Packet format wrong.");
    }
    }
}

void
RoCEv2Socket::HandleDataPacket(Ptr<Packet> packet,
                               Ipv4Header header,
                               uint32_t port,
                               Ptr<Ipv4Interface> incomingInterface,
                               const RoCEv2Header& roce)
{
    NS_LOG_FUNCTION(this << packet);

    const uint32_t srcQP = roce.GetSrcQP(), dstQP = roce.GetDestQP();
    Ipv4Address srcIp = header.GetSource();
    FlowIdentifier flowId = FlowIdentifier{std::move(srcIp), srcQP};
    std::map<FlowIdentifier, FlowInfo>::iterator flowInfoIter = m_receiverFlowInfo.find(flowId);
    // TODO: fork the socket instead of using one as the receiver, i.e., m_receiverFlowInfo should
    // be removed.
    if (flowInfoIter == m_receiverFlowInfo.end())
    {
        auto pp = m_receiverFlowInfo.emplace(std::move(flowId), FlowInfo{dstQP});
        flowInfoIter = std::move(pp.first);
        // TODO: erase flowInfo after flow finishes
    }

    // Check ECN
    if (header.GetEcn() == Ipv4Header::EcnType::ECN_CE) // ECN congestion encountered
    {
        flowInfoIter->second.receivedECN = true;
        ScheduleNextCNP(flowInfoIter, header);
    }

    // Check PSN
    const uint32_t psn = roce.GetPSN();
    uint32_t expectedPSN = flowInfoIter->second.nextPSN;

    // Debug utility, ugly but useful, please do not remove it
    // if (roce.GetSrcQP()==258 && header.GetDestination().Get() == 167772163 && psn >= 42)
    // {
    //     NS_LOG_DEBUG("Break point");
    // }

    if (psn == expectedPSN)
    {
        flowInfoIter->second.nextPSN = (expectedPSN + 1) & 0xffffff;
        if (roce.GetAckQ())
        { // send ACK
            // TODO No check of whether queue disc avaliable, as don't know how to hold the ACK
            // packet at l4
            Ptr<Packet> ack = RoCEv2L4Protocol::GenerateACK(dstQP, srcQP, psn);
            m_innerProto->Send(ack, header.GetDestination(), header.GetSource(), dstQP, srcQP, 0);
        }
        NS_LOG_DEBUG("Send ACK with PSN " << psn << " at time " << Simulator::Now().GetNanoSeconds()
                                          << "ns.");
    }
    else if (psn > expectedPSN)
    { // packet out-of-order, send NACK
        NS_LOG_LOGIC("RoCEv2 receiver " << Simulator::GetContext() << "send NACK of flow " << srcQP
                                        << "->" << dstQP);
        // TODO No check of whether queue disc avaliable, as don't know how to hold the NACK packet
        // at l4
        Ptr<Packet> nack = RoCEv2L4Protocol::GenerateNACK(dstQP, srcQP, expectedPSN);
        m_innerProto
            ->Send(nack, header.GetDestination(), header.GetSource(), dstQP, srcQP, nullptr);
    }
    else
    {
        NS_LOG_WARN("RoCEv2 socket receives smaller PSN than expected, something wrong");
    }

    // Zhaochen: The ForwardUp should hand over the src port, not the dst port
    // The L4 layer has been written wrongly. We pass the src port to the L4 layer in this function
    UdpBasedSocket::ForwardUp(packet, header, srcQP, incomingInterface);
}

void
RoCEv2Socket::GoBackN(uint32_t lostPSN) const
{
    // DcbTxBuffer::DcbTxBufferItemI item = m_buffer.FindPSN(lostPSN);
    NS_LOG_WARN("Go-back-N not implemented. Packet lost or out-of-order happens. Sender is node "
                << Simulator::GetContext() << " at time " << Simulator::Now().GetNanoSeconds()
                << "ns.");

    // TODO Record the statistics
}

void
RoCEv2Socket::ScheduleNextCNP(std::map<FlowIdentifier, FlowInfo>::iterator flowInfoIter,
                              Ipv4Header header)
{
    NS_LOG_FUNCTION(this);

    RoCEv2Socket::FlowInfo& flowInfo = flowInfoIter->second;
    if (flowInfo.lastCNPEvent.IsRunning() || !flowInfo.receivedECN)
    {
        return;
    }
    auto [srcIp, srcQP] = flowInfoIter->first;

    // send Congestion Notification Packet (CNP) to sender
    CheckControlQueueDiscAvaliable();
    Ptr<Packet> cnp = RoCEv2L4Protocol::GenerateCNP(flowInfo.dstQP, srcQP);
    m_innerProto
        ->Send(cnp, header.GetDestination(), header.GetSource(), flowInfo.dstQP, srcQP, nullptr);
    flowInfo.receivedECN = false;
    flowInfo.lastCNPEvent = Simulator::Schedule(m_ccOps->GetCNPInterval(),
                                                &RoCEv2Socket::ScheduleNextCNP,
                                                this,
                                                flowInfoIter,
                                                header);

    // NS_LOG_DEBUG("DCQCN: Receiver send CNP to " << srcIp << " qp " << srcQP << " at time "
    //                                             << Simulator::Now().GetMicroSeconds());
}

int
RoCEv2Socket::Bind()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_boundnetdevice,
                  "RoCEv2Socket should be bound to a net device before calling Bind");
    m_endPoint = m_innerProto->Allocate();
    m_endPoint->SetRxCallback(MakeCallback(&RoCEv2Socket::ForwardUp, this));
    return 0;
}

void
RoCEv2Socket::BindToNetDevice(Ptr<NetDevice> netdevice)
{
    NS_LOG_FUNCTION(this << netdevice);
    // a little check
    if (netdevice)
    {
        bool found = false;
        Ptr<Node> node = GetNode();
        for (uint32_t i = 0; i < node->GetNDevices(); i++)
        {
            if (node->GetDevice(i) == netdevice)
            {
                found = true;
                break;
            }
        }
        NS_ASSERT_MSG(found, "Socket cannot be bound to a NetDevice not existing on the Node");
    }
    m_boundnetdevice = netdevice;
    // store device data rate
    Ptr<DcbNetDevice> dcbDev = DynamicCast<DcbNetDevice>(netdevice);
    if (dcbDev)
    {
        m_deviceRate = dcbDev->GetDataRate();
        // double rai =
        //     static_cast<double> (DataRate ("100Mbps").GetBitRate ()) / m_deviceRate.GetBitRate
        //     ();
        // m_ccOps->SetRateAIRatio (rai);
        // m_ccOps->SetRateHyperAIRatio (10 * rai);
        m_ccOps->SetReady();
    }
    else
    {
        // Set the data rate to the default value
        StringValue sdv;
        if (GlobalValue::GetValueByNameFailSafe("defaultRate", sdv))
            m_deviceRate = DataRate(sdv.Get());
        else
            NS_FATAL_ERROR("RoCEv2Socket is not bound to a DcbNetDevice and no default rate is "
                           "set.");
        m_ccOps->SetReady();
    }
    // Get local ipv4 address
    Ptr<Ipv4> ipv4 = GetNode()->GetObject<Ipv4>();
    NS_ASSERT(ipv4 != nullptr);
    uint32_t ifn = ipv4->GetInterfaceForDevice(netdevice);
    m_localAddress = ipv4->GetAddress(ifn, 0).GetLocal();
}

int
RoCEv2Socket::BindToLocalPort(uint32_t port)
{
    NS_LOG_FUNCTION(this << port);

    NS_ASSERT_MSG(m_boundnetdevice,
                  "RoCEv2Socket should be bound to a net device before calling Bind");
    m_endPoint = DynamicCast<RoCEv2L4Protocol>(m_innerProto)->Allocate(port, 0);
    m_endPoint->SetRxCallback(MakeCallback(&RoCEv2Socket::ForwardUp, this));
    return 0;
}

void
RoCEv2Socket::FinishSending()
{
    NS_LOG_FUNCTION(this);

    m_psnEnd = m_senderNextPSN;
}

void
RoCEv2Socket::SetStopTime(Time stopTime)
{
    m_ccOps->SetStopTime(stopTime);
}

Time
RoCEv2Socket::GetFlowStartTime() const
{
    return m_flowStartTime;
}

RoCEv2Header
RoCEv2Socket::CreateNextProtocolHeader()
{
    NS_LOG_FUNCTION(this);

    RoCEv2Header rocev2Header;
    rocev2Header.SetOpcode(RoCEv2Header::Opcode::RC_SEND_ONLY); // TODO: custom opcode
    rocev2Header.SetDestQP(m_endPoint->GetPeerPort());
    rocev2Header.SetSrcQP(m_endPoint->GetLocalPort());
    rocev2Header.SetPSN(m_senderNextPSN);
    // XXX Request every packet to be ACKed.. Is it necessary?
    rocev2Header.SetAckQ(true);
    m_senderNextPSN = (m_senderNextPSN + 1) % 0xffffff;
    return rocev2Header;
}

bool
RoCEv2Socket::CheckQueueDiscAvaliable(uint8_t priority) const
{
    Ptr<DcbNetDevice> dcbDev = DynamicCast<DcbNetDevice>(m_boundnetdevice);
    if (dcbDev == nullptr)
    {
        return true;
    }

    Ptr<PausableQueueDisc> qdisc = DynamicCast<PausableQueueDisc>(dcbDev->GetQueueDisc());
    if (qdisc == nullptr)
    {
        return true;
    }

    QueueSize qsize = qdisc->GetInnerQueueSize(priority);
    // TODO Make the threshold clearer
    QueueSize threshold = QueueSize("10000B");
    if (qsize < threshold)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void
RoCEv2Socket::CheckControlQueueDiscAvaliable() const
{
    NS_ASSERT_MSG(CheckQueueDiscAvaliable(Socket::SocketPriority::NS3_PRIO_INTERACTIVE),
                  "The control queue disc of Node " << GetObject<Node>()->GetId()
                                                    << " is unavaliable.");
}

NS_OBJECT_ENSURE_REGISTERED(DcbTxBuffer);

TypeId
DcbTxBuffer::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::DcbTxBuffer")
                            .SetGroupName("Dcb")
                            .SetParent<Object>()
                            .AddConstructor<DcbTxBuffer>();
    return tid;
}

DcbTxBuffer::DcbTxBuffer()
    : m_sentIdx(0)
{
}

void
DcbTxBuffer::Push(uint32_t psn,
                  RoCEv2Header header,
                  Ptr<Packet> packet,
                  Ipv4Address daddr,
                  Ptr<Ipv4Route> route)
{
    m_buffer.emplace_back(psn, header, packet, daddr, route);
}

const DcbTxBuffer::DcbTxBufferItem&
DcbTxBuffer::Front() const
{
    return m_buffer.front();
}

DcbTxBuffer::DcbTxBufferItem
DcbTxBuffer::Pop()
{
    DcbTxBuffer::DcbTxBufferItem item = std::move(m_buffer.front());
    m_buffer.pop_front();
    if (m_sentIdx)
    {
        m_sentIdx--;
    }
    return item;
}

const DcbTxBuffer::DcbTxBufferItem&
DcbTxBuffer::GetNextShouldSent()
{
    // TODO The next should send packet could not be the packet who is pointed by sentIdx
    if (m_buffer.size() - m_sentIdx)
    {
        return m_buffer.at(m_sentIdx++);
    }
    NS_FATAL_ERROR("DcbTxBuffer has no packet to be sent.");
}

uint32_t
DcbTxBuffer::Size() const
{
    return m_buffer.size();
}

uint32_t
DcbTxBuffer::GetSizeToBeSent() const
{
    return m_buffer.size() - m_sentIdx;
}

DcbTxBuffer::DcbTxBufferItemI
DcbTxBuffer::FindPSN(uint32_t psn) const
{
    for (auto it = m_buffer.cbegin(); it != m_buffer.cend(); it++)
    {
        if (psn == (*it).m_psn)
        {
            return it;
        }
    }
    return End();
}

DcbTxBuffer::DcbTxBufferItemI
DcbTxBuffer::End() const
{
    return m_buffer.cend();
}

DcbTxBuffer::DcbTxBufferItem::DcbTxBufferItem(uint32_t psn,
                                              RoCEv2Header header,
                                              Ptr<Packet> payload,
                                              Ipv4Address daddr,
                                              Ptr<Ipv4Route> route)
    : m_psn(psn),
      m_header(header),
      m_payload(payload),
      m_daddr(daddr),
      m_route(route)
{
}

NS_OBJECT_ENSURE_REGISTERED(RoCEv2SocketState);

TypeId
RoCEv2SocketState::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2SocketState")
                            .SetParent<Object>()
                            .SetGroupName("Dcb")
                            .AddConstructor<RoCEv2SocketState>();
    return tid;
}

RoCEv2SocketState::RoCEv2SocketState()
    : m_rateRatio(100.)
{
}

RoCEv2Socket::Stats::Stats()
    : nTotalSizePkts(0),
      nTotalSizeBytes(0),
      nTotalSentPkts(0),
      nTotalSentBytes(0),
      nTotalDeliverPkts(0),
      nTotalDeliverBytes(0),
      nTotalLossPkts(0),
      nTotalLossBytes(0),
      tStart(Time(0)),
      tFinish(Time(0)),
      tFct(Time(0)),
      overallFlowRate(DataRate(0))
{
    BooleanValue bv;
    if (GlobalValue::GetValueByNameFailSafe("detailedStats", bv))
        bDetailedStats = bv.Get();
    else
        bDetailedStats = false;
}

std::shared_ptr<RoCEv2Socket::Stats>
RoCEv2Socket::GetStats() const
{
    m_stats->CollectAndCheck();
    return m_stats;
}

void
RoCEv2Socket::Stats::CollectAndCheck()
{
    // Check the sanity of the statistics
    NS_ASSERT_MSG(tStart != Time(0), "The flow has not started yet.");
    NS_ASSERT_MSG(tStart <= tFinish, "The flow has not finished yet.");
    // NS_ASSERT_MSG(nTotalSizeBytes == nTotalDeliverBytes,
    //               "Total size bytes is not equal to total deliver bytes");
    NS_ASSERT_MSG(nTotalSentBytes == nTotalSizeBytes + nTotalLossBytes,
                  "Total sent bytes is not equal to total size bytes plus total loss bytes");

    tFct = tFinish - tStart;
    // Calculate the overallFlowRate
    overallFlowRate = DataRate(nTotalSizeBytes * 8.0 / tFct.GetSeconds());
}

void
RoCEv2Socket::Stats::RecordCcRate(DataRate rate)
{
    if (bDetailedStats)
    {
        // Check if the rate is changed
        if (vCcRate.size() > 0 && vCcRate.back().second == rate)
        {
            return;
        }
        vCcRate.push_back(std::make_pair(Simulator::Now(), rate));
    }
}

void
RoCEv2Socket::Stats::RecordCcCwnd(uint32_t cwnd)
{
    if (bDetailedStats)
    {
        // Check if the cwnd is changed
        if (vCcCwnd.size() > 0 && vCcCwnd.back().second == cwnd)
        {
            return;
        }
        vCcCwnd.push_back(std::make_pair(Simulator::Now(), cwnd));
    }
}

void
RoCEv2Socket::Stats::RecordRecvEcn()
{
    if (bDetailedStats)
    {
        vRecvEcn.push_back(Simulator::Now());
    }
}

void
RoCEv2Socket::Stats::RecordSentPkt(uint32_t size)
{
    if (bDetailedStats)
    {
        vSentPkt.push_back(std::make_pair(Simulator::Now(), size));
    }
}

} // namespace ns3
