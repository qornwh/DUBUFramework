#pragma once
#include "pch.h"
#include "Types.h"

namespace DUBU::DS
{

	/*
	* 일단 Lock기반 큐로 구현 : AI 참고
    *  - MPMC(Multiple Producer - Multiple Consumer) Ringbuffer 
	*/
	template<typename T>
    class RingBuffer
	{
        struct Node {
            // seq_ : 멀티스레드 상에서 동시에 여러 push, pop 내 현재 노드 확인  
            Atomic<uint64_t> seq_;
            T data_;
        };

    public:
        RingBuffer()
        {
            // 초기화
            for (Uint64 i = 0; i < DEFAULT_WINDOW_COUNT; ++i)
            {
                buffer_[i].seq_.store(i);
            }
        }

		bool Push(T t);
        bool Pop(T& t);

	public:
        // 일단 기본 DEFAULT_WINDOW_COUNT == 64개로 고정함.
        Node buffer_[DEFAULT_WINDOW_COUNT];
        Int32 startPos_ = 0;
        Int32 endPos_ = 0;
        Atomic<Uint64> head_{ 0 };
        Atomic<Uint64> tail_{ 0 };
	};

	template<typename T>
	inline bool RingBuffer<T>::Push(T t)
	{
        Uint64 tail = tail_.load();
        while (true)
        {
            Node& node = buffer_[tail % DEFAULT_WINDOW_COUNT];
            const Uint64 seq = node.seq_.load();
            if (seq == tail)
            {
                if (tail_.compare_exchange_strong(tail, tail + 1))
                {
                    node.data_ = t; // 일단 그냥 복사다.
                    node.seq_.store(seq + 1); // 채워짐. +1 증가
                    return true;
                }
            }
            else if (seq < tail)
            {
                // ringbuffer 가득 참
                return false;
            }
            else
            {
                // 이미 push
                tail = tail_.load();
            }
        }
	}

	template<typename T>
	inline bool RingBuffer<T>::Pop(T& t)
	{
        Uint64 head = head_.load();
        while (true)
        {
            Node& node = buffer_[head % DEFAULT_WINDOW_COUNT];
            const Uint64 seq = node.seq_.load();
            if (seq == head + 1)
            {
                if (head_.compare_exchange_strong(head, head + 1))
                {
                    t = node.data_;
                    node.seq_.store(head + DEFAULT_WINDOW_COUNT); // 다음 랩의 push가 기다리는 값 == head + 배열 크기 (seq + 64는 +1 오차)
                    return true;
                }
            }
            else if (seq < head + 1)
            {
                // 비었음
                return false;
            }
            else
            {
                // 이미 pop
                head = head_.load();
            }
        }
	}
}

