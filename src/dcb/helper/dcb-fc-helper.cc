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

#include "dcb-fc-helper.h"

#include "ns3/assert.h"

namespace ns3
{

DcbFcHelper::DcbFcHelper()
{
}

DcbFcHelper::~DcbFcHelper()
{
}

// static
void
DcbFcHelper::InstallPFCtoNodePort(Ptr<Node> node,
                                  const uint32_t port,
                                  const DcbPfcPortConfig& config)
{
    Ptr<DcbTrafficControl> dcbTc = node->GetObject<DcbTrafficControl>();
    NS_ASSERT_MSG(dcbTc, "PFC enabled but there is no DcbTrafficControl aggregated to the node");

    Ptr<NetDevice> dev = node->GetDevice(port);

    // enable flow control on queue disc
    Ptr<PausableQueueDisc> qDisc = DynamicCast<DcbNetDevice>(node->GetDevice(port))->GetQueueDisc();
    qDisc->SetFCEnabled(true);

    // install PFC
    Ptr<DcbPfcPort> pfc = CreateObject<DcbPfcPort>(dev, dcbTc);
    uint8_t enableVec = 0;
    for (const DcbPfcPortConfig::QueueConfig& qConfig : config.queues)
    {
        if (qConfig.priority >= DcbTrafficControl::PRIORITY_NUMBER)
        {
            NS_FATAL_ERROR("PFC priority should be 0~7, your input is " << qConfig.priority);
        }
        if (qConfig.xon > qConfig.reserve)
        {
            NS_FATAL_ERROR("XON should be less or equal to reserve");
        }
        enableVec |= (1 << qConfig.priority);
        pfc->ConfigQueue(qConfig.priority, qConfig.reserve, qConfig.xon);
    }
    pfc->SetEnableVec(enableVec);
    dcbTc->InstallFCToPort(port, pfc);

    // register protocol handler
    node->RegisterProtocolHandler(MakeCallback(&DcbPfcPort::ReceivePfc, pfc),
                                  PfcFrame::PROT_NUMBER,
                                  dev);
}

} // namespace ns3
