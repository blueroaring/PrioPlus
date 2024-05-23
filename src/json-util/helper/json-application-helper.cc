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
#include "json-application-helper.h"

#include "ns3/fifo-queue-disc-ecn.h"
#include "ns3/json-utils.h"
#include "ns3/rocev2-hpcc.h"
#include "ns3/rocev2-swift.h"
#include "ns3/rocev2-timely.h"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <time.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("JsonApplicationHelper");

namespace json_util
{

/***** Utilities about application *****/
typedef std::function<ApplicationContainer(const boost::json::object&, Ptr<DcTopology>)>
    AppInstallFunc;
static ApplicationContainer InstallDcbTrafficGenApplication(const boost::json::object& appConfig,
                                                    Ptr<DcTopology> topology);

static std::map<std::string, AppInstallFunc> appInstallMapper = {
    {"DcbTrafficGenApplication", InstallDcbTrafficGenApplication},
    // {"PacketSink", ProtobufTopologyLoader::InstallPacketSink},
    // {"PreGeneratedApplication", ProtobufTopologyLoader::InstallPreGeneratedApplication}
};

static std::map<std::string, DcbTrafficGenApplication::ProtocolGroup> protocolGroupMapper = {
    {"RAW_UDP", DcbTrafficGenApplication::ProtocolGroup::RAW_UDP},
    {"TCP", DcbTrafficGenApplication::ProtocolGroup::TCP},
    {"RoCEv2", DcbTrafficGenApplication::ProtocolGroup::RoCEv2},
};

ApplicationContainer
InstallApplications(const boost::json::object& conf, Ptr<DcTopology> topology)
{
    boost::json::array appConfigs =
        JsonGetArrayOrRaise(conf, "applicationConfig", "Cannot find applicationConfig field");
    ApplicationContainer apps;
    // This fucntion can be extended to install more sets of applications using this for loop
    for (const auto& appConfig : appConfigs)
    {
        apps.Add(InstallDcbTrafficGenApplication(appConfig.as_object(), topology));
    }
    return apps;
}

/***** Utilities about DcbTrafficGenApplication *****/

std::shared_ptr<DcbTrafficGenApplicationHelper>
ConstructTraceAppHelper(const boost::json::object& appConfig, Ptr<DcTopology> topology)
{
    std::shared_ptr<DcbTrafficGenApplicationHelper> appHelper =
        std::make_shared<DcbTrafficGenApplicationHelper>(topology);
    
    // Application type
    std::string appType =
        JsonGetStringOrRaise(appConfig, "appType", "Need to specify \"appType\"");
    appHelper->SetApplicationTypeId(TypeId::LookupByName(appType));

    // Set protocol group
    std::string sProtocolGroup =
        JsonGetStringOrRaise(appConfig,
                             "protocolGroup",
                             "Using DcbTrafficGenApplication needs to specify \"protocolGroup\"");
    auto pProtocolGroup = protocolGroupMapper.find(sProtocolGroup);
    if (pProtocolGroup == protocolGroupMapper.end())
    {
        NS_FATAL_ERROR("Cannot recognize protocol group \"" << sProtocolGroup << "\"");
    }
    appHelper->SetProtocolGroup(pProtocolGroup->second);

    // Set CDF
    auto cdfObjIt = appConfig.find("cdf");
    if (cdfObjIt != appConfig.end())
    {
        if (cdfObjIt->value().is_object())
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
    }

    if (appConfig.find("applicationConfig") != appConfig.end())
    {
        boost::json::object localAppConfig =
            appConfig.find("applicationConfig")->value().as_object();
        std::unique_ptr<std::vector<DcbTrafficGenApplicationHelper::ConfigEntry_t>> appConfigVector =
            ConstructConfigVector(localAppConfig);
        appHelper->SetAppAttributes(std::move(*appConfigVector));
    }

    if (appConfig.find("socketConfig") != appConfig.end())
    {
        boost::json::object socketConfigObj = appConfig.find("socketConfig")->value().as_object();
        std::unique_ptr<std::vector<DcbTrafficGenApplicationHelper::ConfigEntry_t>> socketConfigVector =
            ConstructConfigVector(socketConfigObj);
        appHelper->SetSocketAttributes(std::move(*socketConfigVector));
    }

    // Set cc config
    if (appConfig.find("congestionConfig") != appConfig.end())
    {
        boost::json::object ccConfig = appConfig.find("congestionConfig")->value().as_object();
        std::unique_ptr<std::vector<DcbTrafficGenApplicationHelper::ConfigEntry_t>> ccConfigVector =
            ConstructConfigVector(ccConfig);
        appHelper->SetCcAttributes(std::move(*ccConfigVector));
    }

    // Get the start time of the application
    Time startTime =
        Time(JsonGetStringOrRaise(appConfig,
                                  "startTime",
                                  "Using DcbTrafficGenApplication needs to specify \"startTime\""));

    // Get the stop time of the application, if not specified, set to 0
    Time stopTime = Time(0);
    if (appConfig.find("stopTime") != appConfig.end())
    {
        stopTime = Time(appConfig.find("stopTime")->value().as_string().c_str());
    }
    appHelper->SetStartAndStopTime(startTime, stopTime);

    return appHelper;
}

static ApplicationContainer
InstallDcbTrafficGenApplication(const boost::json::object& appConfig, Ptr<DcTopology> topology)
{
    std::shared_ptr<DcbTrafficGenApplicationHelper> appHelper =
        ConstructTraceAppHelper(appConfig, topology);
    ApplicationContainer apps;

    // Install the application on nodes specified in the config
    // TODO We just assume that the application is installed on all the hosts
    if (appConfig.find("nodes") == appConfig.end())
    {
        NS_FATAL_ERROR("Using DcbTrafficGenApplication needs to specify \"load\"");
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
    // if nodes start with "random [num]"
    else if (boost::algorithm::starts_with(nodes.c_str(), "random"))
    {
        // If the nodes are specified, convert the string to vector
        // Here we assume the nodes are specified in the format like "random [num]".
        std::string numStr = nodes.c_str();
        numStr = numStr.substr(nodes.find_first_of(" ") + 1);
        uint32_t num = std::stoi(numStr);
        std::set<uint32_t> vHosts = GetRandomHosts(num, topology->GetNHosts());
        // vHosts are index of topology's hosts, not the index of nodes
        for (auto hostIter = topology->hosts_begin(); hostIter != topology->hosts_end(); hostIter++)
        {
            if (hostIter->type != DcTopology::TopoNode::NodeType::HOST)
            {
                NS_FATAL_ERROR("Node "
                               << topology->GetNodeIndex(hostIter->nodePtr)
                               << " is not a host and thus could not install an application.");
            }
            if (vHosts.find(topology->GetNodeIndex(hostIter->nodePtr)) != vHosts.end())
            {
                Ptr<Node> node = hostIter->nodePtr;
                apps.Add(appHelper->Install(node));
            }
        }
    }
    else
    {
        // If the nodes are specified, convert the string to vector
        // Here we assume the nodes are specified in the format like "[1:3,5,7:]".
        std::vector<uint32_t> vNodes = ConvertRangeToVector(nodes.c_str(), topology->GetNHosts());

        // Get the start time and the flow interval
        Time startTime = Time(appConfig.find("startTime")->value().as_string().c_str());
        Time flowInterval = Time(0);
        // If the flow interval is not set, this will have no effect
        if (appConfig.find("flowInterval") != appConfig.end())
        {
            flowInterval = Time(appConfig.find("flowInterval")->value().as_string().c_str());
        }
        Time duration = Time(0);
        // If the duration is not set, this will have no effect
        if (appConfig.find("duration") != appConfig.end())
        {
            duration = Time(appConfig.find("duration")->value().as_string().c_str());
        }

        for (uint32_t i = 0; i < vNodes.size(); ++i)
        {
            Ptr<Node> node = topology->GetNode(vNodes[i]).nodePtr;

            if (duration != Time(0))
            {
                appHelper->SetStartAndStopTime(startTime + i * flowInterval,
                                               startTime + i * flowInterval + duration);
            }
            else
            {
                appHelper->SetStartTime(startTime + i * flowInterval);
            }

            // if has groupCongestionConfig, set the attributes according to the index
            if (appConfig.find("groupCongestionConfig") != appConfig.end())
            {
                boost::json::object gConf =
                    appConfig.find("groupCongestionConfig")->value().as_object();
                SetGroupAttributes(gConf, appHelper, vNodes.size(), i);
            }

            apps.Add(appHelper->Install(node));
        }
    }

    return apps;
}

void
SetGroupAttributes(const boost::json::object& gConf,
                   std::shared_ptr<DcbTrafficGenApplicationHelper> helper,
                   uint32_t groupSize,
                   uint32_t idx)
{
    // Iterate over the attributes in the group
    for (auto kvPair : gConf)
    {
        std::string name = kvPair.key();
        // Check if name is in format of "Congestion::xxx"
        // Split the name into two parts
        std::vector<std::string> vName;
        // boost::split(vName, name, boost::is_any_of("::"));
        // This split will split "Congestion::xxx" into "Congestion", "" and "xxx"
        // So we need to use the following split
        boost::split(vName, name, boost::is_any_of("::"), boost::token_compress_on);
        if (vName.size() != 2)
        {
            NS_FATAL_ERROR("Attribute name \"" << name << "\" is not in format of \"xxx::xxx\"");
        }
        std::string className = vName[0];
        std::string attributeName = vName[1];

        // The value of the attribute is a object with two fields: "Begin" and "End"
        // Calculate the value of the attribute according to the index
        boost::json::object valueObj = kvPair.value().as_object();
        boost::json::value beginValue = valueObj.find("begin")->value();
        boost::json::value endValue = valueObj.find("end")->value();
        // Now assume the value is a sting with number and unit with space, like "50 KB"
        std::string beginStr = beginValue.as_string().c_str();
        std::string endStr = endValue.as_string().c_str();
        // Convert the string into two parts, number and unit
        std::vector<std::string> vBegin;
        boost::split(vBegin, beginStr, boost::is_any_of(" "), boost::token_compress_on);
        std::vector<std::string> vEnd;
        boost::split(vEnd, endStr, boost::is_any_of(" "), boost::token_compress_on);
        // If have unit check the unit is the same
        NS_ASSERT_MSG((vBegin.size() == 1 && vEnd.size() == 1) ||
                          (vBegin.size() == 2 && vEnd.size() == 2 && vBegin[1] == vEnd[1]),
                      "The unit of begin and end should be the same");

        bool isFloat = false;
        if (valueObj.find("isFloat") != valueObj.end())
        {
            isFloat = valueObj.find("isFloat")->value().get_bool();
        }

        Ptr<AttributeValue> attrValue;
        if (isFloat)
        {
            double begin = std::stod(vBegin[0]);
            double end = std::stod(vEnd[0]);
            double value = begin + (double)idx * (double)(end - begin) / (double)(groupSize - 1);
            if (vBegin.size() == 2)
            {
                // Add the unit to the value and make it a string if have unit
                std::string valueStr = std::to_string(value) + vBegin[1];
                attrValue = StringValue(valueStr.c_str()).Copy();
            }
            else
            {
                attrValue = DoubleValue(value).Copy();
            }
        }
        else
        {
            uint32_t begin = std::stoi(vBegin[0]);
            uint32_t end = std::stoi(vEnd[0]);
            uint32_t value = begin + (double)idx * (double)(end - begin) / (double)(groupSize - 1);
            // Add the unit to the value and make it a string if have unit
            std::string valueStr = std::to_string(value) + vBegin[1];
            if (vBegin.size() == 2)
            {
                // Add the unit to the value and make it a string if have unit
                std::string valueStr = std::to_string(value) + vBegin[1];
                attrValue = StringValue(valueStr.c_str()).Copy();
                std::cout << "Set " << className << "::" << attributeName << " to " << valueStr
                          << std::endl;
            }
            else
            {
                attrValue = UintegerValue(value).Copy();
            }
        }

        // Set to application helper
        if (className == "Congestion")
        {
            helper->SetCcAttribute(std::make_pair(attributeName, attrValue));
        }
        // else if (className == "Socket")
        // {
        //     helper->SetSocketAttributes(std::make_pair(attributeName,
        //     StringValue(valueStr.c_str()).Copy()));
        // }
        // else if (className == "Application")
        // {
        //     helper->SetAppAttributes(std::make_pair(attributeName,
        //     StringValue(valueStr.c_str()).Copy()));
        // }
        else
        {
            NS_FATAL_ERROR("Cannot recognize class name \"" << className << "\"");
        }
    }
}

/***** Utilities about CDF *****/
std::unique_ptr<DcbTrafficGenApplication::TraceCdf>
ConstructCdf(const boost::json::object& conf)
{
    std::string cdfFile = conf.find("cdfFile")->value().as_string().c_str();
    uint32_t avgSize = ConvertToUint(conf.find("avgSize")->value());
    // The double value may be interpreted as int64_t
    double scaleFactor = ConvertToDouble(conf.find("scaleFactor")->value());

    // If avgSize is specified, use it to scale the CDF
    if (avgSize != 0)
    {
        return DcbTrafficGenApplicationHelper::ConstructCdfFromFile(cdfFile, avgSize);
    }
    // If scaleFactor is specified, use it to scale the CDF
    else if (scaleFactor != 0)
    {
        return DcbTrafficGenApplicationHelper::ConstructCdfFromFile(cdfFile, scaleFactor);
    }
    // If neither is specified, use the CDF directly
    else
    {
        return DcbTrafficGenApplicationHelper::ConstructCdfFromFile(cdfFile);
    }
}

std::set<uint32_t>
GetRandomHosts(uint32_t num, uint32_t max)
{
    // using ns3's random number generator
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    // can be choce from 0 to hosts's size, and should be integer
    rand->SetAttribute("Min", DoubleValue(0));
    rand->SetAttribute("Max", DoubleValue(max));

    // make sure that no duplicate hosts are selected
    std::set<uint32_t> hosts;
    while (hosts.size() < num)
    {
        hosts.insert(rand->GetInteger());
    }
    return hosts;
}

} // namespace json_util

} // namespace ns3
