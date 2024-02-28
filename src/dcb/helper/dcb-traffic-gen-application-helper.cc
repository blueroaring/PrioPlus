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

#include "dcb-traffic-gen-application-helper.h"

#include "ns3/dc-topology.h"
#include "ns3/dcb-net-device.h"
#include "ns3/dcb-traffic-gen-application.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/real-time-application.h"
#include "ns3/rocev2-l4-protocol.h"

#include <fstream>

namespace ns3
{

DcbTrafficGenApplicationHelper::DcbTrafficGenApplicationHelper(Ptr<DcTopology> topo)
    : m_topology(topo),
      m_cdf(nullptr),
      m_flowMeanInterval(0.),
      m_destNode(-1),
      m_destAddr(InetSocketAddress("0.0.0.0", 0)),
      m_sendEnabled(true),
      m_load(0),
      m_staticFlowInterval(false),
      m_sendOnce(false),
      m_startTime(Time(0)),
      m_stopTime(Time(0)),
      m_realTimeApp(false)
{
}

void
DcbTrafficGenApplicationHelper::SetProtocolGroup(DcbTrafficGenApplication::ProtocolGroup protoGroup)
{
    m_protoGroup = protoGroup;
}

void
DcbTrafficGenApplicationHelper::SetCdf(std::unique_ptr<DcbTrafficGenApplication::TraceCdf> cdf)
{
    m_cdf = std::move(cdf);
}

void
DcbTrafficGenApplicationHelper::SetLoad(Ptr<const DcbNetDevice> dev, double load)
{
    SetLoad(dev->GetDataRate(), load);
}

void
DcbTrafficGenApplicationHelper::SetLoad(DataRate rate, double load)
{
    m_load = load;
    CalcLoad(rate);
}

void
DcbTrafficGenApplicationHelper::SetLoad(double load)
{
    m_load = load;
}

void
DcbTrafficGenApplicationHelper::CalcLoad(DataRate rate)
{
    NS_ASSERT_MSG(m_cdf, "Must set CDF to DcbTrafficGenApplicationHelper before setting load.");
    NS_ASSERT_MSG(m_load >= 0. && m_load <= 1., "Load shoud be between 0 and 1.");
    double mean = CalculateCdfMeanSize(m_cdf.get());
    if (m_load <= 1e-6)
    {
        m_sendEnabled = false;
    }
    else
    {
        m_sendEnabled = true;
        m_flowMeanInterval = mean * 8 / (rate.GetBitRate() * m_load) * 1e6; // us
    }
}

void
DcbTrafficGenApplicationHelper::SetSendEnabled(bool enabled)
{
    m_sendEnabled = enabled;
}

void
DcbTrafficGenApplicationHelper::SetDestination(int32_t dest)
{
    m_destNode = dest;
}

void
DcbTrafficGenApplicationHelper::SetDestination(InetSocketAddress dest)
{
    m_destAddr = dest;
}

void
DcbTrafficGenApplicationHelper::SetStaticFlowInterval(bool staticFlowInterval)
{
    m_staticFlowInterval = staticFlowInterval;
}

void
DcbTrafficGenApplicationHelper::SetSendOnce(bool sendOnce)
{
    m_sendOnce = sendOnce;
}

void
DcbTrafficGenApplicationHelper::SetStartAndStopTime(Time start, Time stop)
{
    m_startTime = start;
    m_stopTime = stop;
}

void
DcbTrafficGenApplicationHelper::SetStartTime(Time start)
{
    m_startTime = start;
}

void
DcbTrafficGenApplicationHelper::SetRealTimeApp(bool realTimeApp)
{
    m_realTimeApp = realTimeApp;
}

void
DcbTrafficGenApplicationHelper::SetCongestionType(StringValue congestionType)
{
    m_congestionType = congestionType;
}

void
DcbTrafficGenApplicationHelper::SetCcAttributes(std::vector<ConfigEntry_t>&& ccAttributes)
{
    m_ccAttributes = std::move(ccAttributes);
}

void
DcbTrafficGenApplicationHelper::SetCcAttribute(ConfigEntry_t ccAttribute)
{
    // If the attribute already exists, replace it. If not, add it.
    for (auto& [name, value] : m_ccAttributes)
    {
        if (name == ccAttribute.first)
        {
            value = ccAttribute.second;
            return;
        }
    }
    m_ccAttributes.push_back(ccAttribute);
}

void
DcbTrafficGenApplicationHelper::SetAppAttributes(std::vector<ConfigEntry_t>&& appAttributes)
{
    m_appAttributes = std::move(appAttributes);
}

void
DcbTrafficGenApplicationHelper::SetSocketAttributes(std::vector<ConfigEntry_t>&& socketAttributes)
{
    m_socketAttributes = std::move(socketAttributes);
}

ApplicationContainer
DcbTrafficGenApplicationHelper::Install(Ptr<Node> node)
{
    // if (m_sendEnabled && !m_sendOnce && m_flowMeanInterval == 0.)
    // {
    //     // The flow interval is not set
    //     // Here we assume the first device of the node is the host's DcbNetDevice
    //     SetLoad(DynamicCast<DcbNetDevice>(node->GetDevice(1)), m_load);
    // }

    // The cdf is not needed to set if the send is disabled.
    // NS_ASSERT_MSG(!m_sendEnabled || m_sendOnce || m_cdf,
    //               "[DcbTrafficGenApplicationHelper] CDF not set, please call SetCdf ().");
    // NS_ASSERT_MSG(m_flowMeanInterval > 0 || !m_sendEnabled || m_sendOnce,
    //               "[DcbTrafficGenApplicationHelper] Load not set, please call SetLoad ().");

    ApplicationContainer app = ApplicationContainer(InstallPriv(node));

    // Set start time and stop time, note that stop time is 0 means infinite
    app.Start(m_startTime);
    app.Stop(m_stopTime);

    return app;
}

Ptr<Application>
DcbTrafficGenApplicationHelper::InstallPriv(Ptr<Node> node)
{
    Ptr<DcbTrafficGenApplication> app = CreateApplication(node);

    // Set app attributes in m_appAttributes
    for (const auto& [name, value] : m_appAttributes)
    {
        app->SetAttribute(name, *value);
    }
    // Set socket attributes in m_socketAttributes
    app->SetSocketAttributes(m_socketAttributes);
    app->SetCcOpsAttributes(m_ccAttributes);

    // Set cdf and average size of CDF as traffic size
    if (m_cdf)
    {
        app->SetFlowCdf(*m_cdf);
        // app->SetAttribute("TrafficSizeBytes", UintegerValue(CalculateCdfMeanSize(m_cdf.get())));
    }

    node->AddApplication(app);

    app->SetProtocolGroup(m_protoGroup);
    switch (m_protoGroup)
    {
    case DcbTrafficGenApplication::ProtocolGroup::RAW_UDP:
        break; // do nothing
    case DcbTrafficGenApplication::ProtocolGroup::TCP:
        break; // TODO: add support of TCP
    case DcbTrafficGenApplication::ProtocolGroup::RoCEv2:
        // must be called after node->AddApplication () becasue it needs to know the node
        app->SetInnerUdpProtocol(RoCEv2L4Protocol::GetTypeId());
    };
    return app;
}

Ptr<DcbTrafficGenApplication>
DcbTrafficGenApplicationHelper::CreateApplication(Ptr<Node> node)
{
    Ptr<DcbTrafficGenApplication> app;

    if (m_realTimeApp)
    {
        app = CreateObject<RealTimeApplication>(m_topology, node->GetId());
    }
    else
    {
        app = CreateObject<DcbTrafficGenApplication>(m_topology, node->GetId());
    }

    return app;
}

// static
double
DcbTrafficGenApplicationHelper::CalculateCdfMeanSize(const DcbTrafficGenApplication::TraceCdf* const cdf)
{
    double res = 0.;
    auto [ls, lp] = (*cdf)[0];
    for (auto [s, p] : (*cdf))
    {
        res += (s + ls) / 2.0 * (p - lp);
        ls = s;
        lp = p;
    }
    return res;
}

// static
std::unique_ptr<std::vector<std::pair<uint32_t, double>>>
DcbTrafficGenApplicationHelper::ConstructCdfFromFile(std::string filename)
{
    std::ifstream cdfFile;
    cdfFile.open(filename);
    // NS_LOG_FUNCTION ("Reading Msg Size Distribution From: " << msgSizeDistFileName);

    std::string line;
    std::istringstream lineBuffer;
    // Note that TraceCdf has const qualifier
    auto cdf = std::make_unique<std::vector<std::pair<uint32_t, double>>>();
    uint32_t msgSizePkts;
    double prob;

    while (getline(cdfFile, line))
    {
        lineBuffer.clear();
        lineBuffer.str(line);
        lineBuffer >> msgSizePkts;
        lineBuffer >> prob;

        cdf->push_back(std::make_pair(msgSizePkts, prob));
    }
    cdfFile.close();

    // Normalize the CDF's probability.
    NormalizeCdf(cdf.get());

    return cdf;
}

// static
std::unique_ptr<std::vector<std::pair<uint32_t, double>>>
DcbTrafficGenApplicationHelper::ConstructCdfFromFile(std::string filename, double scaleFactor)
{
    auto cdf = ConstructCdfFromFile(filename);
    for (auto& [s, p] : *cdf)
    {
        s *= scaleFactor;
    }
    return cdf;
}

// static
std::unique_ptr<std::vector<std::pair<uint32_t, double>>>
DcbTrafficGenApplicationHelper::ConstructCdfFromFile(std::string filename, uint32_t avgSize)
{
    auto cdf = ConstructCdfFromFile(filename);
    double scaleFactor = avgSize / CalculateCdfMeanSize(cdf.get());
    for (auto& [s, p] : *cdf)
    {
        s *= scaleFactor;
    }
    return cdf;
}

// static
void
DcbTrafficGenApplicationHelper::NormalizeCdf(std::vector<std::pair<uint32_t, double>>* cdf)
{
    // The maximum probability of CDF should be 1.
    double max = cdf->back().second;
    for (auto& [s, p] : *cdf)
    {
        p /= max;
    }
}

} // namespace ns3
