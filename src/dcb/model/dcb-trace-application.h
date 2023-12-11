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

#ifndef DCB_TRACE_APPLICATION_H
#define DCB_TRACE_APPLICATION_H

#include "dcb-net-device.h"
#include "rocev2-socket.h"
#include "udp-based-socket.h"

#include "ns3/application.h"
#include "ns3/data-rate.h"
#include "ns3/dc-topology.h"
#include "ns3/inet-socket-address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rocev2-header.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/traced-callback.h"
#include "ns3/flow-identifier.h"

#include <map>

namespace ns3
{

class Socket;

class TraceApplication : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * \brief Create an application in topology node nodeIndex.
     * The application will randomly choose a node as destination and send flows.
     */
    // TraceApplication (Ptr<DcTopology> topology, uint32_t nodeIndex);

    /**
     * \brief Create an application in topology node nodeIndex destined to destIndex.
     * The application will send flows from nodeIndex to destIndex.
     * * If the destIndex is negative, the application will randomly choose a node as the
     * destination.
     */
    TraceApplication(Ptr<DcTopology> topology, uint32_t nodeIndex, int32_t destIndex = -1);
    TraceApplication(Ptr<DcTopology> topology, Ptr<Node> node, InetSocketAddress destAddr);
    virtual ~TraceApplication();

    enum ProtocolGroup
    {
        RAW_UDP,
        TCP,
        RoCEv2
    };

    /**
     * \brief Assign a fixed random variable stream number to the random variables
     * used by this model.
     *
     * \param stream first stream index to use
     * \return the number of stream indices assigned by this model
     */
    // int64_t AssignStreams (int64_t stream);

    void SetProtocolGroup(ProtocolGroup protoGroup);

    void SetInnerUdpProtocol(std::string innerTid);
    void SetInnerUdpProtocol(TypeId innerTid);

    struct Flow
    {
        const Time startTime;
        Time finishTime;
        const uint64_t totalBytes;
        uint64_t remainBytes;
        uint32_t destNode;
        const Ptr<Socket> socket;
        FlowIdentifier flowIdentifier;

        Flow(uint64_t s, Time t, uint32_t dest, Ptr<Socket> sock)
            : startTime(t),
              totalBytes(s),
              remainBytes(s),
              destNode(dest),
              socket(sock)
        {
        }

        void Dispose() // to provide a similar API as ns-3
        {
            socket->Close();
            socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            delete this; // TODO: is it ok to suicide here?
        }
    };

    // The message size trace CDF in <size (in Byte), probility>
    typedef const std::vector<std::pair<uint32_t, double>> TraceCdf;

    void SetFlowCdf(const TraceCdf& cdf);

    void SetFlowMeanArriveInterval(double interval);
    /**
     * \brief Set mean arrive interval
     *
     * \param iterval The mean interval in ms.
     * \param staticFlowInterval Whether the flow interval is static.
     */
    void SetFlowMeanArriveInterval(double interval, bool staticFlowInterval);

    void SetEcnEnabled(bool enabled);

    void SetSendEnabled(bool enabled);
    void SetReceiveEnabled(bool enabled);

    void FlowCompletes(Ptr<UdpBasedSocket> socket);

    void SetDestAddr(InetSocketAddress destAddr);

    constexpr static inline const uint64_t MSS = 1000; // bytes

    static inline TraceCdf TRACE_WEBSEARCH_CDF = {{0., 0.00},
                                                  {10000., 0.15},
                                                  {20000., 0.20},
                                                  {30000., 0.30},
                                                  {50000., 0.40},
                                                  {80000., 0.53},
                                                  {200000., 0.60},
                                                  {1000000., 0.70},
                                                  {2000000., 0.80},
                                                  {5000000., 0.90},
                                                  {10000000., 0.97},
                                                  {30000000., 1.00}};

    static inline TraceCdf TRACE_FDHADOOP_CDF = {
        {0., 0.00},      {100., 0.10},    {200., 0.20},      {300., 0.50},     {350., 0.15},
        {400., 0.20},    {500., 0.30},    {600., 0.40},      {700., 0.50},     {1000., 0.60},
        {2000., 0.67},   {7000., 0.70},   {30000., 0.72},    {50000., 0.82},   {80000., 0.87},
        {120000., 0.90}, {300000., 0.95}, {1000000., 0.975}, {2000000., 0.99}, {10000000., 1.00}};

    class Stats
    {
      public:
        // constructor
        Stats();
        virtual ~Stats() {} // Make the base class polymorphic

        bool isCollected; //<! Whether the stats is collected

        uint32_t nTotalSizePkts;  //<! Data size scheduled by application
        uint64_t nTotalSizeBytes; //<! Data size scheduled by application
        uint32_t nTotalSentPkts;  //<! Data pkts sent to lower layer, including retransmission
        uint64_t nTotalSentBytes;
        uint32_t nTotalDeliverPkts; //<! Data pkts successfully delivered to peer upper layer
        uint64_t nTotalDeliverBytes;
        uint32_t nTotalLossPkts; //<! Data pkts lost and retxed
        uint64_t nTotalLossBytes;
        Time tStart;          //<! Time of the first flow start
        Time tFinish;         //<! Time of the last flow finish
        DataRate overallRate; //<! overall rate, calculate by total size / (first flow arrive -
                              // last flow finish)

        // Detailed statistics, only enabled if needed
        bool bDetailedStats;
        std::map<FlowIdentifier, std::shared_ptr<RoCEv2Socket::Stats>> mFlowStats;
        std::vector<std::shared_ptr<RoCEv2Socket::Stats>>
            vFlowStats; //<! The statistics of each flow

        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows);

        // No getter for simplicity
    };

    virtual std::shared_ptr<Stats> GetStats() const;

  protected:
    DataRate m_socketLinkRate; //!< Link rate of the deice
    uint64_t m_totBytes;       //!< Total bytes sent so far
    uint32_t m_headerSize;     //!< total header bytes of a packet
    std::map<Ptr<Socket>, Flow*> m_flows;
    Ptr<RoCEv2Socket> m_receiverSocket;

  private:
    /**
     * \brief Init fields, e.g., RNGs and m_socketLinkRate.
     */
    void InitForRngs();

    // inherited from Application base class.
    virtual void StartApplication(void) override; // Called at time specified by Start
    virtual void StopApplication(void) override;  // Called at time specified by Stop

    /**
     * \brief Schedule the next On period start
     */
    void ScheduleNextFlow(const Time& startTime);

    /**
     * \brief Create new socket to the destNode.
     *
     * Call this function requires m_topology set.
     */
    Ptr<Socket> CreateNewSocket(uint32_t destNode);

    /**
     * \brief Create new socket send to the destAddr.
     */
    Ptr<Socket> CreateNewSocket(InetSocketAddress destAddr);

    /**
     * \brief Get destination node index of one flow.
     * If m_destNode is negative, return a random destination.
     * Else return m_destNode.
     */
    uint32_t GetDestinationNode() const;
    /**
     * \brief Get the InetSocketAddress of the destNode.
     */
    InetSocketAddress NodeIndexToAddr(uint32_t destNode) const;

    /**
     * \brief Send a dummy packet according to m_remainBytes.
     * Raise error if packet does not sent successfully.
     * \param socket the socket to send packet.
     */
    virtual void SendNextPacket(Flow* flow);

    /**
     * \brief Get next random flow start time.
     */
    Time GetNextFlowArriveInterval() const;

    /**
     * \brief Get next random flow size in bytes.
     */
    uint32_t GetNextFlowSize() const;

    // helpers
    /**
     * \brief Cancel all pending events.
     */
    // void CancelEvents ();

    /**
     * \brief Handle a Connection Succeed event
     * \param socket the connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);

    /**
     * \brief Handle a packet reception.
     *
     * This function is called by lower layers.
     *
     * \param socket the socket the packet was received to.
     */
    virtual void HandleRead(Ptr<Socket> socket);

    /**
     * \brief Find a outbound net device (i.e., not a loopback net device) for the application's
     * node.
     * \return a outbound net device (typically the only outbound net device).
     */
    Ptr<NetDevice> GetOutboundNetDevice();

    void SetFlowIdentifier(Flow* flow, Ptr<Socket> socket);

    std::shared_ptr<Stats> m_stats;

    bool m_enableSend;
    bool m_enableReceive;
    const Ptr<DcTopology> m_topology; //!< The topology
    const uint32_t m_nodeIndex;
    Ptr<Node> m_node; //!< The node the application is installed on, used when dctopo is not set
    bool m_ecnEnabled;
    // bool                   m_connected;       //!< True if connected
    TypeId m_socketTid;         //!< Type of the socket used
    ProtocolGroup m_protoGroup; //!< Protocol group
    TypeId m_innerUdpProtocol;  //!< inner-UDP protocol type id

    Ptr<EmpiricalRandomVariable> m_flowSizeRng;         //!< Flow size random generator
    Time m_staticFlowArriveInterval;                    //!< Static flow arrive interval
    Ptr<ExponentialRandomVariable> m_flowArriveTimeRng; //!< Flow arrive time random generator
    Ptr<UniformRandomVariable> m_hostIndexRng;          //!< Host index random generator
    int32_t m_destNode; //!< if not choosing random destination, store the destined node index here
    InetSocketAddress
        m_destAddr; //!< if not choosing random destination, store the destined address here

    /// traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

    /// Callbacks for tracing the packet Tx events, includes source and destination addresses
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithAddresses;

    /// Callback for tracing the packet Tx events, includes source, destination, the packet sent,
    /// and header
    TracedCallback<Ptr<const Packet>, const Address&, const Address&, const SeqTsSizeHeader&>
        m_txTraceWithSeqTsSize;

    TracedCallback<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, Time, Time>
        m_flowCompleteTrace;
}; // class TraceApplication

} // namespace ns3

#endif // DCB_TRACE_APPLICATION_H
