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

#ifndef DCB_PFC_PORT_H
#define DCB_PFC_PORT_H

#include "dcb-flow-control-port.h"
#include "dcb-traffic-control.h"

#include "ns3/event-id.h"
#include "ns3/net-device.h"

namespace ns3
{

struct DcbPfcPortConfig
{
    struct QueueConfig
    {
        uint32_t priority, reserve, xon;

        QueueConfig(uint32_t prior, uint32_t resv, uint32_t x)
            : priority(prior),
              reserve(resv),
              xon(x)
        {
        }
    }; // struct QueueConfig

    void AddQueueConfig(uint32_t prior, uint32_t resv, uint32_t x)
    {
        queues.emplace_back(prior, resv, x);
    }

    uint32_t port;
    uint8_t enableVec = 0xff;
    std::vector<QueueConfig> queues;

}; // struct DcbPfcPortConfig

class DcbPfcPort : public DcbFlowControlPort
{
  public:
    static TypeId GetTypeId();

    DcbPfcPort(Ptr<NetDevice> dev, Ptr<DcbTrafficControl> tc);
    virtual ~DcbPfcPort();

    virtual void DoIngressProcess(Ptr<const Packet> packet,
                                  uint16_t protocol,
                                  const Address& from,
                                  const Address& to,
                                  NetDevice::PacketType packetType) override;
    /**
     * \brief Process when a packet previously came from this port is going to send
     * out though other port.
     */
    virtual void DoPacketOutCallbackProcess(uint8_t priority, Ptr<Packet> packet) override;

    /**
     * \brief Egress process. Do nothing in PFC.
     */
    virtual void DoEgressProcess(Ptr<Packet> packet) override;

    void ReceivePfc(Ptr<NetDevice> device,
                    Ptr<const Packet> p,
                    uint16_t protocol,
                    const Address& from,
                    const Address& to,
                    NetDevice::PacketType packetType);

    void ConfigQueue(uint32_t priority, uint32_t reserve, uint32_t xon);

    /**
     * \brief Set the EnableVec field of PFC to deviceIndex with enableVec.
     * enableVec should be a 8 bits variable, each bit of it represents whether
     * PFC is enabled on the corresponding priority.
     */
    void SetEnableVec(uint8_t enableVec);

  protected:
    /**
     * A port has 8 priority queues and stores an PFC enableVec
     */
    struct IngressPortInfo
    {
        struct IngressQueueInfo
        {
            uint32_t reserve; // XXX Act as xoff now
            uint32_t xon;
            bool isUpstreamPaused; // Whether the upstream priorty is paused

            EventId pauseEvent;

            IngressQueueInfo();
        }; // struct IngressQueueInfo

        explicit IngressPortInfo(uint32_t index);
        IngressPortInfo(uint32_t index, uint8_t enableVec);

        const IngressQueueInfo& getQueue(uint8_t priority) const;

        IngressQueueInfo& getQueue(uint8_t priority);

        uint32_t m_index;
        std::vector<IngressQueueInfo> m_ingressQueues;
        uint8_t m_enableVec;
    }; // struct IngressPortInfo

    bool CheckEnableVec(uint8_t cls);

    bool CheckShouldSendPause(uint8_t priority, uint32_t packetSize) const;
    void DoSendPause(uint8_t priority, const Address& from);
    bool CheckShouldSendResume(uint8_t priority) const;

    void SetUpstreamPaused(uint8_t priority, bool paused);

    /**
     * \brief Called when a sent pause frame coming to expires.
     * 
     * In this function, we check the ingress queue length, if it still exceeds
     * the threshold, we send another pause frame. By doing this, we can make
     * sure that the upstream will not send any packet to us.
     */
    void UpstreamPauseExpired(uint8_t priority, Address from);
    /**
     * \brief Calculate the pause duration given the quanta and line rate.
     */
    Time PauseDuration(uint16_t quanta, DataRate lineRate) const;

  private:
    IngressPortInfo m_port;

    std::vector<EventId> m_egressResumeEvents;

}; // class DcbPfcPort

} // namespace ns3

#endif // DCB_PFC_PORT_H
