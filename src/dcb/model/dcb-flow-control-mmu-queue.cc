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

#include "dcb-flow-control-mmu-queue.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DcbFlowControlMmuQueue");

NS_OBJECT_ENSURE_REGISTERED(DcbFlowControlMmuQueue);

TypeId
DcbFlowControlMmuQueue::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DcbFlowControlMmuQueue").SetParent<Object>().SetGroupName("Dcb");
    return tid;
}

TypeId
DcbFlowControlMmuQueue::GetInstanceTypeId() const
{
    return GetTypeId();
}
} // namespace ns3