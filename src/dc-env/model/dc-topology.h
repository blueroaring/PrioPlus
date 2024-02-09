/*
 * Copyright (c) 2010 Universita' di Firenze, Italy
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
 * Author: Pavinberg (pavin0702@gmail.com)
 */

#ifndef DC_TOPOLOGY_H
#define DC_TOPOLOGY_H

#include "ns3/ipv4-interface-address.h"
#include "ns3/net-device.h"
#include "ns3/object.h"
#include "ns3/random-variable-stream.h"
#include "ns3/timer.h"

#include <cstddef>
#include <iterator>
#include <map>
#include <vector>

/**
 * \file
 * \ingroup protobuf-topology
 * ns3::DcTopology declaration
 */
namespace ns3
{

/**
 * \breif A datacenter topology representation.
 *
 * NOTICE: User should install hosts first then switches because it can keep the host indicies
 * continuous which is easier for random generator to choose a random host.
 */
class DcTopology : public Object
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId.
     */
    static TypeId GetTypeId(void);

    DcTopology() = default;
    DcTopology(uint32_t nodeNum);
    virtual ~DcTopology();

    struct TopoNode
    {
        enum NodeType
        {
            HOST,
            SWITCH
        };

        NodeType type;
        Ptr<Node> nodePtr;

        const Ptr<Node>& operator->() const
        {
            return nodePtr;
        }
    };

    void InstallNode(const uint32_t index, const TopoNode node);

    void InstallLink(const uint32_t node1, const uint32_t node2);

    const TopoNode& GetNode(const uint32_t index) const;

    const uint32_t GetNodeIndex(const Ptr<Node> node) const;

    const Ptr<NetDevice> GetNetDeviceOfNode(const uint32_t nodei, const uint32_t devi) const;

    const Ipv4InterfaceAddress GetInterfaceOfNode(const uint32_t nodei, uint32_t intfi) const;

    bool IsHost(const uint32_t index) const;
    bool IsSwitch(const uint32_t index) const;

    uint32_t GetNHosts() const;
    uint32_t GetNNodes() const;

    /**
     * \brief Add a key-value to the delay map.
     */
    void AddDelay(const Ipv4Address src, const Ipv4Address dst, const uint32_t hops, Time delay);

    /**
     * \brief Get the propagation delay between two nodes.
     */
    const Time GetDelay(const Ipv4Address src, const Ipv4Address dst) const;

    /**
     * \brief Get the number of hops between two nodes.
     */
    const uint32_t GetHops(const Ipv4Address src, const Ipv4Address dst) const;

    /**
     * \brief Log the delay map.
     */
    void LogDelayMap() const;

    /**
     * \brief Create a configured host index random number generator.
     * Used by applications to find a random destination.
     * We give each application an configured random number generator (RNG) instead
     * of a random address directly because of the need of concurrency. RNG is not
     * thread-safe. So we let the application do the random generation itself.
     */
    const Ptr<UniformRandomVariable> CreateRamdomHostChooser() const;

    void Print(std::ostream& os) const;

  private:
    std::vector<TopoNode> m_nodes;
    std::vector<std::vector<uint32_t>> m_links;
    uint32_t m_nHosts;

    // std::map<std::pair<uint32_t, uint32_t>, Time> m_delayMap; // <src, dst> -> propagation delay
    std::map<std::pair<Ipv4Address, Ipv4Address>, std::pair<uint32_t, Time>>
        m_delayMap; // <src, dst> -> <hops, propagation delay>

  public:
    /**
     * Iterators:
     * 1. use begin(), end() to iterate over all nodes
     * 2. use hosts_begin(), hosts_end() to iterate over hosts
     * 3. use switches_begin(), switches_end() to iterate over switches
     *
     * The definitions are at the end.
     */

    /* 1. Iterator */
    class Iterator
    {
      public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = TopoNode;
        using pointer = TopoNode*;   // or also value_type*
        using reference = TopoNode&; // or also value_type&

        Iterator(pointer ptr)
            : m_ptr(ptr)
        {
        }

        reference operator*() const
        {
            return *m_ptr;
        }

        pointer operator->()
        {
            return m_ptr;
        }

        // Prefix increment
        Iterator& operator++()
        {
            m_ptr++;
            return *this;
        }

        // Postfix increment
        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_ptr == rhs.m_ptr;
        }

        friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_ptr != rhs.m_ptr;
        }

      protected:
        pointer m_ptr;
    };

    Iterator begin()
    {
        return Iterator(&m_nodes[0]);
    }

    Iterator end()
    {
        return Iterator(&(*m_nodes.end()));
    }

    class ConstIterator
    {
      public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = TopoNode;
        using pointer = const TopoNode*;   // or also value_type*
        using reference = const TopoNode&; // or also value_type&

        ConstIterator(pointer ptr)
            : m_ptr(ptr)
        {
        }

        reference operator*() const
        {
            return *m_ptr;
        }

        pointer operator->() const
        {
            return m_ptr;
        }

        // Prefix increment
        ConstIterator& operator++()
        {
            m_ptr++;
            return *this;
        }

        // Postfix increment
        ConstIterator operator++(int)
        {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const ConstIterator& lhs, const ConstIterator& rhs)
        {
            return lhs.m_ptr == rhs.m_ptr;
        }

        friend bool operator!=(const ConstIterator& lhs, const ConstIterator& rhs)
        {
            return lhs.m_ptr != rhs.m_ptr;
        }

      protected:
        pointer m_ptr;
    };

    ConstIterator begin() const
    {
        return ConstIterator(&m_nodes[0]);
    }

    ConstIterator end() const
    {
        return ConstIterator(&(*m_nodes.end()));
    }

    /* 2. HostIterator */
    class HostIterator final : public Iterator
    {
      public:
        using iterator_category = std::bidirectional_iterator_tag;

        HostIterator(pointer ptr, std::vector<TopoNode>::iterator end)
            : Iterator(ptr),
              m_end(&(*end))
        {
        }

        // Prefix increment
        HostIterator operator++()
        {
            while ((++m_ptr)->type != TopoNode::NodeType::HOST && m_ptr != m_end)
            {
            }
            return *this;
        }

        // Postfix increment
        HostIterator operator++(int)
        {
            HostIterator tmp = *this;
            while ((++m_ptr)->type != TopoNode::NodeType::HOST && m_ptr != m_end)
            {
            }
            return tmp;
        }

      private:
        const pointer m_end;
    }; // struct HostIterator

    HostIterator hosts_begin()
    {
        // return the first HOST
        for (TopoNode& node : m_nodes)
        {
            if (node.type == TopoNode::HOST)
            {
                return HostIterator(&node, m_nodes.end());
            }
        }
        return HostIterator(&(*m_nodes.end()), m_nodes.end());
    }

    HostIterator hosts_end()
    {
        return HostIterator(&(*m_nodes.end()), m_nodes.end());
    }

    /* 3. SwitchIterator */
    class SwitchIterator final : public Iterator
    {
      public:
        using iterator_category = std::bidirectional_iterator_tag;

        SwitchIterator(pointer ptr, const std::vector<TopoNode>::iterator end)
            : Iterator(ptr),
              m_end(&(*end))
        {
        }

        // Prefix increment
        SwitchIterator operator++()
        {
            while ((++m_ptr)->type != TopoNode::NodeType::SWITCH && m_ptr != m_end)
            {
            }
            return *this;
        }

        // Postfix increment
        SwitchIterator operator++(int)
        {
            SwitchIterator tmp = *this;
            while ((++m_ptr)->type != TopoNode::NodeType::SWITCH && m_ptr != m_end)
            {
            }
            return tmp;
        }

      private:
        const pointer m_end;
    };

    SwitchIterator switches_begin()
    {
        // return the first HOST
        for (TopoNode& node : m_nodes)
        {
            if (node.type == TopoNode::SWITCH)
            {
                return SwitchIterator(&node, m_nodes.end());
            }
        }
        return SwitchIterator(&(*m_nodes.end()), m_nodes.end());
    }

    SwitchIterator switches_end()
    {
        return SwitchIterator(&(*m_nodes.end()), m_nodes.end());
    }

}; // class DcTopology

} // namespace ns3

#endif
