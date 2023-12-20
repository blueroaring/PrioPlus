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

#include "dcb-trace-application.h"

#include "dcb-net-device.h"
#include "rocev2-dcqcn.h"
#include "rocev2-l4-protocol.h"
#include "rocev2-socket.h"
#include "udp-based-socket.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/fatal-error.h"
#include "ns3/global-value.h"
#include "ns3/integer.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/loopback-net-device.h"
#include "ns3/node.h"
#include "ns3/packet-socket-address.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tracer-extension.h"
#include "ns3/type-id.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TraceApplication");
NS_OBJECT_ENSURE_REGISTERED(TraceApplication);

TypeId
TraceApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TraceApplication")
            .SetParent<Application>()
            .SetGroupName("Dcb")
            // .AddAttribute ("Protocol",
            //                "The type of protocol to use. This should be "
            //                "a subclass of ns3::SocketFactory",
            //                TypeIdValue (UdpSocketFactory::GetTypeId ()),
            //                MakeTypeIdAccessor (&TraceApplication::m_socketTid),
            //                // This should check for SocketFactory as a parent
            //                MakeTypeIdChecker ())
            .AddAttribute("CongestionType",
                          "Socket' congestion control type.",
                          TypeIdValue(RoCEv2Dcqcn::GetTypeId()),
                          MakeTypeIdAccessor(&TraceApplication::m_congestionTypeId),
                          MakeTypeIdChecker())
            .AddAttribute("SendOnce",
                          "Send one flow from start time to stop time if true",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TraceApplication::m_sendOnce),
                          MakeBooleanChecker())
            .AddTraceSource("FlowComplete",
                            "Trace when a flow completes.",
                            MakeTraceSourceAccessor(&TraceApplication::m_flowCompleteTrace),
                            "ns3::TracerExtension::FlowTracedCallback");
    return tid;
}

TraceApplication::TraceApplication(Ptr<DcTopology> topology,
                                   uint32_t nodeIndex,
                                   int32_t destIndex /* = -1 */)
    : m_totBytes(0),
      m_headerSize(8 + 20 + 14 + 2),
      m_enableSend(true),
      m_enableReceive(true),
      m_topology(topology),
      m_nodeIndex(nodeIndex),
      m_node(topology->GetNode(nodeIndex).nodePtr),
      m_ecnEnabled(true),
      m_staticFlowArriveInterval(Time(0)),
      m_destNode(destIndex),
      m_destAddr(InetSocketAddress("0.0.0.0", 0)),
      m_sendOnce(false)
{
    NS_LOG_FUNCTION(this);

    m_stats = std::make_shared<Stats>();

    if (destIndex < 0)
    {
        m_hostIndexRng = topology->CreateRamdomHostChooser();
    }
    // Time::SetResolution (Time::Unit::PS); // improve resolution
    InitForRngs();
}

TraceApplication::TraceApplication(Ptr<DcTopology> topology,
                                   Ptr<Node> node,
                                   InetSocketAddress destAddr)
    : m_totBytes(0),
      m_headerSize(8 + 20 + 14 + 2),
      m_enableSend(true),
      m_enableReceive(true),
      m_topology(topology),
      m_nodeIndex(node->GetId()),
      m_node(node),
      m_ecnEnabled(true),
      m_destNode(-1),
      m_destAddr(destAddr),
      m_sendOnce(false)
{
    NS_LOG_FUNCTION(this);

    m_stats = std::make_shared<Stats>();

    // Time::SetResolution (Time::Unit::PS); // improve resolution
    InitForRngs();
}

void
TraceApplication::InitForRngs()
{
    NS_LOG_FUNCTION(this);

    m_flowArriveTimeRng = CreateObject<ExponentialRandomVariable>();
    m_flowArriveTimeRng->SetAntithetic(true);

    m_flowSizeRng = CreateObject<EmpiricalRandomVariable>();
    // Enable interpolation from CDF
    m_flowSizeRng->SetAttribute("Interpolate", BooleanValue(true));

    if (DynamicCast<DcbNetDevice>(GetOutboundNetDevice()) != nullptr)
    {
        m_socketLinkRate = DynamicCast<DcbNetDevice>(GetOutboundNetDevice())->GetDataRate();
    }
    else
    {
        StringValue sdv;
        if (GlobalValue::GetValueByNameFailSafe("defaultRate", sdv))
            m_socketLinkRate = DataRate(sdv.Get());
        else
            NS_FATAL_ERROR(
                "traceApp's socket is not bound to a DcbNetDevice and no default rate is "
                "set.");
    }
}

TraceApplication::~TraceApplication()
{
    NS_LOG_FUNCTION(this);
}

// int64_t
// TraceApplication::AssignStreams (int64_t stream)
// {
//   NS_LOG_FUNCTION (this << stream);
// }

void
TraceApplication::SetProtocolGroup(ProtocolGroup protoGroup)
{
    m_protoGroup = protoGroup;
    if (protoGroup == ProtocolGroup::TCP)
    {
        NS_FATAL_ERROR("TCP not fully supported."); // TODO
        m_socketTid = TcpSocketFactory::GetTypeId();
        m_headerSize = 20 + 20 + 14 + 2;
    }
}

void
TraceApplication::SetInnerUdpProtocol(std::string innerTid)
{
    SetInnerUdpProtocol(TypeId(innerTid));
}

void
TraceApplication::SetInnerUdpProtocol(TypeId innerTid)
{
    NS_LOG_FUNCTION(this << innerTid);
    if (m_protoGroup != ProtocolGroup::RoCEv2)
    {
        NS_FATAL_ERROR("Inner UDP protocol should be used together with RoCEv2 protocol group.");
    }
    m_socketTid = UdpBasedSocketFactory::GetTypeId();
    Ptr<Node> node = GetNode();
    Ptr<UdpBasedSocketFactory> socketFactory = node->GetObject<UdpBasedSocketFactory>();
    if (socketFactory)
    {
        // Where set Rocev2Socket
        m_headerSize = socketFactory->AddUdpBasedProtocol(node, GetOutboundNetDevice(), innerTid);
    }
    else
    {
        NS_FATAL_ERROR("Application cannot use inner-UDP protocol because UdpBasedL4Protocol and "
                       "UdpBasedSocketFactory is not bound to node correctly.");
    }
}

void
TraceApplication::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_enableReceive)
    {
        if (m_protoGroup == ProtocolGroup::RoCEv2)
        {
            // crate a special socket to act as the receiver
            m_receiverSocket = DynamicCast<RoCEv2Socket>(
                Socket::CreateSocket(GetNode(), UdpBasedSocketFactory::GetTypeId()));
            m_receiverSocket->SetCcOps(m_congestionTypeId);
            m_receiverSocket->BindToNetDevice(GetOutboundNetDevice());
            m_receiverSocket->BindToLocalPort(RoCEv2L4Protocol::DefaultServicePort());
            m_receiverSocket->ShutdownSend();
            // Set stop time to max to avoid receiver socket close
            m_receiverSocket->SetStopTime(Time::Max());
            m_receiverSocket->SetRecvCallback(MakeCallback(&TraceApplication::HandleRead, this));
        }
    }

    if (m_enableSend)
    {
        if (m_sendOnce)
        { // Schedule only once
            ScheduleOnlyOnce();
        }
        else
        {
            // Schedule all flows in the beginning
            for (Time t = Simulator::Now() + GetNextFlowArriveInterval(); t < m_stopTime;
                 t += GetNextFlowArriveInterval())
            {
                ScheduleNextFlow(t);
            }
            if (m_protoGroup == ProtocolGroup::RoCEv2)
            {
                tracer_extension::RegisterTraceFCT(this);
            }
        }
    }
}

void
TraceApplication::StopApplication(void)
{
    NS_LOG_FUNCTION(this);
}

uint32_t
TraceApplication::GetDestinationNode() const
{
    NS_LOG_FUNCTION(this);

    if (m_destNode < 0)
    { // randomly send to a host
        uint32_t destNode;
        do
        {
            destNode = m_hostIndexRng->GetInteger();
        } while (destNode == m_nodeIndex);
        return destNode;
    }
    else
    {
        return m_destNode;
    }
}

InetSocketAddress
TraceApplication::NodeIndexToAddr(uint32_t destNode) const
{
    NS_LOG_FUNCTION(this);

    uint32_t portNum;
    switch (m_protoGroup)
    {
    case ProtocolGroup::RAW_UDP:
        NS_FATAL_ERROR("UDP port has not been chosen");
        break;
    case ProtocolGroup::TCP:
        portNum = 200; // FIXME not raw
        break;
    case ProtocolGroup::RoCEv2:
        portNum = RoCEv2L4Protocol::DefaultServicePort();
        break;
    }

    // 0 interface is LoopbackNetDevice
    Ipv4Address ipv4Addr = m_topology->GetInterfaceOfNode(destNode, 1).GetAddress();
    return InetSocketAddress(ipv4Addr, portNum);
}

Ptr<Socket>
TraceApplication::CreateNewSocket(uint32_t destNode)
{
    NS_LOG_FUNCTION(this);
    InetSocketAddress destAddr = NodeIndexToAddr(destNode);
    return CreateNewSocket(destAddr, m_congestionTypeId);
}


Ptr<Socket>
TraceApplication::CreateNewSocket(InetSocketAddress destAddr)
{
    NS_LOG_FUNCTION(this);
    return CreateNewSocket(destAddr, m_congestionTypeId);
}

Ptr<Socket>
TraceApplication::CreateNewSocket(uint32_t destNode, TypeId congestionTypeId)
{
    NS_LOG_FUNCTION(this);
    InetSocketAddress destAddr = NodeIndexToAddr(destNode);
    return CreateNewSocket(destAddr, congestionTypeId);
}

Ptr<Socket>
TraceApplication::CreateNewSocket(InetSocketAddress destAddr, TypeId congestionTypeId)
{
    NS_LOG_FUNCTION(this);

    // The InstanceTyoeId of socket is RoCEv2Socket
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), m_socketTid);

    if (m_protoGroup == ProtocolGroup::TCP)
    {
        socket->SetAttribute("SegmentSize",
                             UintegerValue(1440)); // default TCP segment is too small
    }
    else
    {
        Ptr<UdpBasedSocket> udpBasedSocket = DynamicCast<UdpBasedSocket>(socket);
        if (udpBasedSocket)
        {
            udpBasedSocket->SetFlowCompleteCallback(
                MakeCallback(&TraceApplication::FlowCompletes, this));
            Ptr<RoCEv2Socket> roceSocket = DynamicCast<RoCEv2Socket>(udpBasedSocket);
            if (roceSocket)
            {
                // Set stop time to max to avoid sender socket's timer close
                roceSocket->SetCcOps(congestionTypeId);
                roceSocket->SetStopTime(Time::Max());
            }
        }
    }
    socket->BindToNetDevice(GetOutboundNetDevice());

    int ret = socket->Bind();
    if (ret == -1)
    {
        NS_FATAL_ERROR("Failed to bind socket");
    }

    if (m_ecnEnabled)
    {
        // The low 2-bits of TOS field is ECN field.
        // The Tos of a flow is setted here.
        destAddr.SetTos(Ipv4Header::EcnType::ECN_ECT1);
    }
    ret = socket->Connect(destAddr);
    if (ret == -1)
    {
        NS_FATAL_ERROR("Socket connection failed");
    }
    socket->SetAllowBroadcast(false);
    // m_socket->SetConnectCallback (MakeCallback (&TraceApplication::ConnectionSucceeded, this),
    //                               MakeCallback (&TraceApplication::ConnectionFailed, this));
    socket->SetRecvCallback(MakeCallback(&TraceApplication::HandleRead, this));
    return socket;
}

void
TraceApplication::ScheduleNextFlow(const Time& startTime)
{
    uint32_t destNode = 0;
    Ptr<Socket> socket;

    if (m_destAddr.GetPort() == 0)
    {
        // The dest addr is not set, select a destination node
        destNode = GetDestinationNode();
        socket = CreateNewSocket(destNode);
    }
    else
    {
        socket = CreateNewSocket(m_destAddr);
    }

    uint64_t size = GetNextFlowSize();

    // XXX If use dest addr, the dest node is not set
    Flow* flow = new Flow(size, startTime, destNode, socket);
    SetFlowIdentifier(flow, socket);
    m_flows.emplace(socket, flow); // used when flow completes
    Simulator::Schedule(startTime - Simulator::Now(),
                        &TraceApplication::SendNextPacket,
                        this,
                        flow);
}

/**
 * Many duplicate code here, but if we want to reuse ScheduleNextFlow(Time), we need to both modify
 * GetNextFlowArriveInterval() and GetNextFlowSize(), which place one feature in two places.
 * So just duplicate the code here...
 */
void
TraceApplication::ScheduleOnlyOnce()
{
    NS_LOG_FUNCTION(this);
    if (!m_sendOnce)
    {
        return;
    }

    // Set the dest and create a socket to it
    uint32_t destNode = 0;
    Ptr<Socket> socket;
    if (m_destAddr.GetPort() == 0)
    {
        // The dest addr is not set, select a destination node
        destNode = GetDestinationNode();
        socket = CreateNewSocket(destNode);
    }
    else
    {
        socket = CreateNewSocket(m_destAddr);
    }

    // Calculate the flow size according to the start time, stop time, and link rate
    Time flowDuration = m_stopTime - Simulator::Now();
    uint64_t flowSize = flowDuration.GetSeconds() * m_socketLinkRate.GetBitRate() / 8;

    // Create a flow and schedule it's send
    Flow* flow = new Flow(flowSize, Simulator::Now(), destNode, socket);
    SetFlowIdentifier(flow, socket);
    m_flows.emplace(socket, flow); // used when flow completes
    Simulator::Schedule(Time(0), &TraceApplication::SendNextPacket, this, flow);
}

void
TraceApplication::SendNextPacket(Flow* flow)
{
    const uint32_t packetSize = std::min(flow->remainBytes, MSS);
    Ptr<Packet> packet = Create<Packet>(packetSize);
    int actual = flow->socket->Send(packet);
    if (actual == static_cast<int>(packetSize))
    {
        m_totBytes += packetSize;
        Time txTime = m_socketLinkRate.CalculateBytesTxTime(packetSize + m_headerSize);
        /**
         * Note that the send will continue even the application is stopped.
         * We just want the appliaction to schedule the flows, but not determine when to stop the
         * simulation.
         */
        if (flow->remainBytes > MSS)
        { // Schedule next packet
            flow->remainBytes -= MSS;
            Simulator::Schedule(txTime, &TraceApplication::SendNextPacket, this, flow);
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
        Simulator::Schedule(txTime, &TraceApplication::SendNextPacket, this, flow);
    }
}

void
TraceApplication::SetFlowMeanArriveInterval(double interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_flowArriveTimeRng->SetAttribute("Mean", DoubleValue(interval)); // in microseconds
}

void
TraceApplication::SetFlowMeanArriveInterval(double interval, bool staticFlowInterval)
{
    NS_LOG_FUNCTION(this << interval);
    if (staticFlowInterval)
    {
        m_staticFlowArriveInterval = Time(MicroSeconds(interval));
        m_flowArriveTimeRng = nullptr;
    }
    else
    {
        m_staticFlowArriveInterval = Time(0);
        SetFlowMeanArriveInterval(interval);
    }
}

void
TraceApplication::SetFlowCdf(const TraceCdf& cdf)
{
    NS_LOG_FUNCTION(this);
    for (auto [sz, prob] : cdf)
    {
        m_flowSizeRng->CDF(sz, prob);
    }
}

inline Time
TraceApplication::GetNextFlowArriveInterval() const
{
    if (m_staticFlowArriveInterval != Time(0))
    {
        return m_staticFlowArriveInterval;
    }
    else if (m_flowArriveTimeRng != nullptr)
    {
        return Time(MicroSeconds(m_flowArriveTimeRng->GetInteger()));
    }
    else
    {
        NS_FATAL_ERROR("Flow arrival interval is not set");
    }
}

inline uint32_t
TraceApplication::GetNextFlowSize() const
{
    return m_flowSizeRng->GetInteger();
}

void
TraceApplication::SetEcnEnabled(bool enabled)
{
    NS_LOG_FUNCTION(this << enabled);
    m_ecnEnabled = enabled;
}

void
TraceApplication::SetDestAddr(InetSocketAddress destAddr)
{
    NS_LOG_FUNCTION(this << destAddr);
    m_destAddr = destAddr;
}

void
TraceApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
TraceApplication::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
}

void
TraceApplication::HandleRead(Ptr<Socket> socket)
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
    }
}

void
TraceApplication::FlowCompletes(Ptr<UdpBasedSocket> socket)
{
    auto p = m_flows.find(socket);
    if (p == m_flows.end())
    {
        NS_FATAL_ERROR("Cannot find socket in this application on node "
                       << Simulator::GetContext());
    }
    Flow* flow = p->second;
    m_flowCompleteTrace(Simulator::GetContext(),
                        flow->destNode,
                        socket->GetSrcPort(),
                        socket->GetDstPort(),
                        flow->totalBytes,
                        flow->startTime,
                        Simulator::Now());
}

void
TraceApplication::SetSendEnabled(bool enabled)
{
    m_enableSend = enabled;
}

void
TraceApplication::SetReceiveEnabled(bool enabled)
{
    m_enableReceive = enabled;
}

Ptr<NetDevice>
TraceApplication::GetOutboundNetDevice()
{
    // We do not use GetNode ()->GetDevice (0) as it is inavlid when the application is created
    Ptr<NetDevice> boundDev = m_node->GetDevice(0);
    if (DynamicCast<LoopbackNetDevice>(boundDev) != nullptr)
    {
        // Try to get the second net device as the first one may be loopback
        boundDev = m_node->GetDevice(1);
    }
    return boundDev;
}

void
TraceApplication::SetFlowIdentifier(Flow* flow, Ptr<Socket> socket)
{
    if (m_protoGroup == ProtocolGroup::RoCEv2)
    {
        Ptr<RoCEv2Socket> roceSocket = DynamicCast<RoCEv2Socket>(socket);
        if (roceSocket != nullptr)
        {
            Ipv4Address srcAddr = roceSocket->GetLocalAddress();
            Ipv4Address dstAddr = roceSocket->GetPeerAddress();
            uint32_t srcQP = roceSocket->GetSrcPort();
            uint32_t dstQP = roceSocket->GetDstPort();
            flow->flowIdentifier = FlowIdentifier(srcAddr, dstAddr, srcQP, dstQP);
        }
        else
        {
            NS_FATAL_ERROR("Socket is not a RoCEv2 socket");
        }
    }
    else
    {
        NS_FATAL_ERROR("Flow identifier is not supported for this protocol group");
    }
}

TraceApplication::Stats::Stats()
    : isCollected(false),
      nTotalSizePkts(0),
      nTotalSizeBytes(0),
      nTotalSentPkts(0),
      nTotalSentBytes(0),
      nTotalDeliverPkts(0),
      nTotalDeliverBytes(0),
      nRetxCount(0),
      tStart(Time::Max()),
      tFinish(Time::Min()),
      overallRate(DataRate(0))
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

std::shared_ptr<TraceApplication::Stats>
TraceApplication::GetStats() const
{
    m_stats->CollectAndCheck(m_flows);
    return m_stats;
}

void
TraceApplication::Stats::CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows)
{
    // Avoid collecting stats twice
    if (isCollected)
    {
        return;
    }
    isCollected = true;

    // Collect the statistics
    for (auto [socket, flow] : flows)
    {
        Ptr<RoCEv2Socket> roceSocket = DynamicCast<RoCEv2Socket>(socket);
        if (roceSocket != nullptr)
        {
            auto roceStats = roceSocket->GetStats();
            nTotalSizePkts += roceStats->nTotalSizePkts;
            nTotalSizeBytes += roceStats->nTotalSizeBytes;
            nTotalSentPkts += roceStats->nTotalSentPkts;
            nTotalSentBytes += roceStats->nTotalSentBytes;
            nTotalDeliverPkts += roceStats->nTotalDeliverPkts;
            nTotalDeliverBytes += roceStats->nTotalDeliverBytes;
            nRetxCount += roceStats->nRetxCount;
            tStart = std::min(tStart, roceStats->tStart);
            tFinish = std::max(tFinish, roceStats->tFinish);

            mFlowStats[flow->flowIdentifier] = roceStats;
            vFlowStats.push_back(roceStats);
        }
    }
    // Calculate the overall rate
    overallRate = DataRate(nTotalSizeBytes * 8.0 / (tFinish - tStart).GetSeconds());
}

} // namespace ns3
