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

#include "fifo-queue-disc-ecn.h"

#include "rocev2-congestion-ops.h"
#include "rocev2-l4-protocol.h"

#include "ns3/abort.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ethernet-header.h"
#include "ns3/global-value.h"
#include "ns3/simulator.h"
#include "ns3/string.h"

#include <cstdint>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("FifoQueueDiscEcn");

NS_OBJECT_ENSURE_REGISTERED(FifoQueueDiscEcn);

TypeId
FifoQueueDiscEcn::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FifoQueueDiscEcn")
                            .SetParent<FifoQueueDisc>()
                            .SetGroupName("dcb")
                            .AddConstructor<FifoQueueDiscEcn>();
    return tid;
}

FifoQueueDiscEcn::FifoQueueDiscEcn()
    : m_ecnKMin(UINT32_MAX - 1),
      m_ecnKMax(UINT32_MAX),
      m_ecnPMax(0.)
{
    NS_LOG_FUNCTION(this);
    m_stats = std::make_shared<Stats>(this);
}

FifoQueueDiscEcn::~FifoQueueDiscEcn()
{
    NS_LOG_FUNCTION(this);
}

bool
FifoQueueDiscEcn::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    if (GetCurrentSize() + item > GetMaxSize())
    {
        NS_LOG_LOGIC("Queue full -- dropping pkt");
        DropBeforeEnqueue(item, LIMIT_EXCEEDED_DROP);
        return false;
    }

    bool retval = GetInternalQueue(0)->Enqueue(item);

    // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
    // internal queue because QueueDisc::AddInternalQueue sets the trace callback

    NS_LOG_LOGIC("Number packets " << GetInternalQueue(0)->GetNPackets());
    NS_LOG_LOGIC("Number bytes " << GetInternalQueue(0)->GetNBytes());

    m_stats->RecordPktEnqueue(DynamicCast<Ipv4QueueDiscItem>(item));

    return retval;
}

Ptr<QueueDiscItem>
FifoQueueDiscEcn::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();

    Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem>(item);
    if (ipv4Item && CheckShouldMarkECN(ipv4Item))
    {
        NS_LOG_DEBUG("Switch " << Simulator::GetContext()
                               << " FifoQueueDiscEcn marks ECN on packet");
        ipv4Item->Mark();
        m_stats->RecordEcn(ipv4Item);
    }

    if (!item)
    {
        NS_LOG_LOGIC("Queue empty");
        return nullptr;
    }

    m_stats->RecordPktDequeue(ipv4Item);

    return item;
}

void
FifoQueueDiscEcn::ConfigECN(uint32_t kmin, uint32_t kmax, double pmax)
{
    NS_LOG_FUNCTION(this);

    if (kmin >= kmax)
    {
        NS_FATAL_ERROR("ECN kMin should be smaller than kMax");
    }
    if (pmax > 1.0 || pmax < 0.)
    {
        NS_FATAL_ERROR("ECN pMAx should be between 0 and 1");
    }
    m_ecnKMin = kmin;
    m_ecnKMax = kmax;
    m_ecnPMax = pmax;

    m_rng = CreateObject<UniformRandomVariable>();
    m_rng->SetAttribute("Min", DoubleValue(0.0));
    m_rng->SetAttribute("Max", DoubleValue(1.0));
}

bool
FifoQueueDiscEcn::CheckShouldMarkECN(Ptr<Ipv4QueueDiscItem> item) const
{
    NS_LOG_FUNCTION(this << item);
    uint32_t nbytes = GetNBytes() + item->GetPacket()->GetSize();
    if (nbytes <= m_ecnKMin)
    {
        return false;
    }
    else if (nbytes >= m_ecnKMax)
    {
        return true;
    }
    else
    { // mark ECN with probability
        // multiplied by 1024 to improve precision
        double prob = m_ecnPMax * 1024 * (nbytes - m_ecnKMin) / (m_ecnKMax - m_ecnKMin);
        return m_rng->GetValue() * 1024 < prob;
    }
}

std::shared_ptr<FifoQueueDiscEcn::Stats>
FifoQueueDiscEcn::GetStats() const
{
    m_stats->CollectAndCheck();
    return m_stats;
}

std::shared_ptr<FifoQueueDiscEcn::Stats>
FifoQueueDiscEcn::GetStatsWithoutCollect() const
{
    return m_stats;
}

FifoQueueDiscEcn::Stats::Stats(Ptr<FifoQueueDiscEcn> qdisc)
    : m_qdisc(qdisc),
      nMaxQLengthBytes(0),
      nMaxQLengthPackets(0),
      nTotalQLengthBytes(0),
      nBackgroundQLengthBytes(0),
      m_recordInterval(Seconds(0)),
      m_recordEvent(EventId())
{
    // Retrieve the global config values
    BooleanValue bv;
    if (GlobalValue::GetValueByNameFailSafe("detailedSwitchStats", bv))
        bDetailedSwitchStats = bv.Get();
    else
        bDetailedSwitchStats = false;

    // If detailedSwitchStats is disabled, record the qlength intervalic
    if (!bDetailedSwitchStats)
    {
        StringValue sv;
        if (GlobalValue::GetValueByNameFailSafe("switchRecordInterval", sv))
            m_recordInterval = Time(sv.Get());
        else
            NS_FATAL_ERROR(
                "Cannot find switchRecordInterval while detailedSwitchStats is disabled");

        // Do not need to schedule the record event as it will be scheduled when the first packet
        // enqueue
    }

    StringValue sv;
    if (GlobalValue::GetValueByNameFailSafe("backgroundCongestionType", sv))
    {
        backgroundCongestionTypeId = TypeId::LookupByName(sv.Get());
        // If cannot find the type id, TypeId will fail with a fatal error
    }
    else
    {
        backgroundCongestionTypeId = TypeId();
    }

    EthernetHeader ethHeader;
    m_extraEgressHeaderSize = ethHeader.GetSerializedSize();
}

bool
FifoQueueDiscEcn::Stats::CheckWhetherBackgroundCongestion(Ptr<Ipv4QueueDiscItem> ipv4Item) const
{
    if (backgroundCongestionTypeId != TypeId())
    {
        CongestionTypeTag ctTag;
        if (ipv4Item->GetPacket()->PeekPacketTag(ctTag))
        {
            if (ctTag.GetCongestionTypeId() == backgroundCongestionTypeId)
                return true;
        }
    }
    return false;
}

void
FifoQueueDiscEcn::Stats::RecordPktEnqueue(Ptr<Ipv4QueueDiscItem> ipv4Item)
{
    nMaxQLengthBytes = std::max(nMaxQLengthBytes, m_qdisc->GetNBytes());
    nMaxQLengthPackets = std::max(nMaxQLengthPackets, m_qdisc->GetNPackets());

    nTotalQLengthBytes += ipv4Item->GetSize() + m_extraEgressHeaderSize;
    // Record the queuelength of the background congestion control algorithm
    if (CheckWhetherBackgroundCongestion(ipv4Item))
    {
        nBackgroundQLengthBytes += ipv4Item->GetSize() + m_extraEgressHeaderSize;
    }

    if (bDetailedSwitchStats)
    {
        vQLengthBytes.push_back(std::make_pair(Simulator::Now(), nTotalQLengthBytes));
        vBackgroundQLengthBytes.push_back(
            std::make_pair(Simulator::Now(), nBackgroundQLengthBytes));
    }
    else
    {
        if (m_recordEvent.IsExpired())
        {
            // If the record event is expired, record the queue length and this will reschedule the
            // record event
            RecordQLengthIntervalic();
        }
    }
}

void
FifoQueueDiscEcn::Stats::RecordPktDequeue(Ptr<Ipv4QueueDiscItem> ipv4Item)
{
    nTotalQLengthBytes -= ipv4Item->GetSize() + m_extraEgressHeaderSize;
    // Record the queuelength of the background congestion control algorithm
    if (CheckWhetherBackgroundCongestion(ipv4Item))
    {
        nBackgroundQLengthBytes -= ipv4Item->GetSize() + m_extraEgressHeaderSize;
    }

    if (bDetailedSwitchStats)
    {
        vQLengthBytes.push_back(std::make_pair(Simulator::Now(), nTotalQLengthBytes));
        vBackgroundQLengthBytes.push_back(
            std::make_pair(Simulator::Now(), nBackgroundQLengthBytes));
    }
}

void
FifoQueueDiscEcn::Stats::RecordQLengthIntervalic()
{
    if (!bDetailedSwitchStats)
    {
        if (nTotalQLengthBytes!=0)
        {
            vQLengthBytes.push_back(std::make_pair(Simulator::Now(), nTotalQLengthBytes));
            vBackgroundQLengthBytes.push_back(
                std::make_pair(Simulator::Now(), nBackgroundQLengthBytes));
            // Reschedule the record event only when the queue is not empty
            m_recordEvent = Simulator::Schedule(m_recordInterval,
                                                &FifoQueueDiscEcn::Stats::RecordQLengthIntervalic,
                                                this);
        }
        else
        {
            NS_LOG_DEBUG("Queue is empty, do not record the queue length");
        }
    }
}

void
FifoQueueDiscEcn::Stats::RecordEcn(Ptr<Ipv4QueueDiscItem> ipv4Item)
{
    if (bDetailedSwitchStats)
    {
        // Check if the packet is a UDP-RoCEv2 packet
        bool isRocev2 = false;
        Ipv4Header ipv4Header = ipv4Item->GetHeader();
        Ptr<Packet> copy = ipv4Item->GetPacket()->Copy();
        // const uint8_t TCP_PROT_NUMBER = 6;  //!< TCP Protocol number
        const uint8_t UDP_PROT_NUMBER = 17; //!< UDP Protocol number
        if (ipv4Header.GetProtocol() == UDP_PROT_NUMBER)
        {
            UdpHeader udpHeader;
            copy->RemoveHeader(udpHeader);
            if (udpHeader.GetDestinationPort() == RoCEv2L4Protocol::PROT_NUMBER)
            {
                RoCEv2Header rocev2Header;
                copy->RemoveHeader(rocev2Header);

                FlowIdentifier flowId(ipv4Item->GetHeader().GetSource(),
                                      ipv4Item->GetHeader().GetDestination(),
                                      rocev2Header.GetSrcQP(),
                                      rocev2Header.GetDestQP());

                vEcn.push_back(std::make_tuple(Simulator::Now(),
                                               flowId,
                                               rocev2Header.GetPSN(),
                                               ipv4Item->GetSize()));

                isRocev2 = true;
            }
        }

        if (!isRocev2)
        {
            vEcn.push_back(
                std::make_tuple(Simulator::Now(), FlowIdentifier(), 0, ipv4Item->GetSize()));
        }
    }
}

void
FifoQueueDiscEcn::Stats::CollectAndCheck()
{
}

} // namespace ns3
