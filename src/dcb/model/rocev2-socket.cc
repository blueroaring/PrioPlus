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
#include "ns3/real-time-stats-tag.h"
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
    static TypeId tid =
        TypeId("ns3::RoCEv2Socket")
            .SetParent<UdpBasedSocket>()
            .SetGroupName("Dcb")
            .AddConstructor<RoCEv2Socket>()
            .AddAttribute("RetxMode",
                          "The retransmission mode",
                          EnumValue(RoCEv2RetxMode::GBN),
                          MakeEnumAccessor(&RoCEv2Socket::m_retxMode),
                          MakeEnumChecker(RoCEv2RetxMode::GBN, "GBN", RoCEv2RetxMode::IRN, "IRN"))
            .AddAttribute("CNPInterval",
                          "The CNP interval",
                          TimeValue(MicroSeconds(50)),
                          MakeTimeAccessor(&RoCEv2Socket::m_CNPInterval),
                          MakeTimeChecker())
            .AddAttribute("InnerPriority",
                          "The inner priority of the sockets when contenting with other sockets",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RoCEv2Socket::m_innerPrio),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RateCap",
                          "The rate cap of the socket",
                          DataRateValue(DataRate("0bps")),
                          MakeDataRateAccessor(&RoCEv2Socket::m_rateCap),
                          MakeDataRateChecker());
    return tid;
}

RoCEv2Socket::RoCEv2Socket()
    : UdpBasedSocket(),
      m_txBuffer(DcbTxBuffer(MakeCallback(&RoCEv2Socket::SendPendingPacket, this))),
      m_senderNextPSN(0),
      m_psnEnd(0),
      m_waitingForSchedule(false)
{
    NS_LOG_FUNCTION(this);
    m_stats = std::make_shared<Stats>();
    m_sockState = CreateObject<RoCEv2SocketState>();
    m_sockState->SetTxBuffer(&m_txBuffer);
    // Note that m_ccOps is not inited.
    //  m_ccOps = CreateObject<RoCEv2CongestionOps>(m_sockState);
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

    // Record the statistics
    if (m_stats->tStart == Time(0))
    {
        m_stats->tStart = Simulator::Now();
    }
    uint32_t mss = m_sockState->GetMss();
    m_stats->nTotalSizePkts += (payload->GetSize() + mss - 1) / mss;
    m_stats->nTotalSizeBytes += payload->GetSize();

    // Calculate the end PSN
    m_psnEnd = (payload->GetSize() + mss - 1) / mss;

    m_txBuffer.RecvPayload(payload, daddr, route, mss);
}

void
RoCEv2Socket::NotifyCouldSend()
{
    NS_LOG_FUNCTION(this);
    m_waitingForSchedule = false;
    SendPendingPacket();
}

void
RoCEv2Socket::SendPendingPacket()
{
    NS_LOG_FUNCTION(this);

    /**
     * Check whether can send packet. As a optimization, the judgement is arranged
     * in the order of the probability of the condition.
     */
    if (m_sendEvent.IsRunning() || m_waitingForSchedule)
    {
        // Already scheduled, wait for the scheduled sending.
        // XXX Logic problem. In the case of low rate before and high rate now, the new
        // sending rate will be applied after the scheduled low-rate sending.
        return;
    }

    const uint32_t totalPacketSize = m_sockState->GetPacketSize();
    if (!m_txBuffer.CouldSend((m_sockState->GetCwnd() + totalPacketSize - 1) /
                              totalPacketSize)) // ceiling to packet unit
    {
        // The cwnd is not enough to send a packet.
        return;
        // Do not need to schedule next sending here.
        // When there is a new ACK, the sending will be scheduled at other place.
    }

    if (m_txBuffer.GetSizeToBeSent() == 0)
    {
        // No packet to send.
        return;
        // Do not need to schedule next sending here.
        // When a new packet is pushed into the buffer, the sending will be scheduled at other
        // place.
    }

    // This check should be placed to the last, as it is will recall the function.
    // if (!CheckQueueDiscAvaliable(GetPriority()))
    // {
    //     // The queue disc is unavaliable, wait for the next sending.

    //     // The interval serve as the interval of spinning when the qdisc is unavaliable.
    //     uint32_t sz = 1000; // XXX A typical packet size
    //     Time interval = m_deviceRate.CalculateBytesTxTime(sz /
    //     m_sockState->GetRateRatioPercent()); m_sendEvent = Simulator::Schedule(interval,
    //     &RoCEv2Socket::SendPendingPacket, this);
    //     // XXX If all the sender's send rate is line rate, this will cause unfairness,
    //     // however, it is unlikely to happen.

    //     return;
    // }
    Ptr<RoCEv2L4Protocol> rocev2Proto = DynamicCast<RoCEv2L4Protocol>(m_innerProto);
    if (rocev2Proto->CheckCouldSend(m_boundnetdevice->GetIfIndex(), GetPriority()) == false)
    {
        // The queue disc is unavaliable, register a callback to RoCEv2L4Proto and wait for the
        // shedule
        rocev2Proto->RegisterSendPendingDataCallback(
            m_boundnetdevice->GetIfIndex(),
            GetPriority(),
            m_innerPrio,
            MakeCallback(&RoCEv2Socket::NotifyCouldSend, this));
        m_waitingForSchedule = true;

        return;
    }

    // rateRatio is controled by congestion control
    // rateRatio = sending rate calculated by CC / line rate, which is between [0.0, 1.0]
    const double rateRatio = m_sockState->GetRateRatioPercent(); // in percentage, i.e., maximum is
                                                                 // 100.0 Record the rate
    DataRate rate = m_deviceRate * rateRatio;
    m_stats->RecordCcRate(rate);
    // [[maybe_unused]] const auto& [_, rocev2Header, payload, daddr, route] =
    //     m_txBuffer.PeekNextShouldSend();
    RoCEv2Header rocev2Header = CreateNextProtocolHeader();
    const DcbTxBuffer::DcbTxBufferItem& item = m_txBuffer.PopNextShouldSend(rocev2Header);
    const uint32_t sz =
        item.m_payload->GetSize() + m_innerProto->GetHeaderSize() + m_ccOps->GetExtraHeaderSize();
    DoSendDataPacket(item);

    // Control the send rate by interval of sending packets.
    if (m_rateCap != DataRate("0bps"))
    {
        rate = std::min(rate, m_rateCap);
    }
    Time interval = rate.CalculateBytesTxTime(sz);
    m_sendEvent = Simulator::Schedule(interval, &RoCEv2Socket::SendPendingPacket, this);
}

void
RoCEv2Socket::DoSendDataPacket(const DcbTxBuffer::DcbTxBufferItem& item)
{
    NS_LOG_FUNCTION(this);

    [[maybe_unused]] const auto& [_, rocev2Header, payload, daddr, route] = item;

    Ptr<Packet> packet = payload->Copy(); // do not modify the payload in the buffer
    packet->AddHeader(rocev2Header);
    m_ccOps->UpdateStateSend(packet);

    // Before send, check if it has RealTimeStatsTag, if so, record the tx time
    RealTimeStatsTag tag;
    if (packet->RemovePacketTag(tag))
    {
        tag.SetTTxNs(Simulator::Now().GetNanoSeconds());
        packet->AddPacketTag(tag);
    }

    // Add the CongestionTypeTag to the packet
    CongestionTypeTag ctTag(m_congTypeId.GetUid());
    packet->AddPacketTag(ctTag);

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
    m_stats->RecordSentPsn(rocev2Header.GetPSN());

    // Used to debug
    NS_LOG_DEBUG("Send packet " << rocev2Header.GetPSN() << " at "
                                << Simulator::Now().GetNanoSeconds() << "ns.");
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
        m_ccOps->UpdateStateWithRcvACK(packet, rocev2Header, m_txBuffer.NextSendPsn());
        NS_LOG_DEBUG("RoCEv2Socket: Received ACK and rate decreased to "
                     << m_sockState->GetRateRatioPercent() * 100 << "% at time "
                     << Simulator::Now().GetMicroSeconds() << "us. " << header.GetSource() << ":"
                     << rocev2Header.GetSrcQP() << "->" << header.GetDestination() << ":"
                     << rocev2Header.GetDestQP());
        HandleACK(packet, rocev2Header);
        break;
    case RoCEv2Header::Opcode::CNP:
        m_ccOps->UpdateStateWithCNP();
        // Record the CNP
        m_stats->RecordRecvEcn();
        NS_LOG_DEBUG("DCQCN: Received CNP and rate decreased to "
                     << m_sockState->GetRateRatioPercent() << "% at time "
                     << Simulator::Now().GetMicroSeconds() << "us. " << header.GetSource() << ":"
                     << rocev2Header.GetSrcQP() << "->" << header.GetDestination() << ":"
                     << rocev2Header.GetDestQP());
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

    if (roce.GetPSN() >= 1444 && m_node->GetId() == 3)
    {
        NS_LOG_DEBUG("Break point");
    }

    // Record the expected PSN, for both ack and nack
    m_stats->RecordExpectedPsn(roce.GetPSN());

    switch (aeth.GetSyndromeType())
    {
    case AETHeader::SyndromeType::FC_DISABLED: { // normal ACK
        // Note that the psn in ACK's BTH is expected PSN, not the PSN of the ACKed packet
        m_txBuffer.AcknowledgeTo(roce.GetPSN() - 1);

        if (m_txBuffer.GetFrontPsn() == m_psnEnd)
        { // last ACk received, flow finshed
            NotifyFlowCompletes();

            // Filter the orphan CNP packets at UdpBasedL4Protocol, thus the socket can be closed
            // immediately
            Close();

            // Record the statistics
            m_stats->tFinish = Simulator::Now();
            // Set the stop time of CC's timer
            m_ccOps->SetStopTime(Simulator::Now());
            NS_LOG_DEBUG("Finish a flow at time " << Simulator::Now().GetNanoSeconds() << "ns.");
        }
        else if (m_retxMode == IRN)
        {
            IrnReactToAck(roce.GetPSN());
        }
        break;
    }
    case AETHeader::SyndromeType::NACK: {
        if (m_retxMode == GBN)
        {
            GoBackN(roce.GetPSN());
        }
        else if (m_retxMode == IRN)
        {
            IrnHeader irnH;
            packet->RemoveHeader(irnH);
            IrnReactToNack(roce.GetPSN(), irnH);
        }
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
        // Get the congestion type from CongestionTypeTag in the packet
        CongestionTypeTag ctTag;
        packet->PeekPacketTag(ctTag);
        ObjectFactory congestionAlgorithmFactory;
        congestionAlgorithmFactory.SetTypeId(ctTag.GetCongestionTypeId());
        Ptr<RoCEv2CongestionOps> algo = congestionAlgorithmFactory.Create<RoCEv2CongestionOps>();

        auto pp = m_receiverFlowInfo.emplace(
            std::move(flowId),
            FlowInfo{dstQP,
                     DcbRxBuffer{MakeCallback(&RoCEv2Socket::DoForwardUp, this),
                                 incomingInterface,
                                 m_retxMode},
                     algo});
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
    uint32_t expectedPSN = flowInfoIter->second.GetExpectedPsn();
    flowInfoIter->second.m_rxBuffer.Add(psn, header, roce, packet);

    // Debug utility, ugly but useful, please do not remove it
    // if (psn >= 2646)
    // {
    //     NS_LOG_DEBUG("Break point");
    // }
    NS_LOG_DEBUG("Receive packet " << psn << " at " << Simulator::Now().GetNanoSeconds() << "ns.");

    uint32_t ackHeaderSize =
        m_innerProto->GetHeaderSize() + 4 +
        flowInfoIter->second.m_ccOps->GetExtraAckSize(); // 4 bytes for AETHeader
    // If ack packet is smaller than 64B, use payload to pad it
    uint32_t ackPayloadSize = ackHeaderSize < 64 ? 64 - ackHeaderSize : 0;

    if (psn == expectedPSN)
    {
        // flowInfoIter->second.nextPSN = (expectedPSN + 1) & 0xffffff;
        if (roce.GetAckQ())
        { // send ACK
            // TODO No check of whether queue disc avaliable, as don't know how to hold the ACK
            // packet at l4
            Ptr<Packet> ack = RoCEv2L4Protocol::GenerateACK(dstQP,
                                                            srcQP,
                                                            flowInfoIter->second.GetExpectedPsn(),
                                                            ackPayloadSize);

            flowInfoIter->second.m_ccOps->UpdateStateWithGenACK(packet, ack);
            m_innerProto->Send(ack, header.GetDestination(), header.GetSource(), dstQP, srcQP, 0);
        }
        NS_LOG_DEBUG("Send ACK with PSN " << flowInfoIter->second.GetExpectedPsn() << " at time "
                                          << Simulator::Now().GetNanoSeconds() << "ns.");
    }
    else if (psn > expectedPSN)
    { // packet out-of-order, send NACK
        NS_LOG_LOGIC("RoCEv2 receiver " << Simulator::GetContext() << "send NACK of flow " << srcQP
                                        << "->" << dstQP);
        // TODO No check of whether queue disc avaliable, as don't know how to hold the NACK packet
        // at l4
        Ptr<Packet> nack = RoCEv2L4Protocol::GenerateNACK(dstQP,
                                                          srcQP,
                                                          flowInfoIter->second.GetExpectedPsn(),
                                                          ackPayloadSize);

        if (m_retxMode == RoCEv2RetxMode::IRN)
        {
            // If the receiver is in IRN mode, add a IRN header with the received packet's PSN
            IrnHeader irnH;
            irnH.SetAckedPsn(psn);
            // The IRN header should be placed after the RoCEv2 and AETH header
            // TODO So ugly, codesign it later with Feiyang's INT header placement
            RoCEv2Header rocev2Header;
            AETHeader aeth;
            nack->RemoveHeader(rocev2Header);
            nack->RemoveHeader(aeth);
            nack->AddHeader(irnH);
            nack->AddHeader(aeth);
            nack->AddHeader(rocev2Header);
        }

        m_innerProto
            ->Send(nack, header.GetDestination(), header.GetSource(), dstQP, srcQP, nullptr);
    }

    // Zhaochen: The ForwardUp should hand over the src port, not the dst port
    // The L4 layer has been written wrongly. We pass the src port to the L4 layer in this function
    // UdpBasedSocket::ForwardUp(packet, header, srcQP, incomingInterface);
}

void
RoCEv2Socket::GoBackN(uint32_t lostPSN)
{
    // DcbTxBuffer::DcbTxBufferItemI item = m_txBuffer.FindPSN(lostPSN);
    NS_LOG_DEBUG("Go-back-N to " << lostPSN << " at time " << Simulator::Now().GetNanoSeconds()
                                 << "ns.");
    m_txBuffer.RetransmitFrom(lostPSN);

    // Record the retx count
    m_stats->nRetxCount++;
}

void
RoCEv2Socket::IrnReactToNack(uint32_t expectedPsn, IrnHeader irnH)
{
    NS_LOG_FUNCTION(this);
    uint32_t ackedPsn = irnH.GetAckedPsn();

    m_txBuffer.AcknowledgeTo(expectedPsn - 1);
    m_txBuffer.Acknowledge(ackedPsn);
    m_txBuffer.Retransmit(expectedPsn);

    // Record the acked PSN
    m_stats->RecordAckedPsn(ackedPsn);

    NS_LOG_DEBUG("IRN expected PSN " << expectedPsn << " ackedPsn " << ackedPsn << " at time "
                                     << Simulator::Now().GetNanoSeconds() << "ns.");
}

void
RoCEv2Socket::IrnReactToAck(uint32_t expectedPsn)
{
    NS_LOG_FUNCTION(this);

    // For IRN, the expected packet should be retxed if it is not acked and there is a gap
    if (!m_txBuffer.HasGap())
    {
        return;
    }
    m_txBuffer.Retransmit(expectedPsn);

    NS_LOG_DEBUG("IRN expected PSN " << expectedPsn << " at time "
                                     << Simulator::Now().GetNanoSeconds() << "ns.");
}

void
RoCEv2Socket::ScheduleNextCNP(std::map<FlowIdentifier, FlowInfo>::iterator flowInfoIter,
                              Ipv4Header header)
{
    NS_LOG_FUNCTION(this);

    RoCEv2Socket::FlowInfo& flowInfo = flowInfoIter->second;
    // If there is already a CNP event running, or the receiver has not received ECN, do not send
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
    flowInfo.lastCNPEvent = Simulator::Schedule(GetCNPInterval(),
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
    }
    m_sockState->SetDeviceRate(&m_deviceRate);
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
RoCEv2Socket::SetStopTime(Time stopTime)
{
    m_ccOps->SetStopTime(stopTime);
}

void
RoCEv2Socket::SetCcOps(TypeId congTypeId)
{
    NS_LOG_FUNCTION(this << congTypeId);

    ObjectFactory congestionAlgorithmFactory;
    congestionAlgorithmFactory.SetTypeId(congTypeId);
    Ptr<RoCEv2CongestionOps> algo = congestionAlgorithmFactory.Create<RoCEv2CongestionOps>();
    m_ccOps = algo;
    m_ccOps->SetSockState(m_sockState);

    // Record the congestion type
    m_congTypeId = congTypeId;
}

void
RoCEv2Socket::SetCcOps(TypeId congTypeId,
                       std::vector<RoCEv2CongestionOps::CcOpsConfigPair_t>& ccConfig)
{
    NS_LOG_FUNCTION(this << congTypeId);

    SetCcOps(congTypeId);

    for (const auto& [name, value] : ccConfig)
    {
        m_ccOps->SetAttribute(name, *value);
    }
}

void
RoCEv2Socket::SetBaseRttNOneWayDelay(uint32_t hop,
                                     Time delay,
                                     uint32_t packetSize,
                                     uint32_t ackSize)
{
    Time transDelayPacket = Time(uint64_t(packetSize * 8e9 / m_deviceRate.GetBitRate()));
    Time transDelayAck = Time(uint64_t(ackSize * 8e9 / m_deviceRate.GetBitRate()));
    m_sockState->SetBaseRtt(delay * 2 + (transDelayPacket + transDelayAck) * hop);
    m_sockState->SetBaseOneWayDelay(delay + transDelayPacket * hop);
    // log the propogation delay, the hops, and the basertt
    NS_LOG_DEBUG("RoCEv2Socket: Propagation delay="
                 << delay.GetNanoSeconds() << "ns, hops=" << hop
                 << ", basertt=" << m_sockState->GetBaseRtt().GetNanoSeconds() << "ns"
                 << ", baserttNOneWayDelay=" << m_sockState->GetBaseOneWayDelay().GetNanoSeconds()
                 << "ns");
}

Time
RoCEv2Socket::GetCNPInterval() const
{
    return m_CNPInterval;
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
    // Now this threshold is set to 1500B, which is slightly larger than the typical packet size
    QueueSize threshold = QueueSize("1500B");
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

void
RoCEv2Socket::DoForwardUp(Ptr<Packet> packet,
                          Ipv4Header header,
                          uint32_t port,
                          Ptr<Ipv4Interface> incomingInterface)
{
    UdpBasedSocket::ForwardUp(packet, header, port, incomingInterface);
}

Ptr<RoCEv2CongestionOps>
RoCEv2Socket::GetCcOps() const
{
    return m_ccOps;
}

Ptr<RoCEv2SocketState>
RoCEv2Socket::GetSocketState() const
{
    return m_sockState;
}

NS_OBJECT_ENSURE_REGISTERED(DcbTxBuffer);

TypeId
DcbTxBuffer::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::DcbTxBuffer").SetGroupName("Dcb").SetParent<Object>();
    return tid;
}

DcbTxBuffer::DcbTxBuffer(Callback<void> sendCb)
    : m_sendCb(sendCb),
      m_frontPsn(0),
      m_maxAckedPsn(0),
      m_tos(0),
      m_priority(0)
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
    m_txQueue.push(psn);
    // m_acked.push_back(false);

    // // Call the send callback to notify the socket to send the packet
    // m_sendCb();
}

void
DcbTxBuffer::RecvPayload(Ptr<Packet> payload, Ipv4Address daddr, Ptr<Ipv4Route> route, uint32_t mss)
{
    m_remainSize = payload->GetSize();
    m_daddr = daddr;
    m_route = route;
    m_mss = mss;
    for (uint32_t i = 0; i < TotalSize(); i++)
    {
        m_acked.push_back(false);
    }

    SocketIpTosTag ipTosTag;
    payload->PeekPacketTag(ipTosTag);
    SocketPriorityTag priorityTag;
    payload->PeekPacketTag(priorityTag);
    if (ipTosTag.GetTos())
    {
        m_tos = ipTosTag.GetTos();
    }
    if (priorityTag.GetPriority())
    {
        m_priority = priorityTag.GetPriority();
    }

    m_sendCb();
}

const DcbTxBuffer::DcbTxBufferItem&
DcbTxBuffer::Front() const
{
    return m_buffer.front();
}

void
DcbTxBuffer::AcknowledgeTo(uint32_t psn)
{
    // No need to check whether psn is smaller than m_frontPsn as there maybe duplicate ack
    // in IRN. NS_ASSERT_MSG(psn >= m_frontPsn, "PSN to be acknowledged is smaller than the
    // front PSN.");
    NS_ASSERT_MSG(psn < TotalSize(), "PSN to be acknowledged is larger than the total size.");
    for (uint32_t i = m_frontPsn; i <= psn; i++)
    {
        m_acked[i] = true;
    }
    m_maxAckedPsn = std::max(m_maxAckedPsn, psn);

    CheckRelease();
    m_sendCb();
}

void
DcbTxBuffer::Acknowledge(uint32_t psn)
{
    NS_ASSERT_MSG(psn >= m_frontPsn, "PSN to be acknowledged is smaller than the front PSN.");
    NS_ASSERT_MSG(psn < TotalSize(), "PSN to be acknowledged is larger than the total size.");
    m_acked[psn] = true;
    m_maxAckedPsn = std::max(m_maxAckedPsn, psn);

    CheckRelease();
    m_sendCb();
}

void
DcbTxBuffer::CheckRelease()
{
    // Try to release the packets form m_frontPsn to the first unacked packet
    while (m_frontPsn < TotalSize() && m_acked[m_frontPsn])
    {
        m_buffer.pop_front();
        m_frontPsn++;
    }
    // Remove the released packets from the txQueue
    while (m_txQueue.top() < m_frontPsn && m_txQueue.size() != 0)
    {
        m_txQueue.pop();
    }
    // m_buffer.size() == 0 means all packets have been acked
    NS_ASSERT_MSG(m_frontPsn == m_buffer.front().m_psn || m_buffer.size() == 0,
                  "The buffer's order has broken.");
}

void
DcbTxBuffer::CreatePacket(RoCEv2Header roceHeader)
{
    uint32_t pktSize = std::min(m_remainSize, m_mss);
    Ptr<Packet> payload = Create<Packet>(pktSize);

    int priority = m_priority;
    if (m_tos)
    {
        // This tag will be used to set IP TOS field in L3
        SocketIpTosTag ipTosTag;
        ipTosTag.SetTos(m_tos);
        // This packet may already have a SocketIpTosTag (see BUG 2440)
        payload->ReplacePacketTag(ipTosTag);
        priority = UdpBasedSocket::IpTos2Priority(m_tos);
    }

    if (priority)
    {
        SocketPriorityTag priorityTag;
        priorityTag.SetPriority(priority);
        payload->ReplacePacketTag(priorityTag);
    }
    m_remainSize -= pktSize;
    Push(roceHeader.GetPSN(), std::move(roceHeader), payload, m_daddr, m_route);
}

uint32_t
DcbTxBuffer::NextSendPsn()
{
    if (GetSizeToBeSent() == 0)
    {
        return TotalSize(); // maximum PSN + 1
    }
    // If the txQueue is empty but there still are packets in the buffer, return the front PSN
    if (m_txQueue.empty())
    {
        return m_frontPsn + m_buffer.size();
    }
    // If the top of the txQueue is acked, pop it and check the next one
    uint32_t nextSendPsn = m_txQueue.top();
    while (m_acked[nextSendPsn] && m_txQueue.size() != 0)
    {
        m_txQueue.pop();
        nextSendPsn = m_txQueue.top();
    }
    return nextSendPsn;
}

const DcbTxBuffer::DcbTxBufferItem&
DcbTxBuffer::PeekNextShouldSend()
{
    // Check whether has packet to send
    NS_ASSERT(GetSizeToBeSent() != 0);
    uint32_t nextSendPsn = NextSendPsn();
    if (TotalSize() - nextSendPsn)
    {
        return m_buffer.at(nextSendPsn - m_frontPsn);
    }
    NS_FATAL_ERROR("DcbTxBuffer has no packet to be sent.");
}

const DcbTxBuffer::DcbTxBufferItem&
DcbTxBuffer::PopNextShouldSend(RoCEv2Header rocev2Header)
{
    // If m_txQueue is empty, push a packet into m_buffer
    if (m_txQueue.empty())
    {
        CreatePacket(rocev2Header);
    }
    const DcbTxBufferItem& item = PeekNextShouldSend();
    // If there are duplicate PSNs in the buffer, pop them all
    while (m_txQueue.top() == item.m_psn && m_txQueue.size() != 0)
    {
        m_txQueue.pop();
    }
    return item;
}

inline bool
DcbTxBuffer::CouldSend(uint64_t cwnd)
{
    return CouldSend(NextSendPsn(), cwnd);
}

inline bool
DcbTxBuffer::CouldSend(uint32_t psn, uint64_t cwnd)
{
    return static_cast<uint64_t>(psn) < cwnd + m_frontPsn;
}

uint32_t
DcbTxBuffer::Size() const
{
    return m_buffer.size() + RemainSizeInPacket();
}

uint32_t
DcbTxBuffer::TotalSize() const
{
    // m_buffer.size() + m_frontPsn = total number of packets to send = maximum PSN + 1
    return m_buffer.size() + m_frontPsn + RemainSizeInPacket();
}

uint32_t
DcbTxBuffer::GetSizeToBeSent() const
{
    return m_txQueue.size() + RemainSizeInPacket();
}

uint32_t
DcbTxBuffer::RemainSizeInPacket() const
{
    return (m_remainSize + m_mss - 1) / m_mss;
}

uint32_t
DcbTxBuffer::GetFrontPsn() const
{
    return m_frontPsn;
}

bool
DcbTxBuffer::HasGap() const
{
    return m_frontPsn != m_maxAckedPsn + 1;
}

DcbTxBuffer::DcbTxBufferItemI
DcbTxBuffer::FindPSN(uint32_t psn) const
{
    NS_ASSERT_MSG(psn < TotalSize(), "PSN not found in DcbTxBuffer");
    auto it = m_buffer.cbegin() + (psn - m_frontPsn);
    NS_ASSERT_MSG((*it).m_psn == psn,
                  "The found PSN is not the expected one, the buffer's order has broken.");
    return End();
}

DcbTxBuffer::DcbTxBufferItemI
DcbTxBuffer::End() const
{
    return m_buffer.cend();
}

void
DcbTxBuffer::Retransmit(uint32_t psn)
{
    NS_ASSERT_MSG(psn < TotalSize(), "PSN to be retransmitted is larger than the total size.");
    NS_ASSERT_MSG(psn >= m_frontPsn, "PSN to be retransmitted not in the buffer.");
    m_txQueue.push(psn);

    // Call the send callback to notify the socket to send the packet
    m_sendCb();
}

void
DcbTxBuffer::RetransmitFrom(uint32_t psn)
{
    NS_ASSERT_MSG(psn < TotalSize(), "PSN to be retransmitted is larger than the total size.");
    // Push all psn from the given psn to the txQueue
    // There will be many duplicate PSNs in the txQueue, which will be removed when poping
    for (uint32_t i = psn; i < TotalSize(); i++)
    {
        Retransmit(i);
    }
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

NS_OBJECT_ENSURE_REGISTERED(DcbRxBuffer);

TypeId
DcbRxBuffer::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::DcbRxBuffer").SetGroupName("Dcb").SetParent<Object>();
    return tid;
}

DcbRxBuffer::DcbRxBuffer(
    Callback<void, Ptr<Packet>, Ipv4Header, uint32_t, Ptr<Ipv4Interface>> forwardCb,
    Ptr<Ipv4Interface> incomingInterface,
    RoCEv2RetxMode retxMode)
    : m_forwardCb(forwardCb),
      m_expectedPsn(0),
      m_forwardInterface(incomingInterface),
      m_retxMode(retxMode)
{
}

void
DcbRxBuffer::Add(uint32_t psn, Ipv4Header ipv4, RoCEv2Header roce, Ptr<Packet> payload)
{
    // If the incoming psn is geq to expected PSN, and is not a duplicate packet, add the
    // packet to the buffer.
    // TODO The comparison should consider the warp around of the PSN
    if (m_retxMode == RoCEv2RetxMode::GBN)
    {
        if (psn == m_expectedPsn)
        {
            m_buffer.emplace(psn, DcbRxBufferItem(ipv4, roce, payload));
        }
        else if (psn > m_expectedPsn)
        {
            NS_LOG_DEBUG("RoCEv2 socket receives out-of-order packet "
                         << psn << " at " << Simulator::Now().GetNanoSeconds() << "ns.");
        }
        else
        {
            NS_LOG_DEBUG("RoCEv2 socket receives duplicate packet "
                         << psn << " at " << Simulator::Now().GetNanoSeconds() << "ns.");
        }
    }
    if (m_retxMode == RoCEv2RetxMode::IRN)
    {
        if (psn >= m_expectedPsn && m_buffer.find(psn) == m_buffer.end())
        {
            m_buffer.emplace(psn, DcbRxBufferItem(ipv4, roce, payload));
        }
        else
        {
            NS_LOG_DEBUG("RoCEv2 socket receives duplicate packet "
                         << psn << " at " << Simulator::Now().GetNanoSeconds() << "ns.");
        }
    }

    // Check and forward the in order packets
    while (m_buffer.find(m_expectedPsn) != m_buffer.end())
    {
        const DcbRxBufferItem& item = m_buffer.at(m_expectedPsn);
        m_forwardCb(item.m_payload, item.m_ipv4H, item.m_roceH.GetSrcQP(), m_forwardInterface);
        m_buffer.erase(m_expectedPsn);
        // TODO No warp around check
        m_expectedPsn++;
    }
}

uint32_t
DcbRxBuffer::GetExpectedPsn() const
{
    return m_expectedPsn;
}

DcbRxBuffer::DcbRxBufferItem::DcbRxBufferItem(Ipv4Header ipv4H,
                                              RoCEv2Header roceH,
                                              Ptr<Packet> payload)
    : m_ipv4H(ipv4H),
      m_roceH(roceH),
      m_payload(payload)
{
}

NS_OBJECT_ENSURE_REGISTERED(RoCEv2SocketState);

TypeId
RoCEv2SocketState::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RoCEv2SocketState")
                            .SetParent<Object>()
                            .SetGroupName("Dcb")
                            .AddConstructor<RoCEv2SocketState>()
                            .AddAttribute("MinRateRatio",
                                          "Socket's minimum rate ratio",
                                          DoubleValue(1e-3),
                                          MakeDoubleAccessor(&RoCEv2SocketState::m_minRateRatio),
                                          MakeDoubleChecker<double>());
    return tid;
}

RoCEv2SocketState::RoCEv2SocketState()
    : m_rateRatio(1.),
      m_cwnd(UINT64_MAX >> 1)
{
}

RoCEv2Socket::Stats::Stats()
    : nTotalSizePkts(0),
      nTotalSizeBytes(0),
      nTotalSentPkts(0),
      nTotalSentBytes(0),
      nTotalDeliverPkts(0),
      nTotalDeliverBytes(0),
      nRetxCount(0),
      tStart(Time(0)),
      tFinish(Time(0)),
      tFct(Time(0)),
      overallFlowRate(DataRate(0)),
      ccStats(nullptr)
{
    // Retrieve the global config values
    BooleanValue bv;
    if (GlobalValue::GetValueByNameFailSafe("detailedSenderStats", bv))
        bDetailedSenderStats = bv.Get();
    else
        bDetailedSenderStats = false;
    if (GlobalValue::GetValueByNameFailSafe("detailedRetxStats", bv))
        bDetailedRetxStats = bv.Get();
    else
        bDetailedRetxStats = false;
}

std::shared_ptr<RoCEv2Socket::Stats>
RoCEv2Socket::GetStats() const
{
    m_stats->CollectAndCheck();

    // Get the cc's statistics
    m_stats->ccStats = m_ccOps->GetStats();

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

    tFct = tFinish - tStart;
    // Calculate the overallFlowRate
    overallFlowRate = DataRate(nTotalSizeBytes * 8.0 / tFct.GetSeconds());
}

void
RoCEv2Socket::Stats::RecordCcRate(DataRate rate)
{
    if (bDetailedSenderStats)
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
    if (bDetailedSenderStats)
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
    if (bDetailedSenderStats)
    {
        vRecvEcn.push_back(Simulator::Now());
    }
}

void
RoCEv2Socket::Stats::RecordSentPkt(uint32_t size)
{
    if (bDetailedSenderStats)
    {
        vSentPkt.push_back(std::make_pair(Simulator::Now(), size));
    }
}

void
RoCEv2Socket::Stats::RecordSentPsn(uint32_t psn)
{
    if (bDetailedRetxStats)
    {
        vSentPsn.push_back(std::make_pair(Simulator::Now(), psn));
    }
}

void
RoCEv2Socket::Stats::RecordAckedPsn(uint32_t psn)
{
    if (bDetailedRetxStats)
    {
        vAckedPsn.push_back(std::make_pair(Simulator::Now(), psn));
    }
}

void
RoCEv2Socket::Stats::RecordExpectedPsn(uint32_t psn)
{
    if (bDetailedRetxStats)
    {
        vExpectedPsn.push_back(std::make_pair(Simulator::Now(), psn));
    }
}

NS_OBJECT_ENSURE_REGISTERED(IrnHeader);

TypeId
IrnHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::IrnHeader")
                            .SetParent<Header>()
                            .SetGroupName("Dcb")
                            .AddConstructor<IrnHeader>();
    return tid;
}

TypeId
IrnHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

IrnHeader::IrnHeader()
    : m_ackedPsn(0)
{
}

uint32_t
IrnHeader::GetSerializedSize(void) const
{
    return 4;
}

void
IrnHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU32(m_ackedPsn);
}

uint32_t
IrnHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_ackedPsn = i.ReadU32();
    return GetSerializedSize();
}

void
IrnHeader::Print(std::ostream& os) const
{
    os << "IrnHeader acked PSN " << m_ackedPsn;
}

void
IrnHeader::SetAckedPsn(uint32_t psn)
{
    m_ackedPsn = psn;
}

uint32_t
IrnHeader::GetAckedPsn() const
{
    return m_ackedPsn;
}

} // namespace ns3
