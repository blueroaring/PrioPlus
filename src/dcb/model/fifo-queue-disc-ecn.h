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
#include "ns3/ipv4-queue-disc-item.h"
#include "ns3/random-variable-stream.h"

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

  private:
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;

    bool CheckShouldMarkECN(Ptr<Ipv4QueueDiscItem> item) const;

    uint32_t m_ecnKMin;
    uint32_t m_ecnKMax;
    double m_ecnPMax;
    Ptr<UniformRandomVariable> m_rng;

}; // class FifoQueueDiscEcn

} // namespace ns3

#endif
