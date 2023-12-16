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

#ifndef TRACE_APPLICATION_HELPER_H
#define TRACE_APPLICATION_HELPER_H

#include "ns3/application-container.h"
#include "ns3/dc-topology.h"
#include "ns3/dcb-net-device.h"
#include "ns3/dcb-trace-application.h"
#include "ns3/net-device.h"
#include "ns3/object-factory.h"

namespace ns3
{

class DcbNetDevice;

class TraceApplicationHelper
{
  public:
    TraceApplicationHelper(Ptr<DcTopology> topology);

    ApplicationContainer Install(Ptr<Node> node);

    void SetProtocolGroup(TraceApplication::ProtocolGroup protoGroup);
    void SetCdf(std::unique_ptr<TraceApplication::TraceCdf> cdf);
    void SetLoad(Ptr<const DcbNetDevice> dev, double load);
    void SetLoad(DataRate rate, double load);
    /**
     * Set the load without calculating the flow interval, which will be
     * calculated at install to node
     */
    void SetLoad(double load);
    void CalcLoad(DataRate rate);
    void SetDestination(int32_t dest);
    void SetDestination(InetSocketAddress dest);
    void SetSendEnabled(bool enabled);
    void SetStaticFlowInterval(bool staticFlowInterval);
    void SetSendOnce(bool sendOnce);
    void SetStartAndStopTime(Time start, Time stop);
    void SetRealTimeApp(bool realTimeApp);

    /**
     * \brief Create an application according to the configuration.
     */
    Ptr<TraceApplication> CreateApplication(Ptr<Node> node);

    /**
     * Record an attribute to be set in each Application after it is is created.
     *
     * \param name the name of the attribute to set
     * \param value the value of the attribute to set
     */
    // void SetAttribute (std::string name, const AttributeValue &value);

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

    static double CalculateCdfMeanSize(const TraceApplication::TraceCdf* const cdf);

    Ptr<DcTopology> m_topology;
    TraceApplication::ProtocolGroup m_protoGroup;
    std::unique_ptr<TraceApplication::TraceCdf> m_cdf;
    double m_flowMeanInterval;
    int32_t m_destNode;
    InetSocketAddress m_destAddr;
    bool m_sendEnabled;
    double m_load;
    bool m_staticFlowInterval;
    bool m_sendOnce;
    Time m_startTime;
    Time m_stopTime;
    bool m_realTimeApp;
}; // class TraceApplicationHelper

} // namespace ns3

#endif
