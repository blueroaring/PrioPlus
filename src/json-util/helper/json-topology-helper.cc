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

#include "json-topology-helper.h"

#include <fstream>
#include <iostream>
#include <map>
#include <time.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("JsonTopologyHelper");

namespace json_util
{

/***** Utilities about topology *****/
static void AssignAddress(const Ptr<Node> node, const Ptr<NetDevice> device);

static DcTopology::TopoNode
CreateOneHost()
{
    const Ptr<Node> host = CreateObject<Node>();

    DcbHostStackHelper hostStack;
    hostStack.Install(host);

    return {.type = DcTopology::TopoNode::NodeType::HOST, .nodePtr = host};
}

static Ptr<DcbNetDevice>
AddPortToHost(const Ptr<Node> host)
{
    // Create device for this port
    const Ptr<DcbNetDevice> dev = CreateObject<DcbNetDevice>();
    dev->SetAddress(Mac48Address::Allocate());

    // Create queue on the device
    ObjectFactory queueFactory;
    queueFactory.SetTypeId(DropTailQueue<Packet>::GetTypeId());
    Ptr<Queue<Packet>> queue = queueFactory.Create<Queue<Packet>>();
    // TODO: Why so high??
    // queue->SetMaxSize({QueueSizeUnit::PACKETS, std::numeric_limits<uint32_t>::max()});
    queue->SetMaxSize(QueueSize("10p"));
    dev->SetQueue(queue);

    host->AddDevice(dev);

    AssignAddress(host, dev);

    return dev;
}

static Ptr<DcbNetDevice>
AddPortToSwitch(const Ptr<Node> sw)
{
    // Create a net device for this port
    Ptr<DcbNetDevice> dev = CreateObject<DcbNetDevice>();
    dev->SetAddress(Mac48Address::Allocate());

    // Create queue on the device
    ObjectFactory queueFactory;
    queueFactory.SetTypeId(DropTailQueue<Packet>::GetTypeId());
    Ptr<Queue<Packet>> queue = queueFactory.Create<Queue<Packet>>();
    queue->SetMaxSize(QueueSize("10p"));
    dev->SetQueue(queue);

    sw->AddDevice(dev);

    return dev;
}

static DcTopology::TopoNode
CreateOneSwitch()
{
    const Ptr<Node> sw = CreateObject<Node>();

    // Add protocol to switch
    DcbSwitchStackHelper switchStack;
    switchStack.InstallSwitchProtos(sw);

    // Basic configurations
    // sw->SetEcmpSeed (m_ecmpSeed);
    return {.type = DcTopology::TopoNode::NodeType::SWITCH, .nodePtr = sw};
}

void
ConfigureSwitch(boost::json::object& configObj, Ptr<Node> sw)
{
    // Get switch config from config json object
    boost::json::object switchConfigObj =
        configObj["topologyConfig"].get_object().find("switch")->value().get_object();
    // Add protocol to each port, must be called after
    // 1. all ports has been added to switch
    // 2. the switch protocols have been installed
    DcbSwitchStackHelper switchStack;
    switchStack.SetBufferSize(
        QueueSize(std::string(switchConfigObj.find("bufferSize")->value().as_string().c_str())));
    switchStack.InstallPortsProtos(sw);

    // Configure the port and its queues according to the config object
    boost::json::object switchPortConfigObj =
        configObj["topologyConfig"].get_object().find("switchPort")->value().get_object();
    boost::json::object switchPortQueueConfigObj =
        configObj["topologyConfig"].get_object().find("switchPortQueue")->value().get_object();

    // Configure all ports one by one
    // Note that the first device is the loopback device install by Ipv4L3Protocol
    for (uint32_t portIdx = 1; portIdx < sw->GetNDevices(); portIdx++)
    {
        uint32_t nPortQueues = switchPortConfigObj.find("queues")->value().as_int64();
        if (nPortQueues != 0 && nPortQueues != 8)
        {
            NS_FATAL_ERROR("The port configuration should have 8 queues or 0 queue, not "
                           << nPortQueues);
        }

        // Assign IPv4 Address to this port
        AssignAddress(sw, sw->GetDevice(portIdx));

        // Configure PFC
        bool pfcEnabled = switchPortConfigObj.find("pfcEnabled")->value().as_bool();
        if (pfcEnabled)
        {
            std::string sPfcReserve(
                switchPortQueueConfigObj.find("pfcReserve")->value().as_string().c_str());
            std::string sPfcXon(
                switchPortQueueConfigObj.find("pfcXon")->value().as_string().c_str());

            DcbPfcPortConfig pfcConfig;
            for (uint32_t qi = 0; qi < nPortQueues; qi++)
            {
                const uint32_t reserve = QueueSize(sPfcReserve).GetValue();
                const uint32_t xon = QueueSize(sPfcXon).GetValue();
                pfcConfig.AddQueueConfig(qi, reserve, xon);
            }
            DcbFcHelper::InstallPFCtoNodePort(sw, portIdx, pfcConfig);
        }

        // Configure ECN
        bool ecnEnabled = switchPortConfigObj.find("ecnEnabled")->value().as_bool();
        if (ecnEnabled)
        {
            std::string sEcnKMin(
                switchPortQueueConfigObj.find("ecnKMin")->value().as_string().c_str());
            std::string sEcnKMax(
                switchPortQueueConfigObj.find("ecnKMax")->value().as_string().c_str());
            double ecnPMax = switchPortQueueConfigObj.find("ecnPMax")->value().as_double();
            std::string sSwitchBufferSize = configObj["topologyConfig"]
                                                .get_object()
                                                .find("switch")
                                                ->value()
                                                .get_object()
                                                .find("bufferSize")
                                                ->value()
                                                .get_string()
                                                .c_str();

            ObjectFactory factory;
            factory.SetTypeId("ns3::FifoQueueDiscEcn");
            Ptr<QueueDisc> dev = DynamicCast<DcbNetDevice>(sw->GetDevice(portIdx))->GetQueueDisc();

            for (uint32_t qi = 0; qi < nPortQueues; qi++)
            {
                uint32_t ecnKMin = QueueSize(sEcnKMin).GetValue();
                uint32_t ecnKMax = QueueSize(sEcnKMax).GetValue();
                // ecnConfig.AddQueueConfig (qi, ecnKMin, ecnKMax, ecnPMax);

                Ptr<FifoQueueDiscEcn> qd = factory.Create<FifoQueueDiscEcn>();
                qd->Initialize();
                qd->ConfigECN(ecnKMin, ecnKMax, ecnPMax);
                // TODO Why this value?
                qd->SetMaxSize(QueueSize(sSwitchBufferSize));
                Ptr<PausableQueueDiscClass> c = CreateObject<PausableQueueDiscClass>();
                c->SetQueueDisc(qd);
                dev->AddQueueDiscClass(c);
            }
        }
    }
}

static void
InstallLink(boost::json::object& configObj, Ptr<DcbNetDevice> dev1, Ptr<DcbNetDevice> dev2)
{
    boost::json::object linkConfigObj =
        configObj["topologyConfig"].get_object().find("link")->value().get_object();
    std::string rate = linkConfigObj.find("rate")->value().as_string().c_str();
    std::string delay = linkConfigObj.find("delay")->value().as_string().c_str();
    dev1->SetAttribute("DataRate", DataRateValue(DataRate(rate)));
    dev2->SetAttribute("DataRate", DataRateValue(DataRate(rate)));

    Ptr<DcbChannel> channel = CreateObject<DcbChannel>();
    channel->SetAttribute("Delay", TimeValue(Time(delay)));

    dev1->Attach(channel);
    dev2->Attach(channel);
}

static void
AssignAddress(const Ptr<Node> node, const Ptr<NetDevice> device)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    NS_ASSERT_MSG(ipv4,
                  "Ipv4AddressHelper::Assign(): NetDevice is associated"
                  " with a node without IPv4 stack installed -> fail "
                  "(maybe need to use DcbStackHelper?)");

    int32_t interface = ipv4->GetInterfaceForDevice(device);
    if (interface == -1)
    {
        interface = ipv4->AddInterface(device);
    }
    NS_ASSERT_MSG(interface >= 0,
                  "Ipv4AddressHelper::Assign(): "
                  "Interface index not found");

    Ipv4Address addr = Ipv4AddressGenerator::NextAddress("255.0.0.0");
    Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(addr, "255.0.0.0");
    ipv4->AddAddress(interface, ipv4Addr);
    ipv4->SetMetric(interface, 1);
    ipv4->SetUp(interface);
}

[[maybe_unused]] void
LogIpAddress(const Ptr<const DcTopology> topology)
{
    int ni = 0;
    for (const auto& node : *topology)
    {
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        const int nintf = ipv4->GetNInterfaces();
        for (int i = 0; i < nintf; i++)
        {
            const int naddr = ipv4->GetNAddresses(i);
            for (int j = 0; j < naddr; j++)
            {
                std::string name =
                    (node.type == DcTopology::TopoNode::NodeType::HOST) ? "host " : "switch ";
                NS_LOG_DEBUG(name << ni << " intf " << i << " addr " << j << " "
                                  << ipv4->GetAddress(i, j));
            }
        }
        ni++;
    }
}

[[maybe_unused]] static void
LogAllRoutes(const Ptr<const DcTopology> topology)
{
    int ni = 0;
    for (const auto& node : *topology)
    {
        Ptr<GlobalRouter> router = node->GetObject<GlobalRouter>();
        Ptr<Ipv4GlobalRouting> route = router->GetRoutingProtocol();
        std::string name =
            (node.type == DcTopology::TopoNode::NodeType::HOST) ? "host " : "switch ";
        if (!route)
        {
            NS_LOG_DEBUG(name << ni << " does not have global routing");
            ni++;
            continue;
        }
        const int n = route->GetNRoutes();
        for (int i = 0; i < n; i++)
        {
            Ipv4RoutingTableEntry entry = route->GetRoute(i);
            NS_LOG_DEBUG(name << ni << " " << entry);
        }
        ni++;
    }
}

[[maybe_unused]] static void
LogGlobalRouting(const Ptr<DcTopology> topology)
{
    for (DcTopology::SwitchIterator sw = topology->switches_begin(); sw != topology->switches_end();
         sw++)
    {
        Ptr<Ipv4ListRouting> lrouting =
            DynamicCast<Ipv4ListRouting>((*sw)->GetObject<Ipv4>()->GetRoutingProtocol());
        int16_t prio;
        Ptr<Ipv4GlobalRouting> glb =
            DynamicCast<Ipv4GlobalRouting>(lrouting->GetRoutingProtocol(0, prio));
        uint32_t n = glb->GetNRoutes();
        for (uint32_t i = 0; i < n; i++)
        {
            NS_LOG_DEBUG("global: " << *glb->GetRoute(i));
        }
        UintegerValue b;
        glb->GetAttribute("RandomEcmpRouting", b);
        NS_LOG_DEBUG("ecmp: " << b.Get());
    }
}

Ptr<DcTopology>
BuildTopology(boost::json::object& configObj)
{
    std::string topoFilename =
        configObj["inputFile"].get_object().find("topo")->value().as_string().c_str();
    std::ifstream topof;
    topof.open(topoFilename);
    Ipv4AddressGenerator::Init("10.0.0.0", "255.0.0.0", "0.0.0.1");

    // The first line
    uint32_t node_num, switch_num, link_num;
    topof >> node_num >> switch_num >> link_num;

    // Creat a topology to manage all nodes and links
    Ptr<DcTopology> topology = CreateObject<DcTopology>(node_num);

    // Read the idxes of switches and build the node type vector
    // In the node type vector, 0 means host, 1 means switch
    std::vector<uint32_t> node_type(node_num, 0);
    for (uint32_t i = 0; i < switch_num; i++)
    {
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 1;
    }

    // Create nodes according to node type list
    NodeContainer n;
    // NS_LOG_INFO ("Create nodes.");
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (node_type[i] == 0)
        {
            // Host
            DcTopology::TopoNode host = CreateOneHost();
            topology->InstallNode(i, std::move(host));
        }
        else
        {
            DcTopology::TopoNode host = CreateOneSwitch();
            topology->InstallNode(i, std::move(host));
        }
    }

    Ipv4GlobalRoutingHelper globalRouting;
    // Ipv4ListRoutingHelper list;
    // list.Add (globalRouting, 1);

    // Explicitly create the channels required by the topology.
    // NS_LOG_INFO ("Create channels.");
    for (uint32_t i = 0; i < link_num; i++)
    {
        // type is not used for now
        // which can be used to build heterogeneous topology if needed
        uint32_t src, dst, type;
        topof >> src >> dst >> type;

        Ptr<Node> srcNode = topology->GetNode(src).nodePtr;
        Ptr<Node> dstNode = topology->GetNode(dst).nodePtr;
        topology->InstallLink(src, dst); // as metadata

        if (topology->GetNode(src).type != DcTopology::TopoNode::NodeType::SWITCH &&
            topology->GetNode(dst).type != DcTopology::TopoNode::NodeType::SWITCH)
        {
            NS_FATAL_ERROR("Do not allow link between two hosts");
        }

        // Add port to the nodes
        std::vector<Ptr<Node>> vNodes = {srcNode, dstNode};
        std::vector<Ptr<DcbNetDevice>> vDevs;
        for (auto node : vNodes)
        {
            if (topology->IsHost(node->GetId()))
            {
                Ptr<DcbNetDevice> dev = AddPortToHost(node);
                vDevs.push_back(dev);
            }
            else
            {
                Ptr<DcbNetDevice> dev = AddPortToSwitch(node);
                vDevs.push_back(dev);
            }
        }
        // Install link
        InstallLink(configObj, vDevs[0], vDevs[1]);
    }

    // After building topology, configure switches
    for (uint32_t nodeIdx = 0; nodeIdx < node_num; nodeIdx++)
    {
        if (topology->IsSwitch(nodeIdx))
        {
            ConfigureSwitch(configObj, topology->GetNode(nodeIdx).nodePtr);
        }
        else if (topology->IsHost(nodeIdx))
        {
            DcbHostStackHelper hostStack;
            hostStack.InstallPortsProtos(topology->GetNode(nodeIdx).nodePtr);
        }
    }

    // Calculate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // p2p.EnablePcapAll ("pcap");
    LogIpAddress(topology);
    LogAllRoutes(topology);

    topof.close();

    return topology;
}

} // namespace json_util

} // namespace ns3
