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
#include "ns3/global-value.h"
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
#include "ns3/socket.h"
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
                          "Whether flow control is enabled",
                          BooleanValue(false),
                          MakeBooleanAccessor(&PausableQueueDisc::m_fcEnabled),
                          MakeBooleanChecker())
            .AddAttribute("TrafficControlCallback",
                          "Callback when deque completed",
                          CallbackValue(MakeNullCallback<void, uint32_t, uint32_t, Ptr<Packet>>()),
                          MakeCallbackAccessor(&PausableQueueDisc::m_tcEgress),
                          MakeCallbackChecker())
            .AddTraceSource("EnqueueWithId",
                            "Enqueue a packet in the queue disc",
                            MakeTraceSourceAccessor(&PausableQueueDisc::m_traceEnqueueWithId),
                            "ns3::QueueDiscItem::TracedCallback");
    return tid;
}

PausableQueueDisc::PausableQueueDisc()
    : m_node(0),
      m_fcEnabled(false),
      m_portIndex(0x7fffffff),
      m_queueSize("1000p"),
      m_stats(std::make_shared<Stats>(this))
{
    NS_LOG_FUNCTION(this);
}

PausableQueueDisc::PausableQueueDisc(uint32_t port)
    : m_node(0),
      m_fcEnabled(false),
      m_portIndex(port),
      m_queueSize("1000p"),
      m_stats(std::make_shared<Stats>(this))
{
    NS_LOG_FUNCTION(this);
}

PausableQueueDisc::PausableQueueDisc(Ptr<Node> node, uint32_t port)
    : m_node(node),
      m_fcEnabled(false),
      m_portIndex(port),
      m_queueSize("1000p"),
      m_stats(std::make_shared<Stats>(this))
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
    Ptr<QueueDiscClass> q = QueueDisc::GetQueueDiscClass(i);
    return DynamicCast<PausableQueueDiscClass>(q);
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
PausableQueueDisc::SetPaused(uint32_t priority, bool paused)
{
    NS_LOG_FUNCTION(this);
    GetQueueDiscClass(priority)->SetPaused(paused);

    // If the queue is resumed, we need to rerun the qdisc
    if (paused == false)
    {
        Run();
    }

    m_stats->RecordPauseResume(priority, paused);
}

void
PausableQueueDisc::RegisterTrafficControlCallback(TCEgressCallback cb)
{
    NS_LOG_FUNCTION(this);
    m_tcEgress = cb;
}

QueueSize
PausableQueueDisc::GetInnerQueueSize(uint32_t priority) const
{
    NS_LOG_FUNCTION(this);
    Ptr<PausableQueueDiscClass> clas = GetQueueDiscClass(priority);
    Ptr<QueueDisc> qdisc = clas->GetQueueDisc();
    QueueSize ans = qdisc->GetCurrentSize();
    return ans;
    // return GetQueueDiscClass(priority)->GetQueueDisc()->GetCurrentSize();
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
    uint32_t priority;
    if (item->GetPacket()->PeekPacketTag(
            cosTag)) // costag should be removed in DcbTrafficControl::EgressProcess
    {
        priority = cosTag.GetCoS() & 0x0f;
    }
    else
    {
        // If the packet doesn't have CoSTag, we use the TOS field in IP header.
        // In this branch, we assume the node is host and the l3 proto is IPv4.
        Ptr<Ipv4QueueDiscItem> ipv4Qdi = DynamicCast<Ipv4QueueDiscItem>(item);
        if (ipv4Qdi == nullptr)
        {
            NS_LOG_ERROR("PausableQueueDisc: could not find the packet's priority");
            return false;
        }
        priority = Socket::IpTos2Priority(ipv4Qdi->GetHeader().GetTos());
    }
    NS_ASSERT_MSG(priority < 8, "Priority should be 0~7 but here we have " << priority);

    Ptr<PausableQueueDiscClass> qdiscClass = GetQueueDiscClass(priority);
    bool retval = qdiscClass->GetQueueDisc()->Enqueue(item);
    if (!retval)
    {
        NS_LOG_WARN("PausableQueueDisc: enqueue failed on node "
                    << Simulator::GetContext()
                    << ", queue size=" << qdiscClass->GetQueueDisc()->GetCurrentSize());
    }
    m_traceEnqueueWithId(item, GetNodeAndPortId(), priority);
    return retval;
}

Ptr<QueueDiscItem>
PausableQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);
    Ptr<QueueDiscItem> item = 0;

    // The strict priority is implemented
    // The order is from high to low priority
    for (uint32_t i = GetNQueueDiscClasses(); i-- > 0;)
    {
        Ptr<PausableQueueDiscClass> qdclass = GetQueueDiscClass(i);
        if ((!m_fcEnabled || !qdclass->IsPaused()) &&
            (item = qdclass->GetQueueDisc()->Dequeue()) != nullptr)
        {
            NS_LOG_LOGIC("Popoed from priority " << i << ": " << item);

            // If the qdice is empty after dequeue, try to call the m_sendDataCb
            if (qdclass->GetQueueDisc()->GetNBytes() == 0)
            {
                // If we are at switch, the m_sendData Callback is null
                if (!m_sendDataCallback.IsNull())
                    // Note that the first device is LoopbackNetDevice, but this is not safe
                    m_sendDataCallback(m_portIndex, i);
            }

            if (!m_tcEgress.IsNull())
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
            (item = qdclass->GetQueueDisc()->Dequeue()) != nullptr)
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

    // If no queue disc class is set
    if (GetNQueueDiscClasses() == 0)
    {
        // create 8 fifo queue discs
        ObjectFactory factory;
        factory.SetTypeId("ns3::FifoQueueDiscEcn");
        // Each inner fifo queue's size is equal to the total queue size
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

std::pair<uint32_t, uint32_t>
PausableQueueDisc::GetNodeAndPortId() const
{
    return std::make_pair(m_node->GetId(), m_portIndex);
}

void
PausableQueueDisc::InitializeParams(void)
{
    NS_LOG_FUNCTION(this);
}

void
PausableQueueDisc::RegisterSendDataCallback(Callback<void, uint32_t, uint32_t> cb)
{
    NS_LOG_FUNCTION(this);
    m_sendDataCallback = cb;
}

std::shared_ptr<PausableQueueDisc::Stats>
PausableQueueDisc::GetStats() const
{
    m_stats->CollectAndCheck();
    return m_stats;
}

void
PausableQueueDisc::SetDetailedSwitchStats(bool bDetailedQlengthStats)
{
    m_stats->bDetailedQlengthStats = bDetailedQlengthStats;
    for (uint8_t i = 0; i < 8; i++)
    {
        Ptr<FifoQueueDiscEcn> qd =
            DynamicCast<FifoQueueDiscEcn>(GetQueueDiscClass(i)->GetQueueDisc());
        if (qd == nullptr)
        {
            // Here we assume the inner queue is FifoQueueDiscEcn
            NS_LOG_ERROR("PausableQueueDisc: cannot cast inner queue to FifoQueueDiscEcn");
            return;
        }
        qd->GetStatsWithoutCollect()->bDetailedQlengthStats = bDetailedQlengthStats;
    }
}

PausableQueueDisc::Stats::Stats(Ptr<PausableQueueDisc> qdisc)
    : m_qdisc(qdisc)
{
    // Retrieve the global config values
    BooleanValue bv;
    if (GlobalValue::GetValueByNameFailSafe("detailedQlengthStats", bv))
        bDetailedQlengthStats = bv.Get();
    else
        bDetailedQlengthStats = false;
}

void
PausableQueueDisc::Stats::RecordPauseResume(uint32_t prio, bool paused)
{
    if (bDetailedQlengthStats)
    {
        vPauseResumeTime.emplace_back(Simulator::Now(), prio, paused);
    }
}

void
PausableQueueDisc::Stats::CollectAndCheck()
{
    // Collect the statistics from each inner queue
    for (uint8_t i = 0; i < 8; i++)
    {
        Ptr<FifoQueueDiscEcn> qd =
            DynamicCast<FifoQueueDiscEcn>(m_qdisc->GetQueueDiscClass(i)->GetQueueDisc());
        if (qd == nullptr)
        {
            // Here we assume the inner queue is FifoQueueDiscEcn
            NS_LOG_ERROR("PausableQueueDisc: cannot cast inner queue to FifoQueueDiscEcn");
            return;
        }
        vQueueStats.emplace_back(qd->GetStats());
    }
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
