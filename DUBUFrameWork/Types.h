#pragma once
#include <memory>
#include <atomic>
#include <queue>
#include <set>
#include <functional>
#include <string>
#include <flatbuffers/flatbuffer_builder.h>

#pragma region BaseTypes
using Bool = bool;
using Uint8 = uint8_t;
using Int8 = int8_t;
using Int16 = int16_t;
using Int32 = int32_t;
using Int64 = int64_t;
using Uint8 = uint8_t;
using Uint16 = uint16_t;
using Uint32 = uint32_t;
using Uint64 = uint64_t;
using String = std::string;
using WString = std::wstring;
#pragma endregion

#pragma region TemplateTypes
template <typename T>
using Atomic = std::atomic<T>;

template <typename T>
using Vector = std::vector<T>;

template <typename T>
using Queue = std::queue<T>;

template <typename T>
using PriorityQueueDes = std::priority_queue<T, Vector<T>, std::greater<T>>;

template <typename T>
using PriorityQueue = std::priority_queue<T>;

template <typename T>
using Set = std::set<T>;

template <typename T, typename K>
using Tuple = std::pair<T, K>;

template <typename T, typename K>
struct TupleHash {
    std::size_t operator()(const Tuple<T, K>& p) const {
        return std::hash<T>()(p.first) ^ std::hash<K>()(p.second); // 비트 연산을 사용하여 두 해시를 결합
    }
};

template <typename T, typename K>
using Map = std::unordered_map<T, K>;

template <typename T1, typename T2, typename K>
using MapTuple = std::unordered_map<Tuple<T1, T2>, K, TupleHash<T1, T2>>; // pair 커스텀 해시 적용

template <typename T>
using Function = std::function<T>;
#pragma endregion

#pragma region CustomTypes
using FbbPtr = std::shared_ptr<flatbuffers::FlatBufferBuilder>;
#pragma endregion

