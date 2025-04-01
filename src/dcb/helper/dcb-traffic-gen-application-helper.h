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

#ifndef DCB_TRAFFIC_GEN_APPLICATION_HELPER_H
#define DCB_TRAFFIC_GEN_APPLICATION_HELPER_H

#include "ns3/application-container.h"
#include "ns3/dc-topology.h"
#include "ns3/dcb-net-device.h"
#include "ns3/dcb-traffic-gen-application.h"
#include "ns3/net-device.h"
#include "ns3/object-factory.h"
#include "ns3/string.h"

namespace ns3
{

class DcbNetDevice;

class DcbTrafficGenApplicationHelper
{
  public:
    DcbTrafficGenApplicationHelper(Ptr<DcTopology> topology);

    ApplicationContainer Install(Ptr<Node> node);

    void SetProtocolGroup(DcbTrafficGenApplication::ProtocolGroup protoGroup);
    void SetCdf(std::unique_ptr<DcbTrafficGenApplication::TraceCdf> cdf);
    /**
     * Set the load without calculating the flow interval, which will be
     * calculated at install to node
     */
    void SetStartAndStopTime(Time start, Time stop);
    void SetStartTime(Time start);
    void SetRealTimeApp(bool realTimeApp);
    void SetApplicationTypeId(TypeId typeId);
    // Use move semantics to avoid copying the vector
    typedef std::pair<std::string, Ptr<AttributeValue>> ConfigEntry_t;
    void SetCcAttributes(std::vector<ConfigEntry_t>&& ccAttributes);
    void SetCcAttribute(ConfigEntry_t ccAttribute);
    void SetAppAttributes(std::vector<ConfigEntry_t>&& appAttributes);
    void SetSocketAttributes(std::vector<ConfigEntry_t>&& socketAttributes);

    /**
     * \brief Create an application according to the configuration.
     */
    Ptr<DcbBaseApplication> CreateApplication(Ptr<Node> node);

    /**
     * Record an attribute to be set in each Application after it is is created.
     *
     * \param name the name of the attribute to set
     * \param value the value of the attribute to set
     */
    void SetAppAttribute (ConfigEntry_t entry);

    /**
     * \brief Read the CDF from a file.
     *
     * \param filename The name of the file to read.
     * \returns A vector of pairs of doubles, representing the CDF.
     */
    static std::unique_ptr<std::vector<std::pair<uint32_t, double>>> ConstructCdfFromFile(
        std::string filename);
    /**
     * \brief Read the CDF from a file and scale the size according to the scaleFactor.
     *
     * \param filename The name of the file to read.
     * \param scaleFactor The scale factor to apply to the CDF.
     * \returns A vector of pairs of doubles, representing the CDF.
     */
    static std::unique_ptr<std::vector<std::pair<uint32_t, double>>> ConstructCdfFromFile(
        std::string filename,
        double scaleFactor);
    /**
     * \brief Read the CDF from a file and scale the size according to the avgSize.
     *
     * \param filename The name of the file to read.
     * \param avgSize The average size of the retruned CDF.
     * \returns A vector of pairs of doubles, representing the CDF.
     */
    static std::unique_ptr<std::vector<std::pair<uint32_t, double>>> ConstructCdfFromFile(
        std::string filename,
        uint32_t avgSize);

    /**
     * \brief Normalize the CDF's probability.
     *
     * The maximum probability of CDF should be 1.
     *
     * \param cdf The CDF to normalize.
     */
    static void NormalizeCdf(std::vector<std::pair<uint32_t, double>>* cdf);

  private:
    /**
     * Install an ns3::UdpEchoClient on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param node The node on which an UdpEchoClient will be installed.
     * \returns Ptr to the application installed.
     */
    Ptr<Application> InstallPriv(Ptr<Node> node);

    static double CalculateCdfMeanSize(const DcbTrafficGenApplication::TraceCdf* const cdf);

    Ptr<DcTopology> m_topology;
    DcbTrafficGenApplication::ProtocolGroup m_protoGroup;
    std::unique_ptr<DcbTrafficGenApplication::TraceCdf> m_cdf;
    Time m_startTime;
    Time m_stopTime;
    bool m_realTimeApp;
    
    TypeId m_applicationTypeId;
    // The attributes to be set to the ccOps
    std::vector<ConfigEntry_t> m_ccAttributes;
    // The attributes to be set to the application
    std::vector<ConfigEntry_t> m_appAttributes;
    std::vector<ConfigEntry_t> m_socketAttributes;
}; // class DcbTrafficGenApplicationHelper

} // namespace ns3

#endif // DCB_TRAFFIC_GEN_APPLICATION_HELPER_H
