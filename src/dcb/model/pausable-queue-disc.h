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

#ifndef PAUSABLE_QUEUE_DISC_H
#define PAUSABLE_QUEUE_DISC_H

#include "ns3/queue-disc.h"
#include "ns3/queue-item.h"
#include "ns3/type-id.h"
#include "ns3/node.h"

namespace ns3
{

struct EcnConfig;

class PausableQueueDiscClass : public QueueDiscClass
{
  public:
    static TypeId GetTypeId();

    PausableQueueDiscClass();
    virtual ~PausableQueueDiscClass();

    bool IsPaused() const;

    void SetPaused(bool paused);

  private:
    bool m_isPaused;
}; // PausableQueueDiscClass

class PausableQueueDisc : public QueueDisc
{
  public:
    static TypeId GetTypeId(void);

    PausableQueueDisc();
    PausableQueueDisc(uint32_t port);
    PausableQueueDisc(Ptr<Node> node, uint32_t port);

    virtual ~PausableQueueDisc();

    /**
     * \brief Get the i-th queue disc class
     * \param i the index of the queue disc class
     * \return the i-th queue disc class.
     */
    Ptr<PausableQueueDiscClass> GetQueueDiscClass(std::size_t i) const;

    virtual void Run(void) override;

    void SetPortIndex(uint32_t portIndex);
    void SetFCEnabled(bool enable);
    void SetQueueSize(QueueSize qSize);

    void SetPaused(uint8_t priority, bool paused);

    typedef Callback<void, uint32_t, uint8_t, Ptr<Packet>> TCEgressCallback;

    /**
     * \brief Called when install the qdisc, to register the callback called after packet is dequeued
     */
    void RegisterTrafficControlCallback(TCEgressCallback cb);

    /**
     * \brief Get the i-th inner queue size.
     * Used by host's socket to determine whether to send more packets down.
     */
    QueueSize GetInnerQueueSize(uint8_t priority) const;

    std::pair<uint32_t, uint32_t> GetNodeAndPortId() const;

  private:
    virtual bool DoEnqueue(Ptr<QueueDiscItem> item) override;

    virtual Ptr<QueueDiscItem> DoDequeue(void) override;

    virtual Ptr<const QueueDiscItem> DoPeek(void) override;

    virtual bool CheckConfig(void) override;

    virtual void InitializeParams(void) override;

    Ptr<Node> m_node;       //!< Node owning this NetDevice

    bool m_fcEnabled; // Whether flow control is enabled, once enabled, the queue disc will be pausable

    TCEgressCallback m_tcEgress; // Costum callback, called after packet is dequeued

    int32_t m_portIndex; //!< the port index this QueueDisc belongs to

    QueueSize m_queueSize;

    /// Traced callback: fired when a packet is enqueued, trace with item, host and port id, priority
    TracedCallback<Ptr<const QueueDiscItem>, std::pair<uint32_t, uint32_t>, uint8_t> m_traceEnqueueWithId;

}; // class PausableQueueDisc

} // namespace ns3

#endif // PAUSABLE_QUEUE_DISC_H
