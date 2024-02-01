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

#include "ns3/mtp-interface.h"

#include <atomic>
#include <boost/json/src.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <time.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("JsonUtil");

namespace json_util
{

/*****************************************************
 * A series of functions to extract field from json.
 * The definitions are at the end of this file.
 *****************************************************/
static inline boost::json::object JsonGetObjectOrRaise(const boost::json::object& obj,
                                                       const std::string& field,
                                                       const std::string& failureMsg);
static inline int64_t JsonGetInt64OrRaise(const boost::json::object& obj,
                                          const std::string& field,
                                          const std::string& failureMsg);
static inline double JsonGetDoubleOrRaise(const boost::json::object& obj,
                                           const std::string& field,
                                           const std::string& failureMsg);
static inline std::string JsonGetStringOrRaise(const boost::json::object& obj,
                                               const std::string& field,
                                               const std::string& failureMsg);
static inline boost::json::array JsonGetArrayOrRaise(const boost::json::object& obj,
                                                     const std::string& field,
                                                     const std::string& failureMsg);
static inline bool JsonCallIfExistsString(const boost::json::object& obj,
                                          std::string field,
                                          std::function<void(std::string)> callback);
template <typename U>
static bool JsonCallIfExistsInt(const boost::json::object& obj,
                                std::string field,
                                std::function<void(U)> callback);
static inline bool JsonCallIfExistsBool(const boost::json::object& obj,
                                        std::string field,
                                        std::function<void(bool)> callback);

boost::json::object
ReadConfig(std::string configFile)
{
    std::ifstream configf;
    configf.open(configFile.c_str());
    if (!configf.good())
    {
        NS_FATAL_ERROR("Cannot open config file: " << configFile);
    }
    std::stringstream buf;
    buf << configf.rdbuf();
    boost::json::error_code ec;
    boost::json::object configJsonObj = boost::json::parse(buf.str()).as_object();
    try
    {
        if (ec.failed())
        {
            std::cout << ec.message() << std::endl;
            NS_FATAL_ERROR("Config file read error!");
        }
        return configJsonObj;
    }
    catch (const std::exception& err)
    {
        NS_FATAL_ERROR("Failed to parse the json file");
    }
}

void
SetDefault(boost::json::object& configObj)
{
    const auto defaultObj =
        JsonGetObjectOrRaise(configObj, "defaultConfig", "Cannot find defaultConfig field");
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
                break;
            case boost::json::kind::uint64:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   UintegerValue(value.get_uint64()));
                break;
            case boost::json::kind::int64:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   UintegerValue(value.get_int64()));
                break;
            case boost::json::kind::bool_:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   BooleanValue(value.get_bool()));
                break;
            case boost::json::kind::double_:
                Config::SetDefault("ns3::" + className + "::" + attributeName,
                                   DoubleValue(value.get_double()));
                break;
            default:;
            }
        }
    }
}

void
SetGlobal(boost::json::object& configObj)
{
    const auto globalObj =
        JsonGetObjectOrRaise(configObj, "globalConfig", "Cannot find globalConfig field");
    // In this function, global value is allocated on heap without being freed
    // See declaration of this function for more details and take care!!!
    for (auto kvPair : globalObj)
    {
        // kvPair are {Name: Value}
        std::string name = kvPair.key();
        boost::json::value value = kvPair.value();
        switch (value.kind())
        {
        case boost::json::kind::string:
            new ns3::GlobalValue(name,
                                 name,
                                 ns3::StringValue(value.get_string().c_str()),
                                 ns3::MakeStringChecker());
            break;
        case boost::json::kind::uint64:
            new ns3::GlobalValue(name,
                                 name,
                                 ns3::UintegerValue(value.get_uint64()),
                                 ns3::MakeUintegerChecker<uint64_t>());
            break;
        case boost::json::kind::int64:
            // XXX All interger values are read as int64_t..
            new ns3::GlobalValue(name,
                                 name,
                                 ns3::UintegerValue(value.get_int64()),
                                 ns3::MakeUintegerChecker<uint64_t>());
            break;
        case boost::json::kind::bool_:
            new ns3::GlobalValue(name,
                                 name,
                                 ns3::BooleanValue(value.get_bool()),
                                 ns3::MakeBooleanChecker());
            break;
        case boost::json::kind::double_:
            new ns3::GlobalValue(name,
                                 name,
                                 ns3::DoubleValue(value.get_double()),
                                 ns3::MakeDoubleChecker<double>());
            break;
        default:;
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
SetRandomSeed(uint32_t seed)
{
    if (seed == 0)
    {
        // Random seed
        SeedManager::SetSeed(time(nullptr));
    }
    else
    {
        // Manually set seed
        SeedManager::SetSeed(seed);
    }
}

void
SetRuntime(boost::json::object& configJsonObj)
{
    const auto runtimeConfig =
        JsonGetObjectOrRaise(configJsonObj, "runtimeConfig", "Cannot find runtimeConfig field");
    uint32_t seed =
        JsonGetInt64OrRaise(runtimeConfig, "seed", "Cannot find seed field in runtimeConfig");
    SetRandomSeed(seed);
    std::string stopTime = JsonGetStringOrRaise(runtimeConfig,
                                                "stopTime",
                                                "Cannot find stopTime field in runtimeConfig");
    Simulator::Stop(Time(stopTime));
    JsonCallIfExistsInt<uint32_t>(runtimeConfig, "mtpThreads", [](uint32_t threads) {
        if (threads > 1)
        {
            NS_LOG_INFO("Using MTP with threads: " << threads);
            MtpInterface::Enable(threads);
        }
    });
}

/***** Utilities about application *****/
typedef std::function<ApplicationContainer(const boost::json::object&, Ptr<DcTopology>)>
    AppInstallFunc;
static ApplicationContainer InstallTraceApplication(const boost::json::object& appConfig,
                                                    Ptr<DcTopology> topology);

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

ApplicationContainer
InstallApplications(const boost::json::object& conf, Ptr<DcTopology> topology)
{
    boost::json::array appConfigs =
        JsonGetArrayOrRaise(conf, "applicationConfig", "Cannot find applicationConfig field");
    ApplicationContainer apps;
    // This fucntion can be extended to install more sets of applications using this for loop
    for (const auto& appConfig : appConfigs)
    {
        std::string appName = JsonGetStringOrRaise(appConfig.as_object(),
                                                   "appName",
                                                   "Cannot find appName field in appConfig");
        auto it = appInstallMapper.find(appName);
        if (it == appInstallMapper.end())
        {
            NS_FATAL_ERROR("App \"" << appName << "\" installation logic has not been implemented");
        }
        AppInstallFunc appInstallLogic = it->second;
        apps.Add(appInstallLogic(appConfig.as_object(), topology));
    }
    return apps;
}

std::shared_ptr<TraceApplicationHelper>
ConstructTraceAppHelper(const boost::json::object& appConfig, Ptr<DcTopology> topology)
{
    std::shared_ptr<TraceApplicationHelper> appHelper =
        std::make_shared<TraceApplicationHelper>(topology);

    // Set protocol group
    std::string sProtocolGroup =
        JsonGetStringOrRaise(appConfig,
                             "protocolGroup",
                             "Using TraceApplication needs to specify \"protocolGroup\"");
    auto pProtocolGroup = protocolGroupMapper.find(sProtocolGroup);
    if (pProtocolGroup == protocolGroupMapper.end())
    {
        NS_FATAL_ERROR("Cannot recognize protocol group \"" << sProtocolGroup << "\"");
    }
    appHelper->SetProtocolGroup(pProtocolGroup->second);

    // Set CDF
    auto cdfObjIt = appConfig.find("cdf");
    if (cdfObjIt == appConfig.end())
    {
        NS_FATAL_ERROR("Using TraceApplication needs to specify \"Cdf\"");
    }
    if (cdfObjIt->value().is_string())
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
    else if (cdfObjIt->value().is_object())
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
    double dLoad =
        JsonGetDoubleOrRaise(appConfig, "load", "Using TraceApplication needs to specify \"load\"");
    if (dLoad < 0 || dLoad > 1)
    {
        NS_FATAL_ERROR("Load should be in [0, 1]");
    }
    appHelper->SetLoad(dLoad);

    // Set static flow interval if is enabled, default is false
    JsonCallIfExistsBool(appConfig, "staticFlowInterval", [&appHelper](bool b) {
        appHelper->SetStaticFlowInterval(b);
    });

    // Set send once if is enabled, default is false
    JsonCallIfExistsBool(appConfig, "sendOnce", [&appHelper](bool b) {
        appHelper->SetStaticFlowInterval(b);
    });

    // Get and set the start time of the application
    std::string startTimeStr =
        JsonGetStringOrRaise(appConfig,
                             "startTime",
                             "Using TraceApplication needs to specify \"startTime\"");
    Time startTime = Time(startTimeStr);

    // Get the stop time of the application, if specified
    Time stopTime = Time(0);
    JsonCallIfExistsString(appConfig, "stopTime", [&stopTime](std::string s) {
        stopTime = Time(s);
    });
    appHelper->SetStartAndStopTime(startTime, stopTime);

    return appHelper;
}

static ApplicationContainer
InstallTraceApplication(const boost::json::object& appConfig, Ptr<DcTopology> topology)
{
    std::shared_ptr<TraceApplicationHelper> appHelper =
        ConstructTraceAppHelper(appConfig, topology);
    ApplicationContainer apps;

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
            apps.Add(appHelper->Install(node));
        }
    }
    else
    {
        NS_FATAL_ERROR("Only \"all\" is supported for now");
    }
    return apps;
}

/***** Utilities about CDF *****/
std::unique_ptr<TraceApplication::TraceCdf>
ConstructCdf(const boost::json::object& conf)
{
    std::string cdfFile = JsonGetStringOrRaise(conf, "cdfFile", "Cannot find cdfFile field");
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
OutputStats(boost::json::object& conf, ApplicationContainer& apps, Ptr<DcTopology> topology)
{
    auto obj = JsonGetObjectOrRaise(conf, "outputFile", "Cannot find outputFile field");
    std::string outputFile = JsonGetStringOrRaise(obj, "resultFile", "CAnnot file resultFile field in outputFile");
    std::ofstream ofs(outputFile);
    if (!ofs.is_open())
    {
        NS_FATAL_ERROR("Cannot open file " << outputFile);
    }
    boost::json::object outputObj;
    outputObj["config"] = conf;
    std::shared_ptr<boost::json::object> appStatsObj = ConstructAppStatsObj(apps);
    outputObj["overallStatistics"] = appStatsObj->find("overallStatistics")->value();

    // Construct flow statistics
    FlowStatsObjMap mFlowStatsObjs;
    ConstructSenderFlowStats(apps, mFlowStatsObjs);
    ConstructRealTimeFlowStats(apps, mFlowStatsObjs);
    boost::json::array flowStatsArray;
    uint32_t flowId = 0;
    for (auto kvPair : mFlowStatsObjs)
    {
        // Replace the flow id at the beginning of the object
        kvPair.second->find("flowId")->value() = flowId++;
        flowStatsArray.emplace_back(*kvPair.second);
        // std::cout << "Flow " << flowId << " statistics" << std::endl;
    }
    outputObj["flowStatistics"] = flowStatsArray;

    // Write the output object to file
    PrettyPrint(ofs, outputObj);
}

std::shared_ptr<boost::json::object>
ConstructAppStatsObj(ApplicationContainer& apps)
{
    std::shared_ptr<boost::json::object> appStatsObj = std::make_shared<boost::json::object>();
    boost::json::object overallStatsObj;

    // Variables used to calculate overall statistics
    // The start time of the first flow and the end time of the last flow
    Time startTime = Simulator::Now();
    Time finishTime = Time(0);
    // Total bytes of all flows, used to calculate the total throughput rate
    uint32_t totalPkts = 0;
    uint64_t totalBytes = 0;
    uint32_t totalSentPkts = 0;
    uint64_t totalSentBytes = 0;
    // uint32_t retxCount = 0;
    // FCT of all flows, used to calculate the average and percentile FCT
    std::vector<Time> vFct;

    for (uint32_t i = 0; i < apps.GetN(); ++i)
    {
        Ptr<TraceApplication> app = DynamicCast<TraceApplication>(apps.Get(i));
        if (app == nullptr)
            continue;
        auto appStats = app->GetStats();

        // Get variables of the overall statistics
        startTime = std::min(startTime, appStats->tStart);
        finishTime = std::max(finishTime, appStats->tFinish);
        totalPkts += appStats->nTotalSizePkts;
        totalBytes += appStats->nTotalSizeBytes;
        totalSentPkts += appStats->nTotalSentPkts;
        totalSentBytes += appStats->nTotalSentBytes;
        // retxCount += appStats->nRetxCount;

        // Per flow statistics
        auto mFlowStats = appStats->mFlowStats;
        for (auto pFlowStats : mFlowStats)
        {
            vFct.push_back(pFlowStats.second->tFct);
        }
    }

    // Calculate overall statistics
    overallStatsObj["totalThroughputBps"] =
        totalBytes * 8.0 / (finishTime - startTime).GetSeconds();
    overallStatsObj["totalRetxRatePkt"] = double(totalSentPkts - totalPkts) / totalPkts;
    overallStatsObj["totalRetxRateByte"] = double(totalSentBytes - totalBytes) / totalBytes;
    // Calculate average and percentile FCT
    uint32_t nFct = vFct.size();
    if (nFct == 0)
    {
        NS_LOG_ERROR("JsonUtil: no flow statistics gathered");
        return appStatsObj;
    }
    std::sort(vFct.begin(), vFct.end());
    // Average FCT is calculated by the total FCT / number of flows
    Time avgFct = std::accumulate(vFct.begin(), vFct.end(), Time(0)) / nFct;
    Time p95Fct = vFct[0.95 * nFct];
    Time p99Fct = vFct[0.99 * nFct];
    Time p999Fct = vFct[0.999 * nFct];
    overallStatsObj["avgFctNs"] = avgFct.GetNanoSeconds();
    overallStatsObj["p95FctNs"] = p95Fct.GetNanoSeconds();
    overallStatsObj["p99FctNs"] = p99Fct.GetNanoSeconds();
    overallStatsObj["p999FctNs"] = p999Fct.GetNanoSeconds();

    appStatsObj->emplace("overallStatistics", overallStatsObj);
    return appStatsObj;
}

void
ConstructSenderFlowStats(ApplicationContainer& apps, FlowStatsObjMap& mFlowStatsObjs)
{
    for (uint32_t i = 0; i < apps.GetN(); ++i)
    {
        Ptr<TraceApplication> app = DynamicCast<TraceApplication>(apps.Get(i));
        if (app == nullptr)
            {
            continue;
            }
        auto appStats = app->GetStats();

        // Per flow statistics
        auto mFlowStats = appStats->mFlowStats;
        for (const auto &pFlowStats : mFlowStats)
        {
            FlowIdentifier flowIdentifier = pFlowStats.first;
            auto flowStats = pFlowStats.second;
            // In this function, we will create a new json object for each flow
            // and store it in the map
            auto flowStatsObj = std::make_shared<boost::json::object>();
            // mFlowStatsObjs.insert(std::make_pair(flowIdentifier, flowStatsObj));
            mFlowStatsObjs[flowIdentifier] = flowStatsObj;

            (*flowStatsObj)["flowId"] = 0; // This will be filled later
            (*flowStatsObj)["flowIdentifier"] =
                boost::json::object{{"srcAddr", flowIdentifier.GetSrcAddrString()},
                                    {"srcPort", flowIdentifier.srcPort},
                                    {"dstAddr", flowIdentifier.GetDstAddrString()},
                                    {"dstPort", flowIdentifier.dstPort}};
            (*flowStatsObj)["totalSizePkts"] = flowStats->nTotalSizePkts;
            (*flowStatsObj)["totalSizeBytes"] = flowStats->nTotalSizeBytes;
            (*flowStatsObj)["retxCount"] = flowStats->nRetxCount;
            (*flowStatsObj)["fctNs"] = flowStats->tFct.GetNanoSeconds();
            (*flowStatsObj)["overallFlowRate"] = flowStats->overallFlowRate.GetBitRate();

            // Detailed statistics
            if (flowStats->bDetailedSenderStats)
            {
                boost::json::array ccRateArray;
                boost::json::array ccCwndArray;
                boost::json::array recvEcnArray;
                boost::json::array sentPktArray;

                for (const auto &ccRate : flowStats->vCcRate)
                {
                    ccRateArray.emplace_back(
                        boost::json::object{{"timeNs", ccRate.first.GetNanoSeconds()},
                                            {"rateBps", ccRate.second.GetBitRate()}});
                }
                for (auto ccCwnd : flowStats->vCcCwnd)
                {
                    ccCwndArray.emplace_back(
                        boost::json::object{{"timeNs", ccCwnd.first.GetNanoSeconds()},
                                            {"cwndByte", ccCwnd.second}});
                }
                for (const auto &recvEcn : flowStats->vRecvEcn)
                {
                    recvEcnArray.emplace_back(
                        boost::json::object{{"timeNs", recvEcn.GetNanoSeconds()}});
                }
                for (auto sentPkt : flowStats->vSentPkt)
                {
                    sentPktArray.emplace_back(
                        boost::json::object{{"timeNs", sentPkt.first.GetNanoSeconds()},
                                            {"sizeByte", sentPkt.second}});
                }

                (*flowStatsObj)["ccRate"] = ccRateArray;
                (*flowStatsObj)["ccCwnd"] = ccCwndArray;
                (*flowStatsObj)["recvEcn"] = recvEcnArray;
                (*flowStatsObj)["sentPkt"] = sentPktArray;
            }

            if (flowStats->bDetailedRetxStats)
            {
                // Add the sent psn into the sentPktArray if bDetailedSenderStats is true
                // Otherwise, we will create a new array
                if (flowStats->bDetailedSenderStats)
                {
                    boost::json::array& sentPktArray = (*flowStatsObj)["sentPkt"].as_array();
                    for (uint32_t i = 0; i < flowStats->vSentPsn.size(); ++i)
                    {
                        // The sentPktArray is already created, so we just need to add the psn
                        sentPktArray[i].as_object().emplace("psn", flowStats->vSentPsn[i].second);
                    }
                }
                else
                {
                    boost::json::array sentPktArray;
                    for (auto sentPsn : flowStats->vSentPsn)
                    {
                        sentPktArray.emplace_back(
                            boost::json::object{{"timeNs", sentPsn.first.GetNanoSeconds()},
                                                {"psn", sentPsn.second}});
                    }
                    (*flowStatsObj)["sentPkt"] = sentPktArray;
                }

                boost::json::array recvAckArray;
                // vExpectedPsn must has stats, but vAckedPsn may not
                bool bHasAckedPsn = flowStats->vAckedPsn.size() > 0;
                // vAckedPsn's number of elements must be leq than vExpectedPsn's as a ack may carry
                // acked psn, but must carry expected psn. Thus we use a index to iterate vAckedPsn.
                uint32_t ackedPsnIndex = 0;
                for (auto expectedPsn : flowStats->vExpectedPsn)
                {
                    boost::json::object recvAckObj;
                    recvAckObj.emplace("timeNs", expectedPsn.first.GetNanoSeconds());
                    recvAckObj.emplace("expectedPsn", expectedPsn.second);
                    if (bHasAckedPsn &&
                        flowStats->vAckedPsn[ackedPsnIndex].first == expectedPsn.first)
                    {
                        recvAckObj.emplace("ackedPsn",
                                           flowStats->vAckedPsn[ackedPsnIndex++].second);
                    }
                    recvAckArray.emplace_back(recvAckObj);
                }
                (*flowStatsObj)["recvAck"] = recvAckArray;
            }
        }
    }
}

void
ConstructRealTimeFlowStats(ApplicationContainer& apps, FlowStatsObjMap& mFlowStatsObjs)
{
    for (uint32_t i = 0; i < apps.GetN(); ++i)
    {
        Ptr<RealTimeApplication> app = DynamicCast<RealTimeApplication>(apps.Get(i));
        if (app == nullptr)
            {
            continue;
            }
        auto appStats = app->GetStats();
        std::shared_ptr<RealTimeApplication::Stats> rtaStats =
            std::dynamic_pointer_cast<RealTimeApplication::Stats>(appStats);
        if (rtaStats == nullptr)
        {
            NS_FATAL_ERROR("Cannot cast to RealTimeApplication::Stats");
        }

        for (const auto &pFlowStats : rtaStats->mflowStats)
        {
            FlowIdentifier flowIdentifier = pFlowStats.first;
            auto flowStats = pFlowStats.second;
            // In this function, every flow should already have a json object
            // We just need to add the real time stats to the object
            auto flowStatsObj = mFlowStatsObjs[flowIdentifier];

            (*flowStatsObj)["throughputFromArriveBps"] = flowStats->rAvgRateFromArrive.GetBitRate();
            (*flowStatsObj)["throughputFromRecvBps"] = flowStats->rAvgRateFromRecv.GetBitRate();

            // Calculate average and percentile delay
            std::vector<Time> vArriveDelay = flowStats->vArriveDelay;
            std::vector<Time> vTxDelay = flowStats->vTxDelay;
            std::sort(vArriveDelay.begin(), vArriveDelay.end());
            std::sort(vTxDelay.begin(), vTxDelay.end());
            uint32_t nArriveDelay = vArriveDelay.size();
            uint32_t nTxDelay = vTxDelay.size();
            Time avgArriveDelay =
                std::accumulate(vArriveDelay.begin(), vArriveDelay.end(), Time(0)) / nArriveDelay;
            Time avgTxDelay = std::accumulate(vTxDelay.begin(), vTxDelay.end(), Time(0)) / nTxDelay;
            Time p95ArriveDelay = vArriveDelay[0.95 * nArriveDelay];
            Time p99ArriveDelay = vArriveDelay[0.99 * nArriveDelay];
            Time p95TxDelay = vTxDelay[0.95 * nTxDelay];
            Time p99TxDelay = vTxDelay[0.99 * nTxDelay];
            (*flowStatsObj)["avgArriveDelayNs"] = avgArriveDelay.GetNanoSeconds();
            (*flowStatsObj)["p95ArriveDelayNs"] = p95ArriveDelay.GetNanoSeconds();
            (*flowStatsObj)["p99ArriveDelayNs"] = p99ArriveDelay.GetNanoSeconds();
            (*flowStatsObj)["avgTxDelayNs"] = avgTxDelay.GetNanoSeconds();
            (*flowStatsObj)["p95TxDelayNs"] = p95TxDelay.GetNanoSeconds();
            (*flowStatsObj)["p99TxDelayNs"] = p99TxDelay.GetNanoSeconds();

            // Add detailed stats
            if (rtaStats->bDetailedSenderStats)
            {
                boost::json::array recvPktArray;
                for (auto recvPkt : flowStats->vRecvPkt)
                {
                    recvPktArray.emplace_back(
                        boost::json::object{{"timeNs", recvPkt.first.GetNanoSeconds()},
                                            {"sizeByte", recvPkt.second}});
                }
                (*flowStatsObj)["recvPkt"] = recvPktArray;
            }
        }
    }
}

/*****************************************************
 * A series of functions to extract field from json.
 *****************************************************/
static inline boost::json::object::const_iterator
JsonGetFieldOrRaise(const boost::json::object& obj,
                    const std::string& field,
                    const std::string& failureMsg)
{
    boost::json::object::const_iterator subobj = obj.find(field);
    if (subobj == obj.end())
    {
        NS_FATAL_ERROR(failureMsg);
    }
    return subobj;
}

static inline boost::json::object
JsonGetObjectOrRaise(const boost::json::object& obj,
                     const std::string& field,
                     const std::string& failureMsg)
{
    auto subobj = JsonGetFieldOrRaise(obj, field, failureMsg);
    return subobj->value().get_object();
}

static inline int64_t
JsonGetInt64OrRaise(const boost::json::object& obj,
                    const std::string& field,
                    const std::string& failureMsg)
{
    auto subobj = JsonGetFieldOrRaise(obj, field, failureMsg);
    try
    {
        return subobj->value().as_int64();
    }
    catch (const std::exception& err)
    {
        NS_FATAL_ERROR("Failed to parse field " << field << " as an int64");
    }
}

static inline double
JsonGetDoubleOrRaise(const boost::json::object& obj,
                     const std::string& field,
                     const std::string& failureMsg)
{
    auto subobj = JsonGetFieldOrRaise(obj, field, failureMsg);
    try
    {
        return subobj->value().as_double();
    }
    catch (const std::exception& err)
    {
        NS_FATAL_ERROR("Failed to parse field " << field << " as an int64");
    }
}

static inline std::string
JsonGetStringOrRaise(const boost::json::object& obj,
                     const std::string& field,
                     const std::string& failureMsg)
{
    auto subobj = JsonGetFieldOrRaise(obj, field, failureMsg);
    try
    {
        return subobj->value().as_string().c_str();
    }
    catch (const std::exception& err)
    {
        NS_FATAL_ERROR("Failed to parse field " << field << " as a string");
    }
}

static inline boost::json::array
JsonGetArrayOrRaise(const boost::json::object& obj,
                    const std::string& field,
                    const std::string& failureMsg)
{
    auto subobj = JsonGetFieldOrRaise(obj, field, failureMsg);
    try
    {
        return subobj->value().get_array();
    }
    catch (const std::exception& err)
    {
        NS_FATAL_ERROR("Failed to parse field " << field << " as array");
    }
}

static inline bool
JsonCallIfExistsString(const boost::json::object& obj,
                       std::string field,
                       std::function<void(std::string)> callback)
{
    boost::json::object::const_iterator subobj = obj.find(field);
    if (subobj == obj.end())
    {
        return false;
    }
    callback(subobj->value().as_string().c_str());
    return true;
}

template <typename U>
static bool
JsonCallIfExistsInt(const boost::json::object& obj,
                    std::string field,
                    std::function<void(U)> callback)
{
    static_assert(std::is_integral<U>::value, "Integer type required.");
    boost::json::object::const_iterator subobj = obj.find(field);
    if (subobj == obj.end())
    {
        return false;
    }
    callback(static_cast<U>(subobj->value().as_int64()));
    return true;
}

static inline bool
JsonCallIfExistsBool(const boost::json::object& obj,
                     std::string field,
                     std::function<void(bool)> callback)
{
    boost::json::object::const_iterator subobj = obj.find(field);
    if (subobj == obj.end())
    {
        return false;
    }
    callback(subobj->value().as_bool());
    return true;
}

} // namespace json_util

} // namespace ns3
