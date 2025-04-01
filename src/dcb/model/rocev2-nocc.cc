/*
 * Copyright (c) 2023
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

#include "rocev2-nocc.h"

#include "rocev2-socket.h"

#include "ns3/global-value.h"
#include "ns3/simulator.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("RoCEv2Nocc");

NS_OBJECT_ENSURE_REGISTERED(RoCEv2Nocc);

void
RoCEv2Nocc::SetReady()
{
    if (!m_fairShare)
    {
        SetRateRatio(m_startRateRatio);
    }
    else
    {
        // Set the rate to fair share
        // Get the number of hosts
        UintegerValue nHostsValue;
        GlobalValue::GetValueByName("NHosts", nHostsValue);
        uint32_t nHosts = nHostsValue.Get();

        // The fair share is 1 / (nHost - 1) for each host
        SetRateRatio(1.0 / (nHosts - 1));

        if (m_targetInflight != 1.0)
        {
            // Set the target inflight
            m_sockState->SetCwnd(m_targetInflight * m_sockState->GetBaseBdp() / (nHosts - 1));
        }
    }
}

} // namespace ns3
