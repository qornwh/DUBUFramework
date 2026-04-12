#pragma once
#include "Singleton.h"
#include "RWLock.h"

namespace DUBU
{
    template<typename T>
    class ObjectPool : public Singleton<ObjectPool<T>>
    {
    public:
        void Initialize(Int32 count);

        template<typename... Args>
        T* Pop(Args&&... args);
        void Push(T* ptr);

    private:
        Vector<T*> freeList_;
        Lock lk_;
    };

    template<typename T>
    inline void ObjectPool<T>::Initialize(Int32 count)
    {
        // 초기 할당 개수. 단 파라미터는 없어야된다.
        freeList_.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            freeList_[i] = new T();
        }
    }

    template<typename T>
    template<typename... Args>
    inline T* ObjectPool<T>::Pop(Args&& ...args)
    {
        if (!freeList_.empty())
        {
            WriteLockGuard wl(lk_);
            T* ptr = freeList_.back();
            freeList_.pop_back();
            return new(ptr) T(std::forward<Args>(args)...);
        }
        else
        {
            T* ptr = new T(std::forward<Args>(args)...);
            return ptr;
        }
    }

    template<typename T>
    inline void ObjectPool<T>::Push(T* ptr)
    {
        WriteLockGuard wl(lk_);
        freeList_.push_back(ptr);
    }
}
