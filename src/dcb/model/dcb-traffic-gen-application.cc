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

#include "dcb-traffic-gen-application.h"

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

NS_LOG_COMPONENT_DEFINE("DcbTrafficGenApplication");
NS_OBJECT_ENSURE_REGISTERED(DcbTrafficGenApplication);

// Define static variables
std::vector<Time> DcbTrafficGenApplication::m_recoveryInterval;
std::vector<std::set<uint32_t>> DcbTrafficGenApplication::m_recoveryIntervalNodes;
bool DcbTrafficGenApplication::m_isRecoveryInit = true;

std::vector<Time> DcbTrafficGenApplication::m_fileRequestInterval;
std::vector<std::set<uint32_t>> DcbTrafficGenApplication::m_fileRequestIntervalNodes;
std::vector<uint32_t> DcbTrafficGenApplication::m_fileRequestIntervalDest;
bool DcbTrafficGenApplication::m_isFileRequestInit = true;

std::vector<std::vector<Time>> DcbTrafficGenApplication::m_ringIntervals;
bool DcbTrafficGenApplication::m_isRingInit = true;

std::vector<bool> DcbTrafficGenApplication::m_bgFlowFinished;

TypeId
DcbTrafficGenApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DcbTrafficGenApplication")
            .SetParent<DcbBaseApplication>()
            .SetGroupName("Dcb")
            .AddConstructor<DcbTrafficGenApplication>()
            // .AddAttribute ("Protocol",
            //                "The type of protocol to use. This should be "
            //                "a subclass of ns3::SocketFactory",
            //                TypeIdValue (UdpSocketFactory::GetTypeId ()),
            //                MakeTypeIdAccessor (&DcbTrafficGenApplication::m_socketTid),
            //                // This should check for SocketFactory as a parent
            //                MakeTypeIdChecker ())
            .AddAttribute("Interpolate",
                          "Enable interpolation from CDF",
                          BooleanValue(true),
                          MakeBooleanAccessor(&DcbTrafficGenApplication::m_interpolate),
                          MakeBooleanChecker())
            .AddAttribute("RecoveryNodeRatio",
                          "The ratio of recovery nodes",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&DcbTrafficGenApplication::m_recoveryNodeRatio),
                          MakeDoubleChecker<double>())
            .AddAttribute("FileRequestNodesNum",
                          "The number of file request nodes",
                          UintegerValue(1),
                          MakeUintegerAccessor(&DcbTrafficGenApplication::m_fileRequestNodesNum),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RingGroupNodesNum",
                          "The number of ring group nodes",
                          UintegerValue(1),
                          MakeUintegerAccessor(&DcbTrafficGenApplication::m_ringGroupNodesNum),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute(
                "TrafficPattern",
                "The traffic pattern",
                EnumValue(DcbTrafficGenApplication::TrafficPattern::CDF),
                MakeEnumAccessor(&DcbTrafficGenApplication::m_trafficPattern),
                MakeEnumChecker(DcbTrafficGenApplication::TrafficPattern::CDF,
                                "CDF",
                                DcbTrafficGenApplication::TrafficPattern::SEND_ONCE,
                                "SendOnce",
                                DcbTrafficGenApplication::TrafficPattern::FIXED_SIZE,
                                "FixedSize",
                                DcbTrafficGenApplication::TrafficPattern::RECOVERY,
                                "Recovery",
                                DcbTrafficGenApplication::TrafficPattern::FILE_REQUEST,
                                "FileRequest",
                                DcbTrafficGenApplication::TrafficPattern::RING,
                                "Ring",
                                DcbTrafficGenApplication::TrafficPattern::CHECKPOINT,
                                "Checkpoint"))
            .AddAttribute("TrafficSizeBytes",
                          "The size of the traffic, average size of CDF if it is use",
                          UintegerValue(0),
                          MakeUintegerAccessor(&DcbTrafficGenApplication::m_trafficSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TrafficLoad",
                          "The load of the traffic",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&DcbTrafficGenApplication::m_trafficLoad),
                          MakeDoubleChecker<double>())
            .AddAttribute("TrafficType",
                          "The type of traffic",
                          EnumValue(DcbTrafficGenApplication::TrafficType::NONE),
                          MakeEnumAccessor(&DcbTrafficGenApplication::m_trafficType),
                          MakeEnumChecker(DcbTrafficGenApplication::TrafficType::FOREGROUND,
                                          "Foreground",
                                          DcbTrafficGenApplication::TrafficType::BACKGROUND,
                                          "Background",
                                          DcbTrafficGenApplication::TrafficType::NONE,
                                          "None"))
            .AddAttribute("DestinationNode",
                          "The destination node index",
                          UintegerValue(0),
                          MakeUintegerAccessor(&DcbTrafficGenApplication::m_destNode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DestFixed",
                          "If the dest is fixed",
                          BooleanValue(false),
                          MakeBooleanAccessor(&DcbTrafficGenApplication::m_isFixedDest),
                          MakeBooleanChecker())
            .AddAttribute("FixedFlowArrive",
                          "If the flow arrive interval is fixed",
                          BooleanValue(false),
                          MakeBooleanAccessor(&DcbTrafficGenApplication::m_isFixedFlowArrive),
                          MakeBooleanChecker())
            .AddAttribute("FlowType",
                          "The type of flow",
                          StringValue(""),
                          MakeStringAccessor(&DcbTrafficGenApplication::SetFlowType),
                          MakeStringChecker());
    return tid;
}

DcbTrafficGenApplication::DcbTrafficGenApplication()
    : DcbBaseApplication(),
      m_fixedFlowArriveInterval(Time(0))
{
    NS_LOG_FUNCTION(this);

    m_stats = std::make_shared<Stats>();
}

DcbTrafficGenApplication::DcbTrafficGenApplication(Ptr<DcTopology> topology, uint32_t nodeIndex)
    : DcbBaseApplication(),
      m_fixedFlowArriveInterval(Time(0))
{
    NS_LOG_FUNCTION(this);

    m_stats = std::make_shared<Stats>();
}

void
DcbTrafficGenApplication::InitMembers()
{
    NS_LOG_FUNCTION(this);

    m_flowArriveTimeRng = CreateObject<ExponentialRandomVariable>();
    m_flowArriveTimeRng->SetAntithetic(true);

    if (!m_isFixedDest)
    {
        m_hostIndexRng = m_topology->CreateRamdomHostChooser();
    }
}

DcbTrafficGenApplication::~DcbTrafficGenApplication()
{
    NS_LOG_FUNCTION(this);
}

void
DcbTrafficGenApplication::CalcTrafficParameters()
{
    // Calculate the mean interval of the flow
    double flowMeanInterval =
        m_trafficSize * 8 / (m_socketLinkRate.GetBitRate() * m_trafficLoad) * 1e9; // ns

    switch (m_trafficPattern)
    {
    case TrafficPattern::CDF:
    case TrafficPattern::FIXED_SIZE:
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
DcbTrafficGenApplication::GenerateTraffic()
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
                                &DcbTrafficGenApplication::ScheduleAForegroundFlow,
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
        }
        break;
    case TrafficPattern::SEND_ONCE:
        // Schedule only once
        ScheduleNextFlow(Simulator::Now());
        break;
    case TrafficPattern::FIXED_SIZE:
        // Schedule all flows in the beginning
        for (Time t = Simulator::Now() + GetNextFlowArriveInterval(); t < m_stopTime;
             t += GetNextFlowArriveInterval())
        {
            ScheduleNextFlow(t);
        }
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

    if (m_protoGroup == ProtocolGroup::RoCEv2)
    {
        tracer_extension::RegisterTraceFCT(this);
    }
}

uint32_t
DcbTrafficGenApplication::GetDestinationNode() const
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

void
DcbTrafficGenApplication::ScheduleNextFlow(const Time& startTime)
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
                        &DcbTrafficGenApplication::SendNextPacket,
                        this,
                        flow);

    // If the trafficType is BACKGROUND, we need to add a false to m_bgFlowFinished
    if (m_trafficType == TrafficType::BACKGROUND)
    {
        m_bgFlowFinished.push_back(false);
    }
}

void
DcbTrafficGenApplication::CreateRecovery()
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
    std::cout << m_recoveryIntervalNodes.size() << std::endl;
}

void
DcbTrafficGenApplication::CreateFileRequest()
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
DcbTrafficGenApplication::CreateRing()
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
DcbTrafficGenApplication::SchedulCheckpointFlow()
{
    NS_LOG_FUNCTION(this);
    // Set the dest to the checkpoint node, ie, the neighbor of the node
    // the neighbor node is like (0 1) (2 3), we have our node index, so we can get the neighbor
    uint32_t destNode = m_nodeIndex % 2 == 0 ? m_nodeIndex + 1 : m_nodeIndex - 1;
    Ptr<Socket> socket = CreateNewSocket(destNode);

    ScheduleNextFlow(Simulator::Now());
}

void
DcbTrafficGenApplication::ScheduleAForegroundFlow()
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
                        &DcbTrafficGenApplication::ScheduleAForegroundFlow,
                        this);
}

void
DcbTrafficGenApplication::SetFlowMeanArriveInterval(double interval)
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
DcbTrafficGenApplication::SetFlowCdf(const TraceCdf& cdf)
{
    NS_LOG_FUNCTION(this);

    m_flowSizeRng = CreateObject<EmpiricalRandomVariable>();
    // Enable interpolation from CDF
    m_flowSizeRng->SetAttribute("Interpolate", BooleanValue(m_interpolate));

    // Set the CDF and calculate the mean
    double res = 0.;
    auto [ls, lp] = cdf[0];
    for (auto [sz, prob] : cdf)
    {
        m_flowSizeRng->CDF(sz, prob);
        res += (sz + ls) / 2.0 * (prob - lp);
        ls = sz;
        lp = prob;
    }
    m_trafficSize = res;
}

inline Time
DcbTrafficGenApplication::GetNextFlowArriveInterval() const
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
DcbTrafficGenApplication::GetNextFlowSize() const
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
DcbTrafficGenApplication::SetFlowType(std::string flowType)
{
    m_stats->appFlowType = flowType;
}

void
DcbTrafficGenApplication::FlowCompletes(Ptr<UdpBasedSocket> socket)
{
    // Call the base class method
    DcbBaseApplication::FlowCompletes(socket);

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

DcbTrafficGenApplication::Stats::Stats()
    : DcbBaseApplication::Stats(),
      isCollected(false)
{
}

std::shared_ptr<DcbBaseApplication::Stats>
DcbTrafficGenApplication::GetStats() const
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
DcbTrafficGenApplication::Stats::CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows)
{
    // Avoid collecting stats twice
    if (isCollected)
    {
        return;
    }
    isCollected = true;

    // Collect the statistics
    DcbBaseApplication::Stats::CollectAndCheck(flows);
}
} // namespace ns3
