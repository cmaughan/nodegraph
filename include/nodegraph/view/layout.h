#pragma once

#include <cstring>
#include <vector>

#include <mutils/logger/logger.h>
#include <yoga/Yoga.h>

#include <nodegraph/view/layout_control.h>

namespace MUtils
{

static int LogLayout(YGConfigRef config, YGNodeRef node, YGLogLevel level, const char* format, va_list args)
{
    static char writeBuffer[4096];
    vsnprintf(writeBuffer + strlen(writeBuffer), sizeof(writeBuffer) - strlen(writeBuffer), format, args);
    LOG(DBG, "Layout:\n"
            << writeBuffer << "\n");
    return 0;
}

// A control which is another type of layout
class Layout : public LayoutControl
{
public:
    Layout(const NVec4f& margin)
    {
        SetMargin(margin);

        yogaParent = YGNodeNewWithConfig(Config());

        // Cross axis stretch (default) (i.e. contained layout will stretch to fill our height)
        YGNodeStyleSetAlignItems(yogaParent, YGAlign::YGAlignStretch);

        YGNodeStyleSetAlignContent(yogaParent, YGAlign::YGAlignStretch);

        // Justify main axis to the flex start
        YGNodeStyleSetJustifyContent(yogaParent, YGJustifyFlexStart);
            
        YGNodeStyleSetMargin(yogaParent, YGEdge::YGEdgeLeft, m_margin.x);
        YGNodeStyleSetMargin(yogaParent, YGEdge::YGEdgeRight, m_margin.z);
        YGNodeStyleSetMargin(yogaParent, YGEdge::YGEdgeTop, m_margin.y);
        YGNodeStyleSetMargin(yogaParent, YGEdge::YGEdgeBottom, m_margin.w);
    }

    virtual void SetSpaceEvenly()
    {
        YGNodeStyleSetJustifyContent(yogaParent, YGJustifySpaceEvenly);
    }

    virtual void SetMinSize(const NVec2f& minSize)
    {
        YGNodeStyleSetMinWidth(yogaParent, minSize.x);
        YGNodeStyleSetMinHeight(yogaParent, minSize.y);
    }

    virtual void SetPreferredSize(const NVec2f& preferred) override
    {
        m_preferredSize = preferred;

        // Override layout size preference
        if (preferred.x != 0.0f)
        {
            YGNodeStyleSetWidth(yogaParent, preferred.x);
        }
        else
        {
            YGNodeStyleSetWidthAuto(yogaParent);
        }
        if (preferred.y != 0.0f)
        {
            YGNodeStyleSetHeight(yogaParent, preferred.y);
        }
        else
        {
            YGNodeStyleSetHeightAuto(yogaParent);
        }

        if (preferred.x == 0.0f || preferred.y == 0.0f)
        {
            YGNodeStyleSetFlexGrow(yogaParent, 1.0f);
        }
    }

    static const YGConfigRef Config()
    {
        static YGConfigRef config = nullptr;
        if (config == nullptr)
        {
            config = YGConfigNew();
            YGConfigSetPrintTreeFlag(config, true);
            YGConfigSetLogger(config, LogLayout);
        }

        return config;
    }

    virtual void SetMargin(const NVec4f& padding)
    {
        m_margin = padding;
    }

    virtual void AddItem(LayoutControl* pControl, const NVec2f& preferredSize = NVec2f(0.0f))
    {
        items.push_back(pControl);

        auto pLayout = dynamic_cast<Layout*>(pControl);
        if (pLayout)
        {
            YGNodeInsertChild(yogaParent, pLayout->yogaParent, YGNodeGetChildCount(yogaParent));
            yogaNodes.push_back(pLayout->yogaParent);

            // This lets the layout grow if 0,0 or fixes size; it needs to be set
            pLayout->SetPreferredSize(preferredSize);
        }
        else
        {
            pControl->SetPreferredSize(preferredSize);

            auto ygNode = YGNodeNewWithConfig(Config());

            YGNodeStyleSetFlexWrap(ygNode, YGWrapNoWrap);
            
            YGNodeStyleSetMargin(ygNode, YGEdge::YGEdgeLeft, m_margin.x);
            YGNodeStyleSetMargin(ygNode, YGEdge::YGEdgeRight, m_margin.z);
            YGNodeStyleSetMargin(ygNode, YGEdge::YGEdgeTop, m_margin.y);
            YGNodeStyleSetMargin(ygNode, YGEdge::YGEdgeBottom, m_margin.w);

            // Override layout size preference
            if (preferredSize.x != 0.0f)
            {
                YGNodeStyleSetWidth(ygNode, preferredSize.x);
            }
            else
            {
                YGNodeStyleSetWidthAuto(ygNode);
            }

            if (preferredSize.y != 0.0f)
            {
                YGNodeStyleSetHeight(ygNode, preferredSize.y);
            }
            else
            {
                YGNodeStyleSetHeightAuto(ygNode);
            }

            if (preferredSize.x == 0.0f || preferredSize.y == 0.0f)
            {
                // Setting flexgrow will make this child node share the parent node's area with whatever space is left.
                YGNodeStyleSetFlexGrow(ygNode, 1.0f);
            }

            yogaNodes.push_back(ygNode);

            YGNodeInsertChild(yogaParent, ygNode, YGNodeGetChildCount(yogaParent));

        }
    }

    virtual void UpdateLayout()
    {
        YGNodeCalculateLayout(yogaParent, YGUndefined, YGUndefined, YGDirection::YGDirectionLTR);

        std::function<void(Layout*, const NVec2f&)> fnVisit = [&](Layout* pLayout, const NVec2f& relative = NVec2f(0.0f)) {
            for (uint32_t i = 0; i < pLayout->items.size(); i++)
            {
                auto pLayoutChild = dynamic_cast<Layout*>(pLayout->items[i]);

                auto topLeft = NVec2f(YGNodeLayoutGetLeft(pLayout->yogaNodes[i]), YGNodeLayoutGetTop(pLayout->yogaNodes[i])) + relative;
                pLayout->items[i]->SetViewRect(NRectf(topLeft.x, topLeft.y, YGNodeLayoutGetWidth(pLayout->yogaNodes[i]), YGNodeLayoutGetHeight(pLayout->yogaNodes[i])));
                if (pLayoutChild)
                {
                    fnVisit(pLayoutChild, topLeft);
                }
            }
        };

        SetViewRect(NRectf(YGNodeLayoutGetLeft(yogaParent), YGNodeLayoutGetTop(yogaParent), YGNodeLayoutGetWidth(yogaParent), YGNodeLayoutGetHeight(yogaParent)));

        fnVisit(this, GetViewRect().topLeftPx);
    }

    virtual void VisitLayouts(std::function<void(Layout*)> fnCB)
    {
        std::function<void(Layout*)> fnVisit = [&](Layout* pLayout) {
            fnCB(pLayout);
            for (uint32_t i = 0; i < pLayout->items.size(); i++)
            {
                auto pLayoutChild = dynamic_cast<Layout*>(pLayout->items[i]);
                if (pLayoutChild)
                {
                    fnVisit(pLayoutChild);
                }
            }
        };
        fnVisit(this);
    }

    const std::vector<LayoutControl*> GetItems() const
    {
        return items;
    }

protected:
    std::vector<LayoutControl*> items;
    std::vector<YGNodeRef> yogaNodes;
    YGNodeRef yogaParent;
};

class VLayout : public Layout
{
public:
    VLayout(const NVec4f& margin = NVec4f(4.0f))
        : Layout(margin)
    {
        YGNodeStyleSetFlexDirection(yogaParent, YGFlexDirection::YGFlexDirectionColumn);
    }
};

class HLayout : public Layout
{
public:
    HLayout(const NVec4f& margin = NVec4f(4.0f))
        : Layout(margin)
    {
        YGNodeStyleSetFlexDirection(yogaParent, YGFlexDirection::YGFlexDirectionRow);
    }
};

} // namespace MUtils
