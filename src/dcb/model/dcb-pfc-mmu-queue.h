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
 * Author: F.Y. Xue <xue.fyang@foxmail.com>
 */

#ifndef DCB_PFC_MMU_QUEUE_H
#define DCB_PFC_MMU_QUEUE_H

#include "dcb-flow-control-mmu-queue.h"

namespace ns3
{
class DcbPfcMmuQueue : public DcbFlowControlMmuQueue
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    /**
     * \brief Get the type ID for the instance
     * \return the instance TypeId
     */
    virtual TypeId GetInstanceTypeId(void) const override;

    DcbPfcMmuQueue();

    DcbPfcMmuQueue(uint32_t reserveSize,
                   uint32_t resumeOffset,
                   uint32_t headroomSize,
                   bool isDynamicThreshold = false,
                   uint32_t dtShift = 2);

    virtual ~DcbPfcMmuQueue();

    virtual bool CheckIngressAdmission(uint32_t packetSize) override;

    virtual bool CheckEgressAdmission(uint32_t packetSize) override
    {
        return true;
    }

    virtual void IngressIncrement(uint32_t packetSize) override;

    virtual inline void EgressIncrement(uint32_t packetSize) override
    {
    }

    virtual void IngressDecrement(uint32_t packetSize) override;

    virtual inline void EgressDecrement(uint32_t packetSize) override
    {
    }

    /**
     * \brief Get the buffer size monopolized by this queue.
     */
    virtual inline uint32_t GetExclusiveBufferSize() override
    {
        return m_reserveSize + m_headroomSize;
    }

    /**
     * \brief Get the shared buffer used by this queue.
     */
    virtual inline uint32_t GetExclusiveSharedBufferUsed() override
    {
        return m_ingressUsed > m_reserveSize ? m_ingressUsed - m_reserveSize : 0;
    }

    virtual bool CheckShouldSendPause();

    virtual bool CheckShouldSendResume();

    virtual inline uint32_t GetDynamicThreshold()
    {
        uint32_t threshold = (m_totalSharedBufferSize() - m_totalSharedBufferUsed()) >> m_dtShift;
        return threshold;
    }

  private:
    uint32_t m_reserveSize;
    uint32_t m_headroomSize;

    uint32_t m_resumeOffset;

    uint32_t m_ingressUsed;
    uint32_t m_headroomUsed;

    bool m_isDynamicThreshold;
    uint32_t m_dtShift;
};
} // namespace ns3

#endif // DCB_PFC_MMU_QUEUE_H