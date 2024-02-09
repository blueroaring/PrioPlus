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

#ifndef DCB_FLOW_CONTROL_MMU_QUEUE_H
#define DCB_FLOW_CONTROL_MMU_QUEUE_H

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/object.h"
#include "ns3/type-id.h"

namespace ns3
{
/**
 * \brief Abstract class for flow control MMU queue. This class itself does nothing.
 * All the functions are virtual and can be overridden by the derived class.
 * \ingroup dcb
 */
class DcbFlowControlMmuQueue : public Object
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

    virtual bool CheckIngressAdmission(uint32_t packetSize) = 0;

    virtual bool CheckEgressAdmission(uint32_t packetSize) = 0;

    virtual void IngressIncrement(uint32_t packetSize) = 0;

    virtual void EgressIncrement(uint32_t packetSize) = 0;

    virtual void IngressDecrement(uint32_t packetSize) = 0;

    virtual void EgressDecrement(uint32_t packetSize) = 0;

    /**
     * \brief Get the buffer size monopolized by this queue.
     * \return the exclusive buffer size
     */
    virtual uint32_t GetExclusiveBufferSize() = 0;

    /**
     * \brief Get the shared buffer used by this queue.
     * \return the shared buffer size
     */
    virtual uint32_t GetExclusiveSharedBufferUsed() = 0;

    Callback<uint32_t> m_totalSharedBufferSize;
    Callback<uint32_t> m_totalSharedBufferUsed;

    void SetBufferCallback(Callback<uint32_t> bufferSharedSize, Callback<uint32_t> bufferSharedUsed)
    {
        m_totalSharedBufferSize = bufferSharedSize;
        m_totalSharedBufferUsed = bufferSharedUsed;
    }
};
} // namespace ns3

#endif // DCB_FLOW_CONTROL_MMU_QUEUE_H