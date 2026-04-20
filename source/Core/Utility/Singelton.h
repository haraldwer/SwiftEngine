#pragma once

#include "Utility/Utility.h"

namespace Utility
{
    struct SingeltonContext
    {
        inline static int value = -1; 
    };

    template <class T, bool Global = false>
    class Singelton
    {
    public:
        virtual ~Singelton()
        {
            Singelton*& ptr = Global ?
                instance : contextInstance[SingeltonContext::value];
            ptr = nullptr;
        }

        Singelton()
        {
            Singelton*& ptr = Global ?
                instance : contextInstance[SingeltonContext::value];
            CHECK_ASSERT(ptr, "Instance already set");
            ptr = this;
        }

        static T& Get()
        {
            Singelton*& ptr = Global ?
                instance : contextInstance[SingeltonContext::value];
            CHECK_ASSERT(!ptr, "Instance invalid");
            return *reinterpret_cast<T*>(ptr);
        }
        
        static T* TryGet()
        {
            Singelton*& ptr = Global ?
                instance : contextInstance[SingeltonContext::value];
            return ptr ? reinterpret_cast<T*>(ptr) : nullptr;
        }
        
        inline static Singelton* instance = nullptr;
        inline static Map<int, Singelton*> contextInstance;
    };
}
