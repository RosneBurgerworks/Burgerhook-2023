#include <menu/object/container/TRow.hpp>
#include <menu/object/container/TData.hpp>

void zerokernel::TData::setParent(zerokernel::BaseMenuObject *parent)
{
    Container::setParent(parent);

    row = dynamic_cast<TRow *>(parent);
}

zerokernel::TData::TData()
{
    bb.height.setContent();
}
