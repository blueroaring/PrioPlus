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

#ifndef DCB_TRAFFIC_GEN_APPLICATION_H
#define DCB_TRAFFIC_GEN_APPLICATION_H

#include "dcb-base-application.h"
#include "dcb-net-device.h"
#include "rocev2-socket.h"
#include "udp-based-socket.h"

#include "ns3/application.h"
#include "ns3/data-rate.h"
#include "ns3/dc-topology.h"
#include "ns3/flow-identifier.h"
#include "ns3/inet-socket-address.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rocev2-header.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/traced-callback.h"

#include <map>
#include <set>
#include <string>

namespace ns3
{

class Socket;

/**
 * \ingroup dcb
 * \brief A application that generates traffic and sends it to a destination.
 */
class DcbTrafficGenApplication : public DcbBaseApplication
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
    // DcbTrafficGenApplication (Ptr<DcTopology> topology, uint32_t nodeIndex);

    /**
     * \brief Create an application in topology node nodeIndex destined to destIndex.
     * The application will send flows from nodeIndex to destIndex.
     * * If the destIndex is negative, the application will randomly choose a node as the
     * destination.
     */
    DcbTrafficGenApplication();
    DcbTrafficGenApplication(Ptr<DcTopology> topology, uint32_t nodeIndex);
    virtual ~DcbTrafficGenApplication();

    // The message size trace CDF in <size (in Byte), probility>
    typedef const std::vector<std::pair<uint32_t, double>> TraceCdf;

    void SetFlowCdf(const TraceCdf& cdf);

    void SetFlowMeanArriveInterval(double interval);

    constexpr static inline const uint64_t MSS = 1000; // bytes

    enum TrafficPattern
    {
        CDF,
        SEND_ONCE,
        FIXED_SIZE,
        RECOVERY,
        FILE_REQUEST,
        CHECKPOINT,
        RING
    };

    virtual void FlowCompletes(Ptr<UdpBasedSocket> socket) override;

    class Stats : public DcbBaseApplication::Stats
    {
      public:
        // constructor
        Stats();

        virtual ~Stats()
        {
        } // Make the base class polymorphic

        bool isCollected; //<! Whether the stats is collected

        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows);

        // No getter for simplicity
    };

    virtual std::shared_ptr<DcbBaseApplication::Stats> GetStats() const;

  private:
    /**
     * \brief Init fields, e.g., RNGs and m_socketLinkRate.
     */
    virtual void InitMembers() override;

    /**
     * \brief Schedule the next On period start
     */
    void ScheduleNextFlow(const Time& startTime);

    /**
     * \brief Get destination node index of one flow.
     * If m_destNode is negative, return a random destination.
     * Else return m_destNode.
     */
    uint32_t GetDestinationNode() const;

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
     * \brief Schedule a flow until the stop condition is satisfied.
     *
     * Now this function is only used for foreground flows.
     */
    void ScheduleAForegroundFlow();

    /**
     * \brief Generate traffic according to the traffic pattern.
     */
    virtual void GenerateTraffic() override;

    /**
     * \brief Calculate parameters of traffic.
     */
    virtual void CalcTrafficParameters() override;

    std::shared_ptr<Stats> m_stats;

    Ptr<EmpiricalRandomVariable> m_flowSizeRng; //!< Flow size random generator
    bool m_isFixedFlowArrive;                   //!< Whether the flow arrive interval is fixed
    Time m_fixedFlowArriveInterval;             //!< Fixed flow arrive interval
    Ptr<ExponentialRandomVariable> m_flowArriveTimeRng; //!< Flow arrive time random generator
    Ptr<UniformRandomVariable> m_hostIndexRng;          //!< Host index random generator
    uint32_t m_destNode; //!< if not choosing random destination, store the destined node index here
    bool m_isFixedDest;  //!< Whether the destination is fixed
    bool m_interpolate;  //!< Whether the application interpolate the flow size

    /*********************************
     * Members for traffic patterns
     *********************************/
    enum TrafficPattern m_trafficPattern; //!< The traffic pattern of the application
    uint64_t m_trafficSize;               //!< The total size of the traffic
    double m_trafficLoad;                 //!< The traffic load

    /***** Members for data recovery in storage cluster *****/
    double m_recoveryNodeRatio; //!< The ratio of recovery traffic node in all nodes

    static std::vector<Time> m_recoveryInterval; //!< The recovery interval
    static std::vector<std::set<uint32_t>>
        m_recoveryIntervalNodes;  //!< The recovery nodes in each recovery interval
    static bool m_isRecoveryInit; //!< Whether the recovery is initialized
    void CreateRecovery();

    /***** Members for file request cluster *****/
    uint32_t m_fileRequestNodesNum; //!< The number of file request nodes in the cluster
    uint32_t m_fileRequestIndex;    //!< The current file request index

    // Note that the file request mode is almost the same as the storage recovery mode, except that
    // every interval only one destination is chosen.

    static std::vector<Time> m_fileRequestInterval; //!< The file request interval
    static std::vector<std::set<uint32_t>>
        m_fileRequestIntervalNodes; //!< The file request nodes in each file request interval
    static std::vector<uint32_t> m_fileRequestIntervalDest; //!< The file request destination in
                                                            //!< each file request interval
    static bool m_isFileRequestInit; //!< Whether the file request is initialized
    void CreateFileRequest();

    /***** Members for checkpoint flows *****/
    /**
     * \brief Schedule the checkpoint flows to neighboring nodes.
     */
    void SchedulCheckpointFlow();

    /***** Members for ring flows *****/
    uint32_t m_ringGroupNodesNum; //!< The number of nodes in one ring group
    uint32_t m_ringGroupIndex;    //!< The index of the current application's ring group
    uint32_t m_ringDestNode;      //!< The destination node of the current application

    static std::vector<std::vector<Time>> m_ringIntervals; //!< The ring intervals for each group
    static bool m_isRingInit;                              //!< Whether the ring is initialized
    void CreateRing();

    /***** Members for continous foreground flows until background flows finished *****/
    enum TrafficType
    {
        FOREGROUND,
        BACKGROUND,
        NONE
    };

    TrafficType m_trafficType;                 //!< The type of the flow
    static std::vector<bool> m_bgFlowFinished; //!< Whether the background flows are finished

    void SetFlowType(std::string flowType);
}; // class DcbTrafficGenApplication

} // namespace ns3

#endif // DCB_TRAFFIC_GEN_APPLICATION_H
