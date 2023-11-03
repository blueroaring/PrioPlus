/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
#include "ns3/abort.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/simulator.h"
#include <cstdint>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FifoQueueDiscEcn");

NS_OBJECT_ENSURE_REGISTERED (FifoQueueDiscEcn);

TypeId
FifoQueueDiscEcn::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::FifoQueueDiscEcn")
    .SetParent<FifoQueueDisc> ()
    .SetGroupName ("dcb")
    .AddConstructor<FifoQueueDiscEcn> ();
  return tid;
}

FifoQueueDiscEcn::FifoQueueDiscEcn ()
  : m_ecnKMin (UINT32_MAX - 1), m_ecnKMax (UINT32_MAX), m_ecnPMax (0.)
{
  NS_LOG_FUNCTION (this);
}

FifoQueueDiscEcn::~FifoQueueDiscEcn ()
{
  NS_LOG_FUNCTION (this);
}

bool
FifoQueueDiscEcn::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);

  Ptr<Ipv4QueueDiscItem> ipv4Item = DynamicCast<Ipv4QueueDiscItem> (item);
  if (ipv4Item && CheckShouldMarkECN (ipv4Item))
    {
      NS_LOG_DEBUG ("Switch " << Simulator::GetContext () << " FifoQueueDiscEcn marks ECN on packet");
      ipv4Item->Mark ();
    }

  if (GetCurrentSize () + item > GetMaxSize ())
    {
      NS_LOG_LOGIC ("Queue full -- dropping pkt");
      DropBeforeEnqueue (item, LIMIT_EXCEEDED_DROP);
      return false;
    }

  bool retval = GetInternalQueue (0)->Enqueue (item);

  // If Queue::Enqueue fails, QueueDisc::DropBeforeEnqueue is called by the
  // internal queue because QueueDisc::AddInternalQueue sets the trace callback

  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  return retval;
}

void
FifoQueueDiscEcn::ConfigECN (uint32_t kmin, uint32_t kmax, double pmax)
{
  NS_LOG_FUNCTION (this);

  if (kmin >= kmax)
    {
      NS_FATAL_ERROR ("ECN kMin should be smaller than kMax");
    }
  if (pmax > 1.0 || pmax < 0.)
    {
      NS_FATAL_ERROR ("ECN pMAx should be between 0 and 1");
    }
  m_ecnKMin = kmin;
  m_ecnKMax = kmax;
  m_ecnPMax = pmax;

  m_rng = CreateObject<UniformRandomVariable> ();
  m_rng->SetAttribute ("Min", DoubleValue (0.0));
  m_rng->SetAttribute ("Max", DoubleValue (1.0));
}

bool
FifoQueueDiscEcn::CheckShouldMarkECN (Ptr<Ipv4QueueDiscItem> item) const
{
  NS_LOG_FUNCTION (this << item);
  uint32_t nbytes = GetNBytes () + item->GetPacket ()->GetSize ();
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
      return m_rng->GetValue () * 1024 < prob;
    }
}

} // namespace ns3
