// https://github.com/sammycage/lunasvg/commit/ec5373150f01b059d7c65956b1c52a95bc8e00c3

#include "textelement.h"
#include "layoutcontext.h"
#include "parser.h"

#include <iostream>

namespace lunasvg {

TextElement::TextElement()
    : StyledElement(ElementID::Text)
{
    //std::cout << "TextElement::TextElement()" << std::endl;
}

Length TextElement::x() const
{
    auto& value = get(PropertyID::X);
    return Parser::parseLength(value, AllowNegativeLengths, Length::Zero);
}

Length TextElement::y() const
{
    auto& value = get(PropertyID::Y);
    return Parser::parseLength(value, AllowNegativeLengths, Length::Zero);
}


void TextElement::layout(LayoutContext* context, LayoutContainer* current) const
{
    if(isDisplayNone())
        return;

    double valueX = x().value(1000000.0); //todo: 1000000.0 is a magic number
    double valueY = y().value(1000000.0); //todo: 1000000.0 is a magic number

    std::cout << "X : " << valueX << std::endl;
    std::cout << "Y : " << valueY << std::endl;

    auto text = get(PropertyID::Text);

    auto layout = makeUnique<LayoutText>();

    layout->x = valueX;
    layout->y = valueY;
    layout->text = text;

    current->addChild(std::move(layout));

    //layoutChildren(context, text.get());
    //current->addChild(std::move(text));

}

std::unique_ptr<Node> TextElement::clone() const
{
    return cloneElement<TextElement>();
}

} // namespace lunasvg
