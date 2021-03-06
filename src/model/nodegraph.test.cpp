#include <catch2/catch.hpp>

#include "nodegraph/model/graph.h"
#include "nodegraph/view/layout.h"
#include "nodegraph/view/graphview.h"

using namespace NodeGraph;


class TestNode : public Node
{
public:
    DECLARE_NODE(TestNode, test);

    explicit TestNode(Graph& m_graph)
        : Node(m_graph, "Adder")
    {
        m_flags |= NodeFlags::OwnerDraw;

        pSum = AddOutput("Sum", .0f, ParameterAttributes(ParameterUI::Knob, 0.0f, 1.0f));
        pValue1 = AddInput("Value1", 0.0f, ParameterAttributes(ParameterUI::Knob, 0.0f, 1.0f));
        pValue2 = AddInput("Value2", 0.0f, ParameterAttributes(ParameterUI::Knob, 0.0f, 1.0f));

        auto pLayout = new MUtils::VLayout();
        pLayout->AddItem(pSum);
        pLayout->AddItem(pValue1);
        pLayout->AddItem(pValue2);
        pLayout->UpdateLayout();
    }

    virtual void Compute() override
    {
        pSum->Set(pValue1->To<float>() + pValue2->To<float>());
    }

    virtual void Draw(GraphView& view, Canvas& canvas, ViewNode& viewNode)
    {
        view.DrawPin(viewNode, *pSum);
    }

    Pin* pSum = nullptr;
    Pin* pValue1 = nullptr;
    Pin* pValue2 = nullptr;
};

TEST_CASE("Parameters Get and Set", "[Parameters]")
{
    SECTION("Default Parameter Can Cast")
    {
        Parameter p;
        REQUIRE(p.GetType() == ParameterType::None);
        p.Set(int64_t(5));
        REQUIRE(p.GetType() == ParameterType::Int64);
    }

    Parameter p(0.0f);
    p.SetLerpSamples(10);
    p.Set(0.5f);

    SECTION("Parameter Begin Lerp")
    {
        // At start of day the parameter has not lerped
        REQUIRE(p.To<float>() == 0.0f);
    }

    SECTION("Parameter half lerp")
    {
        // .. half way there
        p.Update(5);
        REQUIRE(p.To<float>() == 0.25f);
    }

    SECTION("Parameter full lerp")
    {
        // .. clamped at max (target) value
        p.Update(12);
        REQUIRE(p.To<float>() == .5f);
    }

    SECTION("Parameter change")
    {
        auto old = p.GetGeneration();
        p.SetFrom<int>(5);
        REQUIRE(old != p.GetGeneration());
    }
}
TEST_CASE("Creation", "[Nodes]")
{
    Graph g;
    GraphView::Init();

    auto pNode = g.CreateNode<TestNode>();

    pNode->pValue1->SetAttributes(ParameterAttributes(ParameterUI::Knob, 0.0f, 1.0f));
    pNode->pValue2->SetAttributes(ParameterAttributes(ParameterUI::Knob, -1.0f, 1.0f));
    pNode->pValue1->Set(.5f, true);
    pNode->pValue2->Set(.1f, true);

    // Should not have computed yet
    auto val = pNode->pSum->GetValue<float>();
    REQUIRE_FALSE(val == .6f);

    // Compute the m_graph at tick 0
    g.Compute(std::set<Node*>{ pNode }, 0);

    // Get the result
    val = pNode->pSum->GetValue<float>();
    REQUIRE(val == .6f);
}

SCENARIO("NodeGraph clears correctly", "[NodeGraph]")
{
    GIVEN("A node graph")
    {
        Graph g;

        WHEN("A node is created")
        {
            auto pNode = g.CreateNode<TestNode>();
            auto type = pNode->GetType();

            THEN("It is in the graph")
            {
                auto found = g.Find<TestNode>(type);
                REQUIRE(!found.empty());
                REQUIRE(*found.begin() == pNode);
            }
            
            WHEN("The graph is now cleared")
            {
                g.Clear();

                THEN("It is empty again")
                {
                    auto found = g.Find<TestNode>(type);
                    REQUIRE(found.empty());
                }
            }
        }

    }
}
