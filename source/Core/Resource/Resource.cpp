#include "Resource.h"

#include "Manager.h"

Resource::ImplBase* Resource::BaseRef::Register(const ID& InID, Utility::TypeHash InTypeHash)
{
    Manager& man = Manager::Get();
    const uint32 hash = Utility::HashCombine(InID.Hash(), InTypeHash);
    auto* res = man.GetResource(hash);
    if (!res)
    {
        res = this->New(InID);
        CHECK_ASSERT(!res, "Failed to resolve virtual override");
        man.Register(res, hash); 
    }
    return res;
}
