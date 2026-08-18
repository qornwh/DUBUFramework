#pragma once
#include "pch.h"
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
        // 생성자 호출 없이 메모리만 확보해 둔다 (freeList 규약 : 소멸 완료된 빈 메모리)
        freeList_.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            void* vm = operator new(sizeof(T));
            freeList_.push_back(static_cast<T*>(vm));
        }
    }

    template<typename T>
    template<typename... Args>
    inline T* ObjectPool<T>::Pop(Args&& ...args)
    {
        WriteLockGuard wl(lk_);
        if (!freeList_.empty())
        {
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
        ptr->~T();
        freeList_.push_back(ptr);
    }
}
