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

#ifndef REAL_TIME_APPLICATION_H
#define REAL_TIME_APPLICATION_H

#include "dcb-traffic-gen-application.h"

#include "ns3/real-time-stats-tag.h"

namespace ns3
{

class RealTimeApplication : public DcbTrafficGenApplication
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
    RealTimeApplication();
    RealTimeApplication(Ptr<DcTopology> topology, uint32_t nodeIndex);
    virtual ~RealTimeApplication();

    /**
     * The statistics of the real time application mainly record the statistics of the
     * packet received.
     */
    class Stats : public DcbTrafficGenApplication::Stats
    {
      public:
        // constructor
        Stats(Ptr<RealTimeApplication> app);
        Ptr<RealTimeApplication> m_app;

        bool isCollected; //<! Whether the stats is collected

        class FlowStats
        {
          public:
            FlowStats();

            uint32_t nMaxRecvSeq;           //<! Max sequence number of the received packet
            uint32_t nPktLoss;              // number of packet loss
            std::vector<Time> vArriveDelay; // delay from packet arrive to packet received
            std::vector<Time> vTxDelay;     // delay from packet sent to packet received

            // Variables used to calculate the average rate of real time flow
            uint32_t nTotalRecvPkts;     // total bytes of the flow
            uint64_t nTotalRecvBytes;    // total bytes of the flow
            Time tFirstPktArrive;        // time of first packet arrived
            Time tFirstPktRecv;          // time of first packet received
            Time tLastPktRecv;           // time of last packet received
            DataRate rAvgRateFromArrive; // average rate from first packet arrive
            DataRate rAvgRateFromRecv;   // average rate from first packet received

            // Detailed statistics, only enabled if needed
            bool bDetailedSenderStats;
            std::vector<std::pair<Time, uint32_t>> vRecvPkt;

            // Collect the statistics and check if the statistics is correct
            void CollectAndCheck();

            // No getter for simplicity
        };

        std::map<FlowIdentifier, std::shared_ptr<FlowStats>> mflowStats;

        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck(std::map<Ptr<Socket>, Flow*> flows);

        // No getter for simplicity
    };

    virtual std::shared_ptr<DcbBaseApplication::Stats> GetStats() const;

    constexpr static inline const uint64_t MSS = 1500; // 1500 bytes in TCP

  private:
    /**
     * \brief Send a dummy packet according to m_remainBytes.
     *
     * Raise error if packet does not sent successfully.
     * In this class, the packet will carry a RealTimeStatsTag.
     *
     * \param flow the flow to send packet.
     */
    virtual void SendNextPacket(Flow* flow);

    /**
     * \brief Handle a packet reception.
     *
     * This function is called by lower layers.
     * In this class, the packet will read the RealTimeStatsTag and record stats.
     *
     * \param socket the socket the packet was received to.
     */
    virtual void HandleRead(Ptr<Socket> socket);

    std::shared_ptr<Stats> m_stats;

    uint32_t m_pktSeq; //<! Sequence number of the sending packet
};

} // namespace ns3

#endif // REAL_TIME_APPLICATION_H
