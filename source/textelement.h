#ifndef TEXTELEMENT_H
#define TEXTELEMENT_H

#include "styledelement.h"

namespace lunasvg {

class TextElement : public StyledElement {
public:
    TextElement();

    Length x() const;
    Length y() const;

    void layout(LayoutContext* context, LayoutContainer* current) const;

    std::unique_ptr<Node> clone() const;
};


} // namespace lunasvg

#endif // TEXTELEMENT_H