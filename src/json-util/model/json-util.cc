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
#include "json-util.h"

#include <boost/json/src.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <time.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("JsonUtil");

namespace json_util
{

boost::json::object
ReadConfig(std::string config_file)
{
    std::ifstream configf;
    configf.open(config_file.c_str());
    std::stringstream buf;
    buf << configf.rdbuf();
    boost::json::error_code ec;
    boost::json::object configJsonObj = boost::json::parse(buf.str()).as_object();
    if (ec.failed())
    {
        std::cout << ec.message() << std::endl;
        NS_FATAL_ERROR("Config file read error!");
    }
    return configJsonObj;
}

void
SetDefault(boost::json::object defaultObj)
{
    for (auto kvPair : defaultObj)
    {
        // kvPair is the first level pair
        // kvPair contains {Class: {Attribute: Value}}
        std::string className = kvPair.key();
        boost::json::object subObj = kvPair.value().get_object();
        for (auto subKvPair : subObj)
        {
            std::string attributeName = subKvPair.key();
            boost::json::value value = subKvPair.value();
            switch (value.kind())
            {
            case boost::json::kind::string:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   StringValue(value.get_string().c_str()));
                // std::cout << "ns3::" + className + "::" + attributeName + "\t" <<
                // value.get_string ().c_str () << std::endl;
                break;
            case boost::json::kind::uint64:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   UintegerValue(value.get_uint64()));
                // std::cout << "ns3::" + className + "::" + attributeName + "\t" <<
                // value.get_uint64 () << std::endl;
                break;
            case boost::json::kind::int64:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   UintegerValue(value.get_int64()));
                // std::cout << "ns3::" + className + "::" + attributeName + "\t" << value.get_int64
                // () << std::endl;
                break;
            case boost::json::kind::bool_:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   BooleanValue(value.get_bool()));
                // std::cout << "ns3::" + className + "::" + attributeName + "\t" << value.get_bool
                // () << std::endl;
                break;
            case boost::json::kind::double_:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   DoubleValue(value.get_double()));
                // std::cout << "ns3::" + className + "::" + attributeName + "\t" <<
                // value.get_double () << std::endl;
                break;
            default:;
            }
        }
    }
}

// Copy from boost's example
void
PrettyPrint(std::ostream& os, const boost::json::value& jv, std::string* indent)
{
    std::string indent_;
    if (!indent)
    {
        indent = &indent_;
    }
    switch (jv.kind())
    {
    case boost::json::kind::object: {
        os << "{\n";
        indent->append(4, ' ');
        const auto& obj = jv.get_object();
        if (!obj.empty())
        {
            auto it = obj.begin();
            for (;;)
            {
                os << *indent << boost::json::serialize(it->key()) << " : ";
                PrettyPrint(os, it->value(), indent);
                if (++it == obj.end())
                {
                    break;
                }
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "}";
        break;
    }

    case boost::json::kind::array: {
        os << "[\n";
        indent->append(4, ' ');
        const auto& arr = jv.get_array();
        if (!arr.empty())
        {
            auto it = arr.begin();
            for (;;)
            {
                os << *indent;
                PrettyPrint(os, *it, indent);
                if (++it == arr.end())
                {
                    break;
                }
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "]";
        break;
    }

    case boost::json::kind::string: {
        os << boost::json::serialize(jv.get_string());
        break;
    }

    case boost::json::kind::uint64:
        os << jv.get_uint64();
        break;

    case boost::json::kind::int64:
        os << jv.get_int64();
        break;

    case boost::json::kind::double_:
        os << jv.get_double();
        break;

    case boost::json::kind::bool_:
        if (jv.get_bool())
        {
            os << "true";
        }
        else
        {
            os << "false";
        }
        break;

    case boost::json::kind::null:
        os << "null";
        break;
    }

    if (indent->empty())
    {
        os << "\n";
    }
}

/***** Utilities about setting seed*****/
void
SetRandomSeed(boost::json::object& configJsonObj)
{
    uint32_t seed = configJsonObj["runtimeConfig"].get_object().find("seed")->value().as_int64();
    if (seed == 0)
    {
        // Random seed
        SeedManager::SetSeed(time(NULL));
    }
    else
    {
        // Manually set seed
        SeedManager::SetSeed(seed);
    }
}

void
SetRandomSeed(uint32_t seed)
{
    if (seed == 0)
    {
        // Random seed
        SeedManager::SetSeed(time(NULL));
    }
    else
    {
        // Manually set seed
        SeedManager::SetSeed(seed);
    }
}

/***** Utilities about application *****/
typedef std::function<void(const boost::json::object&, Ptr<DcTopology>)> AppInstallFunc;
static void InstallTraceApplication(const boost::json::object& appConfig, Ptr<DcTopology> topology);

static std::map<std::string, AppInstallFunc> appInstallMapper = {
    {"TraceApplication", InstallTraceApplication},
    // {"PacketSink", ProtobufTopologyLoader::InstallPacketSink},
    // {"PreGeneratedApplication", ProtobufTopologyLoader::InstallPreGeneratedApplication}
};

static std::map<std::string, TraceApplication::ProtocolGroup> protocolGroupMapper = {
    {"RAW_UDP", TraceApplication::ProtocolGroup::RAW_UDP},
    {"TCP", TraceApplication::ProtocolGroup::TCP},
    {"RoCEv2", TraceApplication::ProtocolGroup::RoCEv2},
};

static std::map<std::string, TraceApplication::TraceCdf*> appCdfMapper = {
    {"WebSearch", &TraceApplication::TRACE_WEBSEARCH_CDF},
    {"FdHadoop", &TraceApplication::TRACE_FDHADOOP_CDF}};

void
InstallApplications(const boost::json::object& conf, Ptr<DcTopology> topology)
{
    boost::json::array appConfigs = conf.find("applicationConfig")->value().get_array();
    // This fucntion can be extended to install more sets of applications using this for loop
    for (const auto& appConfig : appConfigs)
    {
        auto it = appInstallMapper.find(
            appConfig.as_object().find("appName")->value().as_string().c_str());
        if (it == appInstallMapper.end())
        {
            NS_FATAL_ERROR("App \""
                           << appConfig.as_object().find("appName")->value().as_string().c_str()
                           << "\" installation logic has not been implemented");
        }
        AppInstallFunc appInstallLogic = it->second;
        appInstallLogic(appConfig.as_object(), topology);
    }
}

std::shared_ptr<TraceApplicationHelper>
ConstructTraceAppHelper(const boost::json::object& appConfig, Ptr<DcTopology> topology)
{
    std::shared_ptr<TraceApplicationHelper> appHelper =
        std::make_shared<TraceApplicationHelper>(topology);

    // Set protocol group
    std::string sProtocolGroup = appConfig.find("protocolGroup")->value().as_string().c_str();
    if (appConfig.find("protocolGroup") == appConfig.end())
    {
        NS_FATAL_ERROR("Using TraceApplication needs to specify \"protocolGroup\"");
    }
    auto pProtocolGroup = protocolGroupMapper.find(sProtocolGroup);
    if (pProtocolGroup == protocolGroupMapper.end())
    {
        NS_FATAL_ERROR("Cannot recognize protocol group \"" << sProtocolGroup << "\"");
    }
    appHelper->SetProtocolGroup(pProtocolGroup->second);

    // Set CDF
    if (appConfig.find("cdf") == appConfig.end())
    {
        NS_FATAL_ERROR("Using TraceApplication needs to specify \"Cdf\"");
    }
    if (appConfig.find("cdf")->value().is_string())
    {
        // Has CDF type specified
        std::string sCdf = appConfig.find("cdf")->value().as_string().c_str();
        auto pCdf = appCdfMapper.find(sCdf);
        if (pCdf == appCdfMapper.end())
        {
            NS_FATAL_ERROR("Cannot recognize CDF \"" << sCdf << "\".");
        }
        // According to GPT, it will copy the CDF...
        auto newCdf = std::make_unique<TraceApplication::TraceCdf>(*pCdf->second);
        appHelper->SetCdf(std::move(newCdf));
        // If pass the pointer directly, it will cause double free error
        // appHelper.SetCdf(std::unique_ptr<TraceApplication::TraceCdf>(pCdf->second));
    }
    else if (appConfig.find("cdf")->value().is_object())
    {
        // No CDF type specified, use CDF Read from file
        boost::json::object cdfConfig = appConfig.find("cdf")->value().as_object();
        auto cdf = ConstructCdf(cdfConfig);
        appHelper->SetCdf(std::move(cdf));
    }
    else
    {
        NS_FATAL_ERROR("Cannot recognize CDF type");
    }

    // Set dest if specified
    // If not specified, the application will randomly choose a node as destination
    if (appConfig.find("Dest") != appConfig.end())
    {
        // TODO implement this
    }

    // Get the load of the application
    if (appConfig.find("load") == appConfig.end())
    {
        NS_FATAL_ERROR("Using TraceApplication needs to specify \"load\"");
    }
    double dLoad = appConfig.find("load")->value().as_double();
    if (dLoad < 0 || dLoad > 1)
    {
        NS_FATAL_ERROR("Load should be in [0, 1]");
    }
    appHelper->SetLoad(dLoad);

    // Get if static flow interval is enabled, default is false
    auto sfi = appConfig.find("staticFlowInterval");
    if (sfi != appConfig.end())
    {
        appHelper->SetStaticFlowInterval(sfi->value().get_bool());
    }

    // Get the start time of the application
    if (appConfig.find("startTime") == appConfig.end())
    {
        NS_FATAL_ERROR("Using TraceApplication needs to specify \"startTime\"");
    }
    Time startTime = Time(appConfig.find("startTime")->value().as_string().c_str());

    // Get the stop time of the application, if not specified, set to 0
    Time stopTime = Time(0);
    if (appConfig.find("stopTime") != appConfig.end())
    {
        stopTime = Time(appConfig.find("stopTime")->value().as_string().c_str());
    }
    appHelper->SetStartAndStopTime(startTime, stopTime);

    return appHelper;
}

static void
InstallTraceApplication(const boost::json::object& appConfig, Ptr<DcTopology> topology)
{
    std::shared_ptr<TraceApplicationHelper> appHelper =
        ConstructTraceAppHelper(appConfig, topology);

    // Install the application on nodes specified in the config
    // TODO We just assume that the application is installed on all the hosts
    if (appConfig.find("nodes") == appConfig.end())
    {
        NS_FATAL_ERROR("Using TraceApplication needs to specify \"load\"");
    }
    boost::json::string nodes = appConfig.find("nodes")->value().get_string().c_str();
    if (nodes == "all")
    {
        for (auto hostIter = topology->hosts_begin(); hostIter != topology->hosts_end(); hostIter++)
        {
            if (hostIter->type != DcTopology::TopoNode::NodeType::HOST)
            {
                NS_FATAL_ERROR("Node "
                               << topology->GetNodeIndex(hostIter->nodePtr)
                               << " is not a host and thus could not install an application.");
            }
            Ptr<Node> node = hostIter->nodePtr;
            appHelper->Install(node);
        }
    }
    else
    {
        NS_FATAL_ERROR("Only \"all\" is supported for now");
    }
}

/***** Utilities about CDF *****/
std::unique_ptr<TraceApplication::TraceCdf>
ConstructCdf(const boost::json::object& conf)
{
    std::string cdfFile = conf.find("cdfFile")->value().as_string().c_str();
    uint32_t avgSize = ConvertToUint(conf.find("avgSize")->value());
    // The double value may be interpreted as int64_t
    double scaleFactor = ConvertToDouble(conf.find("scaleFactor")->value());

    // If avgSize is specified, use it to scale the CDF
    if (avgSize != 0)
    {
        return TraceApplicationHelper::ConstructCdfFromFile(cdfFile, avgSize);
    }
    // If scaleFactor is specified, use it to scale the CDF
    else if (scaleFactor != 0)
    {
        return TraceApplicationHelper::ConstructCdfFromFile(cdfFile, scaleFactor);
    }
    // If neither is specified, use the CDF directly
    else
    {
        return TraceApplicationHelper::ConstructCdfFromFile(cdfFile);
    }
}

double
ConvertToDouble(const boost::json::value& v)
{
    switch (v.kind())
    {
    case boost::json::kind::uint64:
        return v.get_uint64();
    case boost::json::kind::int64:
        return v.get_int64();
    case boost::json::kind::double_:
        return v.get_double();
    default:
        NS_FATAL_ERROR("Cannot convert to double");
    }
}

int64_t
ConvertToInt(const boost::json::value& v)
{
    switch (v.kind())
    {
    case boost::json::kind::uint64:
        return v.get_uint64();
    case boost::json::kind::int64:
        return v.get_int64();
    case boost::json::kind::double_:
        // If the value is read as double, it may be like 0.999 which should be converted to 1
        // To avoid this, we should convert the double to the nearest integer
        // Note that we need to consider the negative value
        if (v.get_double() < 0)
        {
            return (int64_t)(v.get_double() - 0.5);
        }
        else
        {
            return (int64_t)(v.get_double() + 0.5);
        }
    default:
        NS_FATAL_ERROR("Cannot convert to uint64_t");
    }
}

uint64_t
ConvertToUint(const boost::json::value& v)
{
    switch (v.kind())
    {
    case boost::json::kind::uint64:
        return v.get_uint64();
    case boost::json::kind::int64:
        // If the value is negative, we cannot convert it to uint64_t
        if (v.get_int64() < 0)
        {
            NS_FATAL_ERROR("Cannot convert to uint64_t");
        }
        return v.get_int64();
    case boost::json::kind::double_:
        // If the value is negative, we cannot convert it to uint64_t
        if (v.get_double() < 0)
        {
            NS_FATAL_ERROR("Cannot convert to uint64_t");
        }
        // If the value is read as double, it may be like 0.999 which should be converted to 1
        // To avoid this, we should convert the double to the nearest integer
        return (uint64_t)(v.get_double() + 0.5);
    default:
        NS_FATAL_ERROR("Cannot convert to uint64_t");
    }
}

void
SetStopTime(boost::json::object& configJsonObj)
{
    Time stopTime = Time(
        configJsonObj["runtimeConfig"].get_object().find("stopTime")->value().as_string().c_str());
    Simulator::Stop(stopTime);
}
} // namespace json_util

} // namespace ns3
