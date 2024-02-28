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

#ifndef FIFO_QUEUE_DISC_ECN_H
#define FIFO_QUEUE_DISC_ECN_H

#include "ns3/fifo-queue-disc.h"
#include "ns3/flow-identifier.h"
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/random-variable-stream.h"
#include "ns3/data-rate.h"

namespace ns3
{

struct EcnConfig
{
    struct QueueConfig
    {
        uint32_t priority, kMin, kMax;
        double pMax;

        QueueConfig(uint32_t p, uint32_t kmin, uint32_t kmax, double pmax)
            : priority(p),
              kMin(kmin),
              kMax(kmax),
              pMax(pmax)
        {
        }
    }; // struct QueueConfig

    void AddQueueConfig(uint32_t prior, uint32_t kmin, uint32_t kmax, double pmax)
    {
        queues.emplace_back(prior, kmin, kmax, pmax);
    }

    uint32_t port;
    std::vector<QueueConfig> queues;
};

class FifoQueueDiscEcn : public FifoQueueDisc
{
  public:
    static TypeId GetTypeId();
    FifoQueueDiscEcn();
    virtual ~FifoQueueDiscEcn();

    void ConfigECN(uint32_t kmin, uint32_t kmax, double pmax);

    class Stats
    {
      public:
        // constructor
        Stats(Ptr<FifoQueueDiscEcn> qdisc);
        Ptr<FifoQueueDiscEcn> m_qdisc;

        uint32_t nMaxQLengthBytes;
        uint32_t nMaxQLengthPackets;
        uint32_t nTotalQLengthBytes;
        /**
         * When enabled, the vQLengthBytes will be recorded each packet enqueue/dequeue.
         * When disabled, the vQLengthBytes will be recorded at the end of each record interval. And
         * if in a interval there is no packet in the queue, the queue length will not be recorded.
         */
        bool bDetailedQlengthStats;
        std::vector<std::pair<Time, uint32_t>>
            vQLengthBytes; //<! Record the queue length and the time when the queue length is
                           // recorded
        TypeId backgroundCongestionTypeId; //<! The type id of the background congestion control
                                           // algorithm
        uint32_t nBackgroundQLengthBytes;  //<! The  queue length of the background congestion
                                           // control algorithm
        std::vector<std::pair<Time, uint32_t>>
            vBackgroundQLengthBytes; //<! Record the queue length and the time when the queue length
                                     // is recorded
        // Variables for the intervalic statistics
        Time m_qlengthRecordInterval;
        EventId m_qlengthRecordEvent;
        std::vector<std::tuple<Time, FlowIdentifier, uint32_t, uint32_t>>
            vEcn; //<! Record the <time, 4-tuple, psn, pkt size> when the ECN is marked

        uint32_t m_extraEgressHeaderSize;

        /**
         * When enabled, the vDeviceThroughput will be recorded each m_throughputRecordInterval.
         * When disabled, the vDeviceThroughput will not be recorded.
         */
        bool bDetailedDeviceThroughputStats;
        std::vector<std::pair<Time, DataRate>>
            vDeviceThroughput; //<! Record the throughput and the time when the throughput is recorded
        uint32_t nDequeueBytes;
        Time m_throughputRecordInterval;
        EventId m_throughputRecordEvent;

        // Recorder function
        void RecordPktEnqueue(Ptr<Ipv4QueueDiscItem> ipv4Item);
        void RecordPktDequeue(Ptr<Ipv4QueueDiscItem> ipv4Item);
        // void RecordQLengthDetailed();
        void RecordQLengthIntervalic();
        void RecordThroughputIntervalic();
        void RecordEcn(Ptr<Ipv4QueueDiscItem> ipv4Item);
        bool CheckWhetherBackgroundCongestion(Ptr<Ipv4QueueDiscItem> ipv4Item) const;

        // Collect the statistics and check if the statistics is correct
        void CollectAndCheck();

        // No getter for simplicity
    };

    std::shared_ptr<Stats> GetStats() const;

    std::shared_ptr<Stats> GetStatsWithoutCollect() const;

  private:
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    /**
     * \brief Remove the packet from the head of the queue
     *
     * This method is the same as the one in the base class, except that it record the statistics.
     */
    Ptr<QueueDiscItem> DoDequeue() override;

    bool CheckShouldMarkECN(Ptr<Ipv4QueueDiscItem> item) const;

    std::shared_ptr<Stats> m_stats;

    uint32_t m_ecnKMin;
    uint32_t m_ecnKMax;
    double m_ecnPMax;
    Ptr<UniformRandomVariable> m_rng;
}; // class FifoQueueDiscEcn

} // namespace ns3

#endif
