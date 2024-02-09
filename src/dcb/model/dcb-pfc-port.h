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
#include "dcb-pfc-mmu-queue.h"
#include "dcb-traffic-control.h"

#include "ns3/event-id.h"
#include "ns3/net-device.h"

namespace ns3
{

struct DcbPfcPortConfig
{
    struct QueueConfig
    {
        uint32_t priority, reserve, resumeOffset, headroom;
        bool isDynamicThreshold;
        uint32_t dtShift;

        QueueConfig(uint32_t prior,
                    uint32_t resv,
                    uint32_t reso,
                    uint32_t hdr,
                    bool isDt = false,
                    uint32_t dtS = 2)
            : priority(prior),
              reserve(resv),
              resumeOffset(reso),
              headroom(hdr),
              isDynamicThreshold(isDt),
              dtShift(dtS)
        {
        }
    }; // struct QueueConfig

    void AddQueueConfig(uint32_t prior,
                        uint32_t resv,
                        uint32_t reso,
                        uint32_t hdr,
                        bool isDt = false,
                        uint32_t dtS = 2)
    {
        queues.emplace_back(prior, resv, reso, hdr, isDt, dtS);
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

    virtual void DoIngressProcess(Ptr<NetDevice> outDev, Ptr<QueueDiscItem> item) override;
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

    bool CheckShouldSendPause(uint8_t priority) const;
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
    std::pair<uint32_t, uint32_t> GetNodeAndPortId() const;

    IngressPortInfo m_port;

    std::vector<EventId> m_egressResumeEvents;

    enum PFCExpireReactionType
    {
        RESEND_PAUSE,
        RESET_UPSTREAM_PAUSED,
        NEVER_EXPIRE
    }; // enum PFCExpireReactionType
    const enum PFCExpireReactionType m_reactionType = RESEND_PAUSE;

    /// Traced callback: fired a PFC frame is sent, trace with node and port id, priority, pause or
    /// resume
    TracedCallback<std::pair<uint32_t, uint32_t>, uint8_t, bool> m_tracePfcSent;
    /// Traced callback: fired when a pause frame is received, trace with node and port id,
    /// priority, pause or resume
    TracedCallback<std::pair<uint32_t, uint32_t>, uint8_t, bool> m_tracePfcReceived;

}; // class DcbPfcPort

} // namespace ns3

#endif // DCB_PFC_PORT_H
