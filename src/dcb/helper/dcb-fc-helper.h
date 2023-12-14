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

#ifndef DCB_FC_HELPER_H
#define DCB_FC_HELPER_H

#include "ns3/dcb-hpcc-port.h"
#include "ns3/dcb-pfc-port.h"

namespace ns3
{

class DcbFcHelper
{
  public:
    DcbFcHelper();

    virtual ~DcbFcHelper();

    static void InstallPFCtoNodePort(Ptr<Node> node,
                                     const uint32_t port,
                                     const DcbPfcPortConfig& config);

    // ... other flow control configurtions should be added here

    static void InstallHpccPFCtoNodePort(Ptr<Node> node,
                                         const uint32_t port,
                                         const DcbPfcPortConfig& config);
}; // class DcbFcHelper

} // namespace ns3

#endif
