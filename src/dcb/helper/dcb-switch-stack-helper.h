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

#ifndef DCB_SWITCH_STACK_HELPER_H
#define DCB_SWITCH_STACK_HELPER_H

#include "ns3/fifo-queue-disc-ecn.h"
#include "ns3/internet-trace-helper.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/packet.h"
#include "ns3/pausable-queue-disc.h"
#include "ns3/ptr.h"
#include "ns3/traffic-control-layer.h"

namespace ns3
{

/**
 * \ingroup dcb
 * \brief subclass of InternetStackHelper where user can customize TrafficControlLayer
 */
class DcbSwitchStackHelper : public PcapHelperForIpv4,
                             public PcapHelperForIpv6,
                             public AsciiTraceHelperForIpv4,
                             public AsciiTraceHelperForIpv6
{
  public:
    DcbSwitchStackHelper();

    virtual ~DcbSwitchStackHelper();

    /**
     * Return helper internal state to that of a newly constructed one
     */
    void Reset(void);

    void SetTrafficControlLayer(const std::string tid);

    /**
     * \param routing a new routing helper
     *
     * Set the routing helper to use during Install. The routing
     * helper is really an object factory which is used to create
     * an object of type ns3::Ipv4RoutingProtocol per node. This routing
     * object is then associated to a single ns3::Ipv4 object through its
     * ns3::Ipv4::SetRoutingProtocol.
     */
    void SetRoutingHelper(const Ipv4RoutingHelper& routing);

    /**
     * \brief Set IPv6 routing helper.
     * \param routing IPv6 routing helper
     */
    void SetRoutingHelper(const Ipv6RoutingHelper& routing);

    /**
     * Aggregate implementations of the ns3::Ipv4, ns3::Ipv6, ns3::Udp, and ns3::Tcp classes
     * onto the provided node.  This method will assert if called on a node that
     * already has an Ipv4 object aggregated to it.
     *
     * \param nodeName The name of the node on which to install the stack.
     */
    void Install(std::string nodeName) const;

    /**
     * Aggregate implementations of the ns3::Ipv4, ns3::Ipv6, ns3::Udp, and ns3::Tcp classes
     * onto the provided node.  This method will assert if called on a node that
     * already has an Ipv4 object aggregated to it.
     *
     * \param node The node on which to install the stack.
     */
    void Install(Ptr<Node> node) const;

    /**
     * Aggregate implementations of the ns3::Ipv4, ns3::Ipv6, ns3::Udp, and ns3::Tcp classes
     * onto the provided node.  This method will assert if called on a node that
     * already has an Ipv4 object aggregated to it.
     * This method will not install flow control protocols on the ports of the switch.
     *
     * \param node The node on which to install the stack.
     */
    void InstallSwitchProtos(Ptr<Node> node) const;

    /**
     * Install flow control protocols for the ports of this switch.
     *
     * \param node The node on which to install the stack.
     */
    void InstallPortsProtos(Ptr<Node> node) const;

    /**
     * For each node in the input container, aggregate implementations of the
     * ns3::Ipv4, ns3::Ipv6, ns3::Udp, and, ns3::Tcp classes.  The program will assert
     * if this method is called on a container with a node that already has
     * an Ipv4 object aggregated to it.
     *
     * \param c NodeContainer that holds the set of nodes on which to install the
     * new stacks.
     */
    void Install(NodeContainer c) const;

    /**
     * Aggregate IPv4, IPv6, UDP, and TCP stacks to all nodes in the simulation
     */
    void InstallAll(void) const;

    void SetBufferSize(QueueSize bufSize);

    void SetFCEnabled(bool enable);

    /**
     * \brief Enable/disable IPv6 stack install.
     * \param enable enable state
     */
    void SetIpv6StackInstall(bool enable);

    /**
     * \brief Enable/disable IPv6 NS and RS Jitter.
     * \param enable enable state
     */
    void SetIpv6NsRsJitter(bool enable);

    /**
     * Assign a fixed random variable stream number to the random variables
     * used by this model.  Return the number of streams (possibly zero) that
     * have been assigned.  The Install() method should have previously been
     * called by the user.
     *
     * \param stream first stream index to use
     * \param c NodeContainer of the set of nodes for which the internet models
     *          should be modified to use a fixed stream
     * \return the number of stream indices assigned by this helper
     */
    int64_t AssignStreams(NodeContainer c, int64_t stream);

  private:
    /**
     * @brief Enable pcap output the indicated Ipv4 and interface pair.
     *
     * @param prefix Filename prefix to use for pcap files.
     * @param ipv4 Ptr to the Ipv4 interface on which you want to enable tracing.
     * @param interface Interface ID on the Ipv4 on which you want to enable tracing.
     * @param explicitFilename Treat the prefix as an explicit filename if true
     */
    virtual void EnablePcapIpv4Internal(std::string prefix,
                                        Ptr<Ipv4> ipv4,
                                        uint32_t interface,
                                        bool explicitFilename) override;

    /**
     * @brief Enable ascii trace output on the indicated Ipv4 and interface pair.
     *
     * @param stream An OutputStreamWrapper representing an existing file to use
     *               when writing trace data.
     * @param prefix Filename prefix to use for ascii trace files.
     * @param ipv4 Ptr to the Ipv4 interface on which you want to enable tracing.
     * @param interface Interface ID on the Ipv4 on which you want to enable tracing.
     * @param explicitFilename Treat the prefix as an explicit filename if true
     */
    virtual void EnableAsciiIpv4Internal(Ptr<OutputStreamWrapper> stream,
                                         std::string prefix,
                                         Ptr<Ipv4> ipv4,
                                         uint32_t interface,
                                         bool explicitFilename) override;

    /**
     * @brief Enable pcap output the indicated Ipv6 and interface pair.
     *
     * @param prefix Filename prefix to use for pcap files.
     * @param ipv6 Ptr to the Ipv6 interface on which you want to enable tracing.
     * @param interface Interface ID on the Ipv6 on which you want to enable tracing.
     * @param explicitFilename Treat the prefix as an explicit filename if true
     */
    virtual void EnablePcapIpv6Internal(std::string prefix,
                                        Ptr<Ipv6> ipv6,
                                        uint32_t interface,
                                        bool explicitFilename) override;

    /**
     * @brief Enable ascii trace output on the indicated Ipv6 and interface pair.
     *
     * @param stream An OutputStreamWrapper representing an existing file to use
     *               when writing trace data.
     * @param prefix Filename prefix to use for ascii trace files.
     * @param ipv6 Ptr to the Ipv6 interface on which you want to enable tracing.
     * @param interface Interface ID on the Ipv6 on which you want to enable tracing.
     * @param explicitFilename Treat the prefix as an explicit filename if true
     */
    virtual void EnableAsciiIpv6Internal(Ptr<OutputStreamWrapper> stream,
                                         std::string prefix,
                                         Ptr<Ipv6> ipv6,
                                         uint32_t interface,
                                         bool explicitFilename) override;

    /**
     * \brief Initialize the helper to its default values
     */
    void Initialize(void);

    /**
     * \brief IPv4 routing helper.
     */
    const Ipv4RoutingHelper* m_routing;

    /**
     * \brief IPv6 routing helper.
     */
    const Ipv6RoutingHelper* m_routingv6;

    /**
     * \brief create an object from its TypeId and aggregates it to the node
     * \param node the node
     * \param typeId the object TypeId
     */
    static void CreateAndAggregateObjectFromTypeId(Ptr<Node> node, const std::string typeId);

    /**
     * \brief checks if there is an hook to a Pcap wrapper
     * \param ipv4 pointer to the IPv4 object
     * \returns true if a hook is found
     */
    bool PcapHooked(Ptr<Ipv4> ipv4);

    /**
     * \brief checks if there is an hook to an ascii output stream
     * \param ipv4 pointer to the IPv4 object
     * \returns true if a hook is found
     */
    bool AsciiHooked(Ptr<Ipv4> ipv4);

    /**
     * \brief checks if there is an hook to a Pcap wrapper
     * \param ipv6 pointer to the IPv6 object
     * \returns true if a hook is found
     */
    bool PcapHooked(Ptr<Ipv6> ipv6);

    /**
     * \brief checks if there is an hook to an ascii output stream
     * \param ipv6 pointer to the IPv6 object
     * \returns true if a hook is found
     */
    bool AsciiHooked(Ptr<Ipv6> ipv6);

    /**
     * \brief IPv6 install state (enabled/disabled) ?
     */
    bool m_ipv6Enabled;

    /**
     * \brief IPv6 IPv6 NS and RS Jitter state (enabled/disabled) ?
     */
    bool m_ipv6NsRsJitterEnabled;

    ObjectFactory m_tcFactory;

    QueueSize m_bufferSize;

    bool m_fcEnabled;
};

} // namespace ns3

#endif /* DCB_SWITCH_STACK_HELPER_H */
