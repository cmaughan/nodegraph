#pragma once

#include <cassert>
#include <exception>
#include <functional>
#include <set>

#include <nod/nod.hpp>

#include <gsl/gsl.hpp>

#include "mutils/profile/profile.h"

#include "threadpool/threadpool.h"

#include "nodegraph/model/node.h"
#include "nodegraph/model/pin.h"

namespace NodeGraph
{

// A collection of nodes that can be computed
class Graph
{
public:
    Graph();
    virtual ~Graph();
    virtual void Clear();

    // Use this method to create nodes and add them to the m_graph
    template <typename T, typename... Args>
    T* CreateNode(Args&&... args)
    {
        PreModify();

        auto pNode = std::make_shared<T>(*this, std::forward<Args>(args)...);
        nodes.insert(pNode);
        m_displayNodes.push_back(pNode.get());

        PostModify();
        return pNode.get();
    }

    template <class T>
    std::set<T*> Find(ctti::type_id_t type) const
    {
        std::set<T*> found;
        for (auto& pNode : nodes)
        {
            if (pNode->GetType() == type)
                found.insert(static_cast<T*>(pNode.get()));
        }
        return found;
    }

    template <class T>
    std::set<T*> Find(const std::vector<ctti::type_id_t>& nodeTypes) const
    {
        std::set<T*> found;
        for (auto& t : nodeTypes)
        {
            auto f = Find<T>(t);
            found.insert(f.begin(), f.end());
        }
        return found;
    }

    void Visit(Node& node, PinDir dir, ParameterType type, std::function<bool(Node&)> fn);

    // Get the list of pins that could be on the UI
    virtual std::vector<Pin*> GetControlSurface() const;

    virtual void Compute(const std::vector<Node*>& nodes, int64_t numTicks);

    const std::set<std::shared_ptr<Node>>& GetNodes() const
    {
        return nodes;
    }

    const std::vector<Node*>& GetDisplayNodes() const
    {
        return m_displayNodes;
    }
    void SetDisplayNodes(const std::vector<Node*>& nodes)
    {
        m_displayNodes = nodes;
    }

    const std::vector<Node*>& GetOutputNodes() const
    {
        return m_outputNodes;
    }
    void SetOutputNodes(const std::vector<Node*>& nodes)
    {
        m_outputNodes = nodes;
    }

    void PreModify()
    {
        if (m_modifyTracker == 0)
        {
            Signal_BeginModify(this);
        }

        m_modifyTracker++;
    }

    void PostModify()
    {
        assert(m_modifyTracker > 0);
        m_modifyTracker--;
        if (m_modifyTracker == 0)
        {
            Signal_EndModify(this);
        }
    }

    void SetName(const std::string& name);
    std::string Name() const;

    // Called to notify that this graph is about to be destroyed
    void NotifyDestroy(Graph* pGraph);

    // Signals
    nod::signal<void(Graph*)> Signal_BeginModify;
    nod::signal<void(Graph*)> Signal_EndModify;
    nod::signal<void(Graph*)> Signal_Destroy;

protected:
    uint32_t m_modifyTracker = 0;
    std::set<std::shared_ptr<Node>> nodes;
    std::vector<Node*> m_displayNodes;
    uint64_t currentGeneration = 1;
    std::vector<Node*> m_outputNodes;
    std::string m_strName;
}; // Graph

class GraphModify final
{
public:
    GraphModify(Graph& graph)
        : m_graph(graph)
    {
        m_graph.PreModify();
    }

    ~GraphModify()
    {
        m_graph.PostModify();
    }
    Graph& m_graph;
};

#define GRAPH_MODIFY(a) GraphModify __graphModify(a);

} // namespace NodeGraph
