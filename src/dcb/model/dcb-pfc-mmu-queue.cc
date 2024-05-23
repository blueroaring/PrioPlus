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

#include "dcb-pfc-mmu-queue.h"

#include "ns3/boolean.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcbPfcMmuQueue");

NS_OBJECT_ENSURE_REGISTERED(DcbPfcMmuQueue);

TypeId
DcbPfcMmuQueue::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DcbPfcMmuQueue")
            .SetParent<DcbFlowControlMmuQueue>()
            .AddConstructor<DcbPfcMmuQueue>()
            .AddAttribute("ReserveSize",
                          "The size of the reserve buffer",
                          QueueSizeValue(QueueSize("0B")),
                          MakeQueueSizeAccessor(&DcbPfcMmuQueue::SetReserveSize),
                          MakeQueueSizeChecker())
            .AddAttribute("HeadroomSize",
                          "The size of the headroom buffer",
                          QueueSizeValue(QueueSize("0B")),
                          MakeQueueSizeAccessor(&DcbPfcMmuQueue::SetHeadroomSize),
                          MakeQueueSizeChecker())
            .AddAttribute("ResumeOffset",
                          "The size of the resume offset",
                          QueueSizeValue(QueueSize("0B")),
                          MakeQueueSizeAccessor(&DcbPfcMmuQueue::SetResumeOffset),
                          MakeQueueSizeChecker())
            .AddAttribute("IsDynamicThreshold",
                          "Whether the threshold is dynamic",
                          BooleanValue(false),
                          MakeBooleanAccessor(&DcbPfcMmuQueue::m_isDynamicThreshold),
                          MakeBooleanChecker())
            .AddAttribute("DtShift",
                          "The shift of the dynamic threshold",
                          UintegerValue(2),
                          MakeUintegerAccessor(&DcbPfcMmuQueue::m_dtShift),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

TypeId
DcbPfcMmuQueue::GetInstanceTypeId() const
{
    return GetTypeId();
}

DcbPfcMmuQueue::DcbPfcMmuQueue()
    : DcbFlowControlMmuQueue(),
      m_reserveSize(0),
      m_headroomSize(0),
      m_resumeOffset(0),
      m_ingressUsed(0),
      m_headroomUsed(0),
      m_isDynamicThreshold(false),
      m_dtShift(2)
{
}

DcbPfcMmuQueue::DcbPfcMmuQueue(uint32_t reserveSize,
                               uint32_t resumeOffset,
                               uint32_t headroomSize,
                               bool isDynamicThreshold,
                               uint32_t dtShift)
    : DcbFlowControlMmuQueue(),
      m_reserveSize(reserveSize),
      m_headroomSize(headroomSize),
      m_resumeOffset(resumeOffset),
      m_ingressUsed(0),
      m_headroomUsed(0),
      m_isDynamicThreshold(isDynamicThreshold),
      m_dtShift(dtShift)
{
}

DcbPfcMmuQueue::~DcbPfcMmuQueue()
{
}

bool
DcbPfcMmuQueue::CheckIngressAdmission(uint32_t packetSize)
{
    if (m_isDynamicThreshold)
    {
        if (packetSize + m_headroomUsed <= m_headroomSize ||
            packetSize + GetExclusiveSharedBufferUsed() <= GetDynamicThreshold())
        {
            return true;
        }
        return false;
    }
    else
    {
        // if headroom has enough space or sharedbuffer has enough space, return true
        if (packetSize + m_headroomUsed <= m_headroomSize ||
            packetSize + m_totalSharedBufferUsed() <= m_totalSharedBufferSize())
        {
            return true;
        }
        return false;
    }
}

void
DcbPfcMmuQueue::IngressIncrement(uint32_t packetSize)
{
    uint32_t newIngressUsed = m_ingressUsed + packetSize;
    if (newIngressUsed <= m_reserveSize)
    {
        // if reserve has enough, use reserve space
        m_ingressUsed = newIngressUsed;
    }
    else
    {
        // since this function is called after CheckIngressAdmission returns true, we can assume
        // that either sharedbuffer or headroom has enough space
        // Trying to use sharedbuffer first
        if (m_isDynamicThreshold)
        {
            if (newIngressUsed - m_reserveSize <= GetDynamicThreshold())
            {
                m_ingressUsed = newIngressUsed;
            }
            else
            {
                m_headroomUsed += packetSize;
            }
        }
        else
        {
            if (packetSize + m_totalSharedBufferUsed() <= m_totalSharedBufferSize())
            {
                m_ingressUsed = newIngressUsed;
            }
            else
            {
                m_headroomUsed += packetSize;
            }
        }
    }
}

void
DcbPfcMmuQueue::IngressDecrement(uint32_t packetSize)
{
    // first decrement headroom if there is any used, then decrement ingress by the rest
    uint32_t headroomPart = std::min(packetSize, m_headroomUsed);
    m_headroomUsed -= headroomPart;
    m_ingressUsed -= packetSize - headroomPart;
}

bool
DcbPfcMmuQueue::CheckShouldSendPause()
{
    if (m_isDynamicThreshold)
    {
        return m_headroomUsed > 0 || GetExclusiveSharedBufferUsed() > GetDynamicThreshold();
    }
    else
    {
        return m_headroomUsed > 0;
    }
}

bool
DcbPfcMmuQueue::CheckShouldSendResume()
{
    if (m_isDynamicThreshold)
    {
        if (m_headroomUsed > 0)
        {
            return false;
        }
        if (m_totalSharedBufferSize() == 0)
        {
            return m_ingressUsed + m_resumeOffset <= m_reserveSize;
        }
        else
        {
            return GetExclusiveSharedBufferUsed() + m_resumeOffset <= GetDynamicThreshold();
        }
    }
    else
    {
        if (m_headroomUsed > 0)
        {
            return false;
        }
        if (m_totalSharedBufferSize() == 0)
        {
            // compare with ingressUsed
            return m_ingressUsed + m_resumeOffset <= m_reserveSize;
        }
        else
        {
            // compare with sharedbufferUsed
            return m_totalSharedBufferUsed() + m_resumeOffset <= m_totalSharedBufferSize();
        }
    }
    return false;
}

void
DcbPfcMmuQueue::SetReserveSize(QueueSize reserveSize)
{
    if (reserveSize.GetUnit() != QueueSizeUnit::BYTES)
    {
        NS_FATAL_ERROR("Reserve size must be in bytes");
    }
    m_reserveSize = reserveSize.GetValue();
}

void
DcbPfcMmuQueue::SetHeadroomSize(QueueSize headroomSize)
{
    if (headroomSize.GetUnit() != QueueSizeUnit::BYTES)
    {
        NS_FATAL_ERROR("Headroom size must be in bytes");
    }
    m_headroomSize = headroomSize.GetValue();
}

void
DcbPfcMmuQueue::SetResumeOffset(QueueSize resumeOffset)
{
    if (resumeOffset.GetUnit() != QueueSizeUnit::BYTES)
    {
        NS_FATAL_ERROR("Resume offset must be in bytes");
    }
    m_resumeOffset = resumeOffset.GetValue();
}

} // namespace ns3
