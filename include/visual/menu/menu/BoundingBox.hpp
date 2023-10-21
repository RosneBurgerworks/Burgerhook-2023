#pragma once

#include <limits>

namespace zerokernel
{

class BaseMenuObject;

class BoundingBox
{
public:
    class SizeMode
    {
    public:
        enum class Mode
        {
            FIXED,
            CONTENT,
            FILL
        };

        int clamp(int value)
        {
            if (value < min)
                value = min;
            if (value > max)
                value = max;
            return value;
        }

        Mode mode{ Mode::FIXED };

        void setFixed();

        void setContent();

        void setFill();

        int min{ 0 };
        int max{ std::numeric_limits<int>::max() };
    };
    struct box_sides
    {
        int top;
        int bottom;
        int left;
        int right;
    };
    struct box
    {
        int x;
        int y;
        int width;
        int height;

        inline int left() const
        {
            return x;
        }
        inline int right() const
        {
            return x + width;
        }
        inline int top() const
        {
            return y;
        }
        inline int bottom() const
        {
            return y + height;
        }
    };

public:
    explicit BoundingBox(BaseMenuObject &object);

    /*
     * Does the border box area contain the point
     */
    bool contains(int x, int y);

    bool containsForTooltip(int x, int y, int length);

    /*
     * Extends the box, if needed, to fit the other box without moving it
     * Returns true if the size of the box was changed
     */
    bool extend(BoundingBox &box);

    //

    BoundingBox &getParentBox();

    bool isFloating();

    void setFloating(bool flag);

    /*
     * Move the border box
     */
    bool move(int x, int y);

    /*
     * Resize the border box, returns true if size was changed
     */
    bool resize(int width, int height);

    bool resizeContent(int width, int height);

    /*
     * Normalizes size according to min/max
     * Returns true if size was changed
     */
    bool normalizeSize();

    /*
     * Shinks the box to minimum size
     */
    bool shrink();

    /*
     * Shrinks only the CONTENT parts
     */
    bool shrinkContent();

    bool updateFillSize();

    void setPadding(int top, int bottom, int left, int right);

    void setMargin(int top, int bottom, int left, int right);

    //

    void onParentSizeUpdate();

    void onChildSizeUpdate();

    void emitObjectSizeUpdate();

    //

    box getFullBox() const;

    box getContentBox() const;

    const box &getBorderBox() const;

    //

    box border_box{};
    box_sides padding{};
    box_sides margin{};

    SizeMode width{};
    SizeMode height{};

    bool floating{ false };

    BaseMenuObject &object;
};
} // namespace zerokernel
