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

#include "pausable-queue-disc.h"

#include "dcb-traffic-control.h"
#include "fifo-queue-disc-ecn.h"

#include "ns3/assert.h"
#include "ns3/boolean.h"
#include "ns3/fatal-error.h"
#include "ns3/integer.h"
#include "ns3/log-macros-enabled.h"
#include "ns3/log.h"
#include "ns3/object-base.h"
#include "ns3/object-factory.h"
#include "ns3/queue-disc.h"
#include "ns3/queue-item.h"
#include "ns3/queue-size.h"
#include "ns3/random-variable-stream.h"
#include "ns3/simulator.h"
#include "ns3/type-id.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("PausableQueueDisc");

NS_OBJECT_ENSURE_REGISTERED(PausableQueueDisc);

TypeId
PausableQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::PausableQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("Dcb")
            .AddConstructor<PausableQueueDisc>()
            .AddAttribute("FcEnabled",
                          "Wether flow control is enabled",
                          BooleanValue(false),
                          MakeBooleanAccessor(&PausableQueueDisc::m_fcEnabled),
                          MakeBooleanChecker())
            .AddAttribute("TrafficControlCallback",
                          "Callback when deque completed",
                          CallbackValue(MakeNullCallback<void, uint32_t, uint32_t, Ptr<Packet>>()),
                          MakeCallbackAccessor(&PausableQueueDisc::m_tcEgress),
                          MakeCallbackChecker());
    return tid;
}

PausableQueueDisc::PausableQueueDisc()
    : m_fcEnabled(false),
      m_portIndex(0x7fffffff),
      m_queueSize("1000p")
{
    NS_LOG_FUNCTION(this);
}

PausableQueueDisc::PausableQueueDisc(uint32_t port)
    : m_fcEnabled(false),
      m_portIndex(port),
      m_queueSize("1000p")
{
    NS_LOG_FUNCTION(this);
}

PausableQueueDisc::~PausableQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

Ptr<PausableQueueDiscClass>
PausableQueueDisc::GetQueueDiscClass(std::size_t i) const
{
    NS_LOG_FUNCTION(this);
    return DynamicCast<PausableQueueDiscClass>(QueueDisc::GetQueueDiscClass(i));
}

void
PausableQueueDisc::Run()
{
    NS_LOG_FUNCTION(this);
    if (RunBegin())
    {
        // TODO: Not supporting Requeue () at this moment
        Ptr<QueueDiscItem> item = DoDequeue();
        if (item)
        {
            NS_ASSERT_MSG(m_send, "Send callback not set");
            item->AddHeader();
            m_send(item); // m_send is usually set to NetDevice::Send ()
        }
        else
        {
            RunEnd(); // release if queue is empty
        }
    }
    // RunEnd () is called by DcbNetDevice::TransmitComplete ()
}

void
PausableQueueDisc::SetPortIndex(uint32_t portIndex)
{
    NS_LOG_FUNCTION(this << portIndex);
    m_portIndex = portIndex;
}

void
PausableQueueDisc::SetFCEnabled(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_fcEnabled = enable;
}

void
PausableQueueDisc::SetQueueSize(QueueSize qSize)
{
    m_queueSize = qSize;
}

void
PausableQueueDisc::SetPaused(uint8_t priority, bool paused)
{
    NS_LOG_FUNCTION(this);
    GetQueueDiscClass(priority)->SetPaused(paused);
}

void
PausableQueueDisc::RegisterTrafficControlCallback(TCEgressCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_tcEgress = cb;
}

bool
PausableQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    // TODO: Use Classify to call PacketFilter

    // Get priority from packet tag.
    // We use tag rather than DSCP field to get the priority because in this way
    // we can use different strategies to set priority.
    CoSTag cosTag;
    if (item->GetPacket()->RemovePacketTag(cosTag))
    {
        uint8_t priority = cosTag.GetCoS() & 0x0f;
        NS_ASSERT_MSG(priority < 8, "Priority should be 0~7 but here we have " << priority);
        Ptr<PausableQueueDiscClass> qdiscClass = GetQueueDiscClass(priority);
        bool retval = qdiscClass->GetQueueDisc()->Enqueue(item);
        if (!retval)
        {
            NS_LOG_WARN("PausableQueueDisc: enqueue failed on node "
                        << Simulator::GetContext()
                        << ", queue size=" << qdiscClass->GetQueueDisc()->GetCurrentSize());
        }
        return retval;
    }
    NS_FATAL_ERROR("No priority tag set one the item" << item);
    return false;
}

Ptr<QueueDiscItem>
PausableQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);
    Ptr<QueueDiscItem> item = 0;

    for (uint32_t i = GetNQueueDiscClasses(); i-- > 0;)
    {
        Ptr<PausableQueueDiscClass> qdclass = GetQueueDiscClass(i);
        if ((!m_fcEnabled || !qdclass->IsPaused()) &&
            (item = qdclass->GetQueueDisc()->Dequeue()) != 0)
        {
            NS_LOG_LOGIC("Popoed from priority " << i << ": " << item);
            m_tcEgress(m_portIndex, i, item->GetPacket());
            return item;
        }
    }
    NS_LOG_LOGIC("Queue empty");
    return item;
}

Ptr<const QueueDiscItem>
PausableQueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);
    Ptr<const QueueDiscItem> item;

    for (uint32_t i = 0; i < GetNQueueDiscClasses(); i++)
    {
        Ptr<PausableQueueDiscClass> qdclass = GetQueueDiscClass(i);
        if ((!m_fcEnabled || !qdclass->IsPaused()) &&
            (item = qdclass->GetQueueDisc()->Dequeue()) != 0)
        {
            NS_LOG_LOGIC("Peeked from priority " << i << ": " << item);
            return item;
        }
    }

    NS_LOG_LOGIC("Queue empty");
    return item;
}

bool
PausableQueueDisc::CheckConfig(void)
{
    NS_LOG_FUNCTION(this);
    if (GetNInternalQueues() > 0)
    {
        NS_LOG_ERROR("PausableQueueDisc cannot have internal queues");
        return false;
    }
    // if (m_fcEnabled && GetQuota () != 1)
    //   {
    //     NS_LOG_ERROR ("Quota of PausableQueueDisc should be 1");
    //     return false;
    //   }
    if (GetNQueueDiscClasses() == 0)
    {
        // create 8 fifo queue discs
        ObjectFactory factory;
        factory.SetTypeId("ns3::FifoQueueDiscEcn");
        factory.Set("MaxSize", QueueSizeValue(m_queueSize));
        for (uint8_t i = 0; i < 8; i++)
        {
            Ptr<QueueDisc> qd = factory.Create<QueueDisc>();
            qd->Initialize();
            Ptr<PausableQueueDiscClass> c = CreateObject<PausableQueueDiscClass>();
            c->SetQueueDisc(qd);
            AddQueueDiscClass(c);
        }
    }
    return true;
}

void
PausableQueueDisc::InitializeParams(void)
{
    NS_LOG_FUNCTION(this);
}

TypeId
PausableQueueDiscClass::GetTypeId()
{
    static TypeId tid = TypeId("ns3::PausableQueueDiscClass")
                            .SetParent<QueueDiscClass>()
                            .SetGroupName("Dcb")
                            .AddConstructor<PausableQueueDiscClass>();
    return tid;
}

PausableQueueDiscClass::PausableQueueDiscClass()
    : m_isPaused(false)
{
    NS_LOG_FUNCTION(this);
}

PausableQueueDiscClass::~PausableQueueDiscClass()
{
    NS_LOG_FUNCTION(this);
}

bool
PausableQueueDiscClass::IsPaused() const
{
    return m_isPaused;
}

void
PausableQueueDiscClass::SetPaused(bool paused)
{
    m_isPaused = paused;
}

} // namespace ns3
