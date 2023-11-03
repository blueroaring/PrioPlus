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

#ifndef DCB_DEVICE_HELPER_H
#define DCB_DEVICE_HELPER_H

#include "ns3/trace-helper.h"

namespace ns3 {

class DcbNetDeviceHelper : public PcapHelperForDevice, public AsciiTraceHelperForDevice
{
public:
  DcbNetDeviceHelper ();

  virtual void EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous,
                                   bool explicitFilename) override;

  virtual void EnableAsciiInternal (Ptr<OutputStreamWrapper> stream, std::string prefix,
                                    Ptr<NetDevice> nd, bool explicitFilename) override;

}; // class DcbNetDeviceHelper

} // namespace ns3

#endif // DCB_DEVICE_HELPER_H
