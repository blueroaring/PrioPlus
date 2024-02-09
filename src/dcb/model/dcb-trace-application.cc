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
#include "ns3/enum.h"
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
#include "ns3/udp-socket.h"
#include "ns3/uinteger.h"

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TraceApplication");
NS_OBJECT_ENSURE_REGISTERED(TraceApplication);

// Define static variables
std::vector<Time> TraceApplication::m_recoveryInterval;
std::vector<std::set<uint32_t>> TraceApplication::m_recoveryIntervalNodes;
bool TraceApplication::m_isRecoveryInit = true;

std::vector<bool> TraceApplication::m_bgFlowFinished;

std::vector<Time> TraceApplication::m_fileRequestInterval;
std::vector<std::set<uint32_t>> TraceApplication::m_fileRequestIntervalNodes;
std::vector<uint32_t> TraceApplication::m_fileRequestIntervalDest;
bool TraceApplication::m_isFileRequestInit = true;

std::vector<std::vector<Time>> TraceApplication::m_ringIntervals;
bool TraceApplication::m_isRingInit = true;

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
            .AddAttribute("Interpolate",
                          "Enable interpolation from CDF",
                          BooleanValue(true),
                          MakeBooleanAccessor(&TraceApplication::m_interpolate),
                          MakeBooleanChecker())
            .AddAttribute("RecoveryNodeRatio",
                          "The ratio of recovery nodes",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&TraceApplication::m_recoveryNodeRatio),
                          MakeDoubleChecker<double>())
            .AddAttribute("FileRequestNodesNum",
                          "The number of file request nodes",
                          UintegerValue(1),
                          MakeUintegerAccessor(&TraceApplication::m_fileRequestNodesNum),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RingGroupNodesNum",
                          "The number of ring group nodes",
                          UintegerValue(1),
                          MakeUintegerAccessor(&TraceApplication::m_ringGroupNodesNum),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TrafficPattern",
                          "The traffic pattern",
                          EnumValue(TraceApplication::TrafficPattern::CDF),
                          MakeEnumAccessor(&TraceApplication::m_trafficPattern),
                          MakeEnumChecker(TraceApplication::TrafficPattern::CDF,
                                          "CDF",
                                          TraceApplication::TrafficPattern::SEND_ONCE,
                                          "SendOnce",
                                          TraceApplication::TrafficPattern::RECOVERY,
                                          "Recovery",
                                          TraceApplication::TrafficPattern::FILE_REQUEST,
                                          "FileRequest",
                                          TraceApplication::TrafficPattern::RING,
                                          "Ring",
                                          TraceApplication::TrafficPattern::CHECKPOINT,
                                          "Checkpoint"))
            .AddAttribute("TrafficSizeBytes",
                          "The size of the traffic, average size of CDF if it is use",
                          UintegerValue(0),
                          MakeUintegerAccessor(&TraceApplication::m_trafficSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TrafficLoad",
                          "The load of the traffic",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&TraceApplication::m_trafficLoad),
                          MakeDoubleChecker<double>())
            .AddAttribute("TrafficType",
                          "The type of traffic",
                          EnumValue(TraceApplication::TrafficType::NONE),
                          MakeEnumAccessor(&TraceApplication::m_trafficType),
                          MakeEnumChecker(TraceApplication::TrafficType::FOREGROUND,
                                          "Foreground",
                                          TraceApplication::TrafficType::BACKGROUND,
                                          "Background",
                                          TraceApplication::TrafficType::NONE,
                                          "None"))
            .AddAttribute("DestinationNode",
                          "The destination node index",
                          UintegerValue(0),
                          MakeUintegerAccessor(&TraceApplication::m_destNode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DestFixed",
                          "If the dest is fixed",
                          BooleanValue(false),
                          MakeBooleanAccessor(&TraceApplication::m_isFixedDest),
                          MakeBooleanChecker())
            .AddAttribute("SendEnabled",
                          "Enable sending",
                          BooleanValue(true),
                          MakeBooleanAccessor(&TraceApplication::m_enableSend),
                          MakeBooleanChecker())
            .AddAttribute("FlowType",
                          "The type of flow",
                          StringValue(""),
                          MakeStringAccessor(&TraceApplication::SetFlowType),
                          MakeStringChecker())
            .AddTraceSource("FlowComplete",
                            "Trace when a flow completes.",
                            MakeTraceSourceAccessor(&TraceApplication::m_flowCompleteTrace),
                            "ns3::TracerExtension::FlowTracedCallback");
    return tid;
}

TraceApplication::TraceApplication(Ptr<DcTopology> topology, uint32_t nodeIndex)
    : m_totBytes(0),
      m_headerSize(8 + 20 + 14 + 2),
      m_enableSend(true),
      m_enableReceive(true),
      m_topology(topology),
      m_nodeIndex(nodeIndex),
      m_node(topology->GetNode(nodeIndex).nodePtr),
      m_ecnEnabled(true),
      m_fixedFlowArriveInterval(Time(0))
{
    NS_LOG_FUNCTION(this);

    m_stats = std::make_shared<Stats>();

    // Time::SetResolution (Time::Unit::PS); // improve resolution
    InitSocketLineRate();
}

void
TraceApplication::InitForRngs()
{
    NS_LOG_FUNCTION(this);

    m_flowArriveTimeRng = CreateObject<ExponentialRandomVariable>();
    m_flowArriveTimeRng->SetAntithetic(true);

    if (!m_isFixedDest)
    {
        m_hostIndexRng = m_topology->CreateRamdomHostChooser();
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
        NS_LOG_WARN("TCP not fully supported.");
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
        m_headerSize = socketFactory->AddUdpBasedProtocol(node, GetOutboundNetDevice(), innerTid);
    }
    else
    {
        NS_FATAL_ERROR("Application cannot use inner-UDP protocol because UdpBasedL4Protocol and "
                       "UdpBasedSocketFactory is not bound to node correctly.");
    }
}

void
TraceApplication::InitSocketLineRate()
{
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

void
TraceApplication::SetupReceiverSocket()
{
    NS_LOG_FUNCTION(this);
    if (m_protoGroup == ProtocolGroup::RoCEv2)
    {
        // Check if a receiver socket has been created
        Ptr<RoCEv2L4Protocol> roceL4 = GetNode()->GetObject<RoCEv2L4Protocol>();
        if (!roceL4->CheckLocalPortExist(RoCEv2L4Protocol::DefaultServicePort()))
        {
            // crate a special socket to act as the receiver
            m_receiverSocket = Socket::CreateSocket(GetNode(), m_socketTid);
            Ptr<RoCEv2Socket> roceSocket = DynamicCast<RoCEv2Socket>(m_receiverSocket);
            roceSocket->SetCcOps(m_congestionTypeId, m_ccAttributes);
            roceSocket->BindToNetDevice(GetOutboundNetDevice());
            roceSocket->BindToLocalPort(RoCEv2L4Protocol::DefaultServicePort());
            roceSocket->ShutdownSend();
            // Set stop time to max to avoid receiver socket close
            roceSocket->SetStopTime(Time::Max());
            roceSocket->SetRecvCallback(MakeCallback(&TraceApplication::HandleRead, this));
        }
    }
    // TCP
    else if (m_protoGroup == ProtocolGroup::TCP)
    {
        m_receiverSocket = Socket::CreateSocket(GetNode(), m_socketTid);
        if (m_receiverSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 200)) == -1)
        {
            NS_FATAL_ERROR("Failed to bind TCP socket");
        }
        m_receiverSocket->Listen();
        m_receiverSocket->ShutdownSend();
        // Set stop time to max to avoid receiver socket close
        m_receiverSocket->SetRecvCallback(MakeCallback(&TraceApplication::HandleRead, this));
        m_receiverSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address&>(),
                                            MakeCallback(&TraceApplication::HandleTcpAccept, this));
        m_receiverSocket->SetCloseCallbacks(
            MakeCallback(&TraceApplication::HandleTcpPeerClose, this),
            MakeCallback(&TraceApplication::HandleTcpPeerError, this));
    }
    else
    {
        NS_FATAL_ERROR("Receive is not supported for this protocol group");
    }
}

void
TraceApplication::CalcTrafficParameters()
{
    // Calculate the mean interval of the flow
    double flowMeanInterval =
        m_trafficSize * 8 / (m_socketLinkRate.GetBitRate() * m_trafficLoad) * 1e9; // ns

    switch (m_trafficPattern)
    {
    case TrafficPattern::CDF:
        SetFlowMeanArriveInterval(flowMeanInterval);
        break;

    case TrafficPattern::RECOVERY:
        // In each file request event, the traffic generated is (traffic size *
        // m_topology->GetNHosts() * m_recoveryNodeRatio) And the generate process is only done at
        // one node, so the mean interval should be flowMeanInterval * m_recoveryNodeRatio
        SetFlowMeanArriveInterval(flowMeanInterval * m_recoveryNodeRatio);
        break;

    case TrafficPattern::FILE_REQUEST:
        // In each file request event, the traffic generated is traffic size * m_fileRequestNodesNum
        // And the generate process is only done at one node, so the mean interval should be
        // flowMeanInterval * m_fileRequestNodesNum / m_topology->GetNHosts()
        SetFlowMeanArriveInterval(flowMeanInterval * m_fileRequestNodesNum /
                                  m_topology->GetNHosts());
        break;

    case TrafficPattern::RING:
        // In each ring event, the traffic generated is traffic size * m_topology->GetNHosts()
        // And the generate process is only done at one node, so the mean interval should be
        // flowMeanInterval * m_topology->GetNHosts() / m_topology->GetNHosts()
        SetFlowMeanArriveInterval(flowMeanInterval);
        break;

    default:
        break;
    }

    // Calculate the traffic size according to start, stop time and load if not set
    if (m_trafficSize == 0)
    {
        NS_ASSERT_MSG(m_stopTime > m_startTime,
                      "Stop time should be meaningful if size is not set.");
        m_trafficSize = m_trafficLoad * (m_stopTime - m_startTime).GetSeconds() *
                        m_socketLinkRate.GetBitRate() / 8;
    }
}

void
TraceApplication::GenerateTraffic()
{
    // Sanity check
    NS_ASSERT_MSG(
        m_trafficType != TrafficType::FOREGROUND ||
            (m_trafficType == TrafficType::FOREGROUND && m_trafficPattern == TrafficPattern::CDF),
        "If the traffic type is foreground, the traffic pattern should be CDF. ");

    switch (m_trafficPattern)
    {
    case TrafficPattern::CDF:
        if (m_trafficType == TrafficType::FOREGROUND)
        {
            // Schedule the next foreground flow
            Simulator::Schedule(GetNextFlowArriveInterval(),
                                &TraceApplication::ScheduleAForegroundFlow,
                                this);
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
        break;
    case TrafficPattern::SEND_ONCE:
        // Schedule only once
        ScheduleNextFlow(Simulator::Now());
        break;
    case TrafficPattern::RECOVERY:
        if (m_isRecoveryInit)
        {
            CreateRecovery();
            m_isRecoveryInit = false;
        }
        // traverse all recovery intervals
        for (uint32_t i = 0; i < m_recoveryInterval.size(); i++)
        { // check if current node is in the current recovery nodesset
            if (m_recoveryIntervalNodes[i].find(m_nodeIndex) != m_recoveryIntervalNodes[i].end())
            {
                ScheduleNextFlow(m_recoveryInterval[i]);
            }
        }
        break;
    case TrafficPattern::FILE_REQUEST:
        m_fileRequestIndex = 0;
        if (m_isFileRequestInit)
        {
            CreateFileRequest();
            m_isFileRequestInit = false;
        }
        // traverse all file request intervals
        for (uint32_t i = 0; i < m_fileRequestInterval.size(); i++)
        { // check if current node is in the current file request nodesset
            if (m_fileRequestIntervalNodes[i].find(m_nodeIndex) !=
                m_fileRequestIntervalNodes[i].end())
            {
                ScheduleNextFlow(m_fileRequestInterval[i]);
            }
            m_fileRequestIndex++;
        }
        break;
    case TrafficPattern::RING:
        if (m_isRingInit)
        {
            CreateRing();
            m_isRingInit = false;
        }
        // Calculate the current application's group index using current index and group size
        m_ringGroupIndex = m_nodeIndex / m_ringGroupNodesNum;
        // Calculate the current application's destination node
        m_ringDestNode =
            (m_nodeIndex + 1) % m_ringGroupNodesNum + m_ringGroupIndex * m_ringGroupNodesNum;
        // Traverse curent group's intervals
        for (uint32_t i = 0; i < m_ringIntervals[m_ringGroupIndex].size(); i++)
        {
            ScheduleNextFlow(m_ringIntervals[m_ringGroupIndex][i]);
        }
        break;
    case TrafficPattern::CHECKPOINT:
        SchedulCheckpointFlow();
        break;
    default:
        NS_FATAL_ERROR("Traffic pattern can not be recognized.");
        break;
    }
}

void
TraceApplication::StartApplication(void)
{
    NS_LOG_FUNCTION(this);

    if (m_enableReceive)
    {
        SetupReceiverSocket();
    }

    if (m_enableSend)
    {
        InitForRngs();
        CalcTrafficParameters();
        GenerateTraffic();
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

    if (!m_isFixedDest)
    {
        switch (m_trafficPattern)
        {
        case TrafficPattern::FILE_REQUEST:
            return m_fileRequestIntervalDest[m_fileRequestIndex];
        case TrafficPattern::RING:
            return m_ringDestNode;
        default:
            // randomly send to a host
            uint32_t destNode;
            do
            {
                destNode = m_hostIndexRng->GetInteger();
            } while (destNode == m_nodeIndex);
            return destNode;
        }
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

    uint32_t portNum = 0;
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
    return CreateNewSocket(destAddr);
}

Ptr<Socket>
TraceApplication::CreateNewSocket(InetSocketAddress destAddr)
{
    NS_LOG_FUNCTION(this);

    // The InstanceTyoeId of socket is RoCEv2Socket
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), m_socketTid);
    // bool isRoce = false;

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

    if (m_protoGroup == ProtocolGroup::RoCEv2)
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
                roceSocket->SetCcOps(m_congestionTypeId, m_ccAttributes);
                roceSocket->SetStopTime(Time::Max());
                // Set socket attributes in m_appAttributes
                for (const auto& [name, value] : m_socketAttributes)
                {
                    roceSocket->SetAttribute(name, *value);
                }

                // calculate baseRTT and ack's payload size
                uint32_t ackHeaderSize =
                    m_headerSize + 4; // ack header has AETHeader, which is 4 bytes

                m_headerSize +=
                    roceSocket->GetCcOps()
                        ->GetExtraHeaderSize(); // header may be added by congestion control
                ackHeaderSize += roceSocket->GetCcOps()
                                     ->GetExtraAckSize(); // ack may be added by congestion control

                roceSocket->GetSocketState()->SetPacketSize(MSS + m_headerSize);
                roceSocket->GetSocketState()->SetMss(MSS);
                // ack size should be at least 64B
                uint32_t ackPayload = ackHeaderSize < 64 ? (64 - ackHeaderSize) : 0;

                Ipv4Address srcIpAddr = m_topology->GetInterfaceOfNode(m_nodeIndex, 1).GetAddress();
                Ipv4Address destIpAddr = destAddr.GetIpv4();

                roceSocket->SetBaseRttNOneWayDelay(m_topology->GetHops(srcIpAddr, destIpAddr),
                                                   m_topology->GetDelay(srcIpAddr, destIpAddr),
                                                   MSS + m_headerSize,
                                                   ackPayload + ackHeaderSize);
                // Should not call SetReady here as the flow may not start now
            }
        }
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

    destNode = GetDestinationNode();
    socket = CreateNewSocket(destNode);

    uint64_t size = GetNextFlowSize();

    // XXX If use dest addr, the dest node is not set
    Flow* flow = new Flow(size, startTime, destNode, socket);
    SetFlowIdentifier(flow, socket);
    m_flows.emplace(socket, flow); // used when flow completes
    Simulator::Schedule(startTime - Simulator::Now(),
                        &TraceApplication::SendNextPacket,
                        this,
                        flow);

    // If the trafficType is BACKGROUND, we need to add a false to m_bgFlowFinished
    if (m_trafficType == TrafficType::BACKGROUND)
    {
        m_bgFlowFinished.push_back(false);
    }
}

void
TraceApplication::CreateRecovery()
{
    // Check m_isRecovryInit in case
    if (!m_isRecoveryInit)
    {
        return;
    }

    // Schedule all flows in the beginning
    for (Time t = Simulator::Now() + GetNextFlowArriveInterval(); t < m_stopTime;
         t += GetNextFlowArriveInterval())
    {
        m_recoveryInterval.push_back(t);

        uint32_t nodeNum = m_topology->GetNHosts() * m_recoveryNodeRatio;
        m_recoveryIntervalNodes.push_back(std::set<uint32_t>());

        // Select recovery node
        while (m_recoveryIntervalNodes.back().size() < nodeNum)
        {
            uint32_t node = m_hostIndexRng->GetInteger();
            if (m_recoveryIntervalNodes.back().find(node) == m_recoveryIntervalNodes.back().end())
            {
                m_recoveryIntervalNodes.back().insert(node);
            }
        }
    }
}

void
TraceApplication::CreateFileRequest()
{
    // Check m_isFileRequestInit in case
    if (!m_isFileRequestInit)
    {
        return;
    }

    // Schedule all flows in the beginning
    for (Time t = Simulator::Now() + GetNextFlowArriveInterval(); t < m_stopTime;
         t += GetNextFlowArriveInterval())
    {
        m_fileRequestInterval.push_back(t);

        m_fileRequestIntervalNodes.push_back(std::set<uint32_t>());
        // Select file request destination node
        uint32_t destNode = m_hostIndexRng->GetInteger();
        m_fileRequestIntervalDest.push_back(destNode);

        // Select file request node
        while (m_fileRequestIntervalNodes.back().size() < m_fileRequestNodesNum)
        {
            uint32_t node = m_hostIndexRng->GetInteger();
            if (m_fileRequestIntervalNodes.back().find(node) ==
                    m_fileRequestIntervalNodes.back().end() &&
                node != destNode)
            {
                m_fileRequestIntervalNodes.back().insert(node);
            }
        }
    }
}

void
TraceApplication::CreateRing()
{
    if (!m_isRingInit)
    {
        return;
    }
    uint32_t totalHosts = m_topology->GetNHosts();
    if (totalHosts % m_ringGroupNodesNum != 0)
    {
        NS_FATAL_ERROR(
            "The number of hosts should be a multiple of the number of ring group nodes");
    }
    uint32_t groupNum = totalHosts / m_ringGroupNodesNum;
    for (uint32_t i = 0; i < groupNum; i++)
    {
        m_ringIntervals.push_back(std::vector<Time>());
        for (Time t = Simulator::Now() + GetNextFlowArriveInterval(); t < m_stopTime;
             t += GetNextFlowArriveInterval())
        {
            m_ringIntervals[i].push_back(t);
        }
    }
}

void
TraceApplication::SchedulCheckpointFlow()
{
    NS_LOG_FUNCTION(this);
    // Set the dest to the checkpoint node, ie, the neighbor of the node
    // the neighbor node is like (0 1) (2 3), we have our node index, so we can get the neighbor
    uint32_t destNode = m_nodeIndex % 2 == 0 ? m_nodeIndex + 1 : m_nodeIndex - 1;
    Ptr<Socket> socket = CreateNewSocket(destNode);

    ScheduleNextFlow(Simulator::Now());
}

void
TraceApplication::ScheduleAForegroundFlow()
{
    NS_LOG_FUNCTION(this);
    // Stop condition: all the background flows are finished, ie, m_bgFlowFinished is all true
    // Use std::all_of to check
    if (std::all_of(m_bgFlowFinished.begin(), m_bgFlowFinished.end(), [](bool b) { return b; }))
    {
        return;
    }

    ScheduleNextFlow(Simulator::Now());

    // Schedule the next foreground flow
    Simulator::Schedule(GetNextFlowArriveInterval(),
                        &TraceApplication::ScheduleAForegroundFlow,
                        this);
}

void
TraceApplication::SendNextPacket(Flow* flow)
{
    // SetReady for RoCEv2Socket, this will start the congestion control
    // Note that for rocev2Socket, this function will only be called once
    Ptr<RoCEv2Socket> roceSocket = DynamicCast<RoCEv2Socket>(flow->socket);
    if (roceSocket)
    {
        roceSocket->GetCcOps()->SetReady();
        Ptr<Packet> packet = Create<Packet>(flow->remainBytes);
        flow->socket->Send(packet);
        m_totBytes += flow->remainBytes;
        flow->remainBytes = 0;
        return;
    }

    while (flow->remainBytes != 0)
    {
        const uint32_t packetSize = std::min(flow->remainBytes, MSS);
        Ptr<Packet> packet = Create<Packet>(packetSize);
        int actual = flow->socket->Send(packet);
        if (actual == static_cast<int>(packetSize))
        {
            m_totBytes += packetSize;
            flow->remainBytes -= actual;

            Ptr<UdpSocket> udpSock = DynamicCast<UdpSocket>(flow->socket);
            if (udpSock != nullptr)
            {
                // For UDP socket, pacing the sending at application layer
                Time txTime = m_socketLinkRate.CalculateBytesTxTime(packetSize + m_headerSize);
                Simulator::Schedule(txTime, &TraceApplication::SendNextPacket, this, flow);
                return;
            }
        }
        else
        {
            // Typically this is because TCP socket's txBuffer is full, retry later.
            Time txTime = m_socketLinkRate.CalculateBytesTxTime(packetSize + m_headerSize);
            Simulator::Schedule(txTime, &TraceApplication::SendNextPacket, this, flow);
            return;
        }
    }

    // flow sending completes for RoCEv2Socket
    Ptr<UdpBasedSocket> udpSock = DynamicCast<UdpBasedSocket>(flow->socket);
    if (udpSock)
    {
        udpSock->FinishSending();
    }
}

void
TraceApplication::SetFlowMeanArriveInterval(double interval)
{
    NS_LOG_FUNCTION(this << interval);
    if (m_isFixedFlowArrive)
    {
        m_fixedFlowArriveInterval = Time(NanoSeconds(interval));
        m_flowArriveTimeRng = nullptr;
    }
    else
    {
        m_fixedFlowArriveInterval = Time(0);
        m_flowArriveTimeRng->SetAttribute("Mean", DoubleValue(interval)); // in ns
    }
}

void
TraceApplication::SetFlowCdf(const TraceCdf& cdf)
{
    NS_LOG_FUNCTION(this);

    m_flowSizeRng = CreateObject<EmpiricalRandomVariable>();
    // Enable interpolation from CDF
    m_flowSizeRng->SetAttribute("Interpolate", BooleanValue(m_interpolate));
    for (auto [sz, prob] : cdf)
    {
        m_flowSizeRng->CDF(sz, prob);
    }
}

inline Time
TraceApplication::GetNextFlowArriveInterval() const
{
    if (m_isFixedFlowArrive)
    {
        return m_fixedFlowArriveInterval;
    }
    else if (m_flowArriveTimeRng != nullptr)
    {
        return Time(NanoSeconds(m_flowArriveTimeRng->GetInteger()));
    }
    else
    {
        NS_FATAL_ERROR("Flow arrival interval is not set");
    }
}

inline uint32_t
TraceApplication::GetNextFlowSize() const
{
    switch (m_trafficPattern)
    {
    case TrafficPattern::CDF:
        return m_flowSizeRng->GetInteger();

    default:
        return m_trafficSize;
    }
}

void
TraceApplication::SetEcnEnabled(bool enabled)
{
    NS_LOG_FUNCTION(this << enabled);
    m_ecnEnabled = enabled;
}

void
TraceApplication::SetCcOpsAttributes(
    const std::vector<RoCEv2CongestionOps::CcOpsConfigPair_t>& configs)
{
    NS_LOG_FUNCTION(this);
    m_ccAttributes = configs;
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

    // If the trafficType is BACKGROUND, we need to turn a false to true in m_bgFlowFinished
    if (m_trafficType == TrafficType::BACKGROUND)
    {
        // Find the first false
        for (uint32_t i = 0; i < m_bgFlowFinished.size(); i++)
        {
            if (!m_bgFlowFinished[i])
            {
                m_bgFlowFinished[i] = true;
                break;
            }
        }
    }
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

void
TraceApplication::SetSocketAttributes(
    const std::vector<TraceApplication::ConfigEntry_t>& socketAttributes)
{
    m_socketAttributes = socketAttributes;
}

// TODO TCP callback Handler
void
TraceApplication::HandleTcpAccept(Ptr<Socket> socket, const Address& from)
{
}

void
TraceApplication::HandleTcpPeerClose(Ptr<Socket> socket)
{
    // Here to calculate the fct
}

void
TraceApplication::HandleTcpPeerError(Ptr<Socket> socket)
{
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
        NS_LOG_WARN("Flow identifier is not supported for this protocol group");
    }
}

void
TraceApplication::SetFlowType(std::string flowType)
{
    m_stats->appFlowType = flowType;
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
    if (m_stats->appFlowType.empty())
    {
        switch (m_trafficType)
        {
        case TrafficType::FOREGROUND:
            m_stats->appFlowType = "foreground";
            break;
        case TrafficType::BACKGROUND:
            m_stats->appFlowType = "background";
            break;
        default:
            m_stats->appFlowType = "none";
            break;
        }
    }
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
