#include "pch.h"
#include <Pool.h>
#include <BufferManager.h>

// PacketManager 버퍼 풀 테스트
// - A5-1 회귀 : 송신 경로가 size_를 줄여놔도(opb->size_ = totalSize_)
//   Pop 시점에 size_/pos_가 풀사이즈로 리셋되어야 한다

TEST(BufferPoolTest, PopResetsSizeAndPos)
{
    DUBU::Initialize();
    auto& manager = DUBU::PacketManager::GetInstance();

    DUBU::OverlappedPacketBuffer* opb = manager.PopPacketBuffer();
    ASSERT_NE(opb, nullptr);
    EXPECT_EQ(opb->size_, sizeof(opb->buffer_));
    EXPECT_EQ(opb->pos_, opb->buffer_);

    // 송신 경로처럼 오염시킨 뒤 반환
    opb->size_ = 24;
    opb->pos_ = opb->buffer_ + 100;
    manager.PushPacketBuffer(opb);

    // LIFO라 방금 오염시킨 그 버퍼가 나온다 -> 리셋 확인
    DUBU::OverlappedPacketBuffer* reused = manager.PopPacketBuffer();
    EXPECT_EQ(reused, opb);
    EXPECT_EQ(reused->size_, sizeof(reused->buffer_));
    EXPECT_EQ(reused->pos_, reused->buffer_);

    manager.PushPacketBuffer(reused);
}

// Pop한 버퍼는 useList에 잡히고, Push하면 빠진다
TEST(BufferPoolTest, UseListTracksOwnership)
{
    DUBU::Initialize();
    auto& manager = DUBU::PacketManager::GetInstance();

    DUBU::OverlappedPacketBuffer* opb = manager.PopPacketBuffer();
    EXPECT_TRUE(manager.GetUseList().contains(opb));

    manager.PushPacketBuffer(opb);
    EXPECT_FALSE(manager.GetUseList().contains(opb));
}
