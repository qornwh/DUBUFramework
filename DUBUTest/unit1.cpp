#include "pch.h"
#define private public
#include "Session.h"
#undef private
#include "BufferManager.h"
#include <gtest/gtest.h>
#include <random>

using namespace DUBU;

static constexpr Uint8 TEST_CODE = 10;   // 테스트용 packetCode

// 실행된 패킷의 시퀀스 기록
static std::vector<Uint32> g_parsed;

// 테스트용 핸들러 맵
static Map<Uint8, Packet::PacketHandler> MakeTestHandlers()
{
    Map<Uint8, Packet::PacketHandler> handlers;
    Packet::PacketHandler h;
    h.verifier_ = [](flatbuffers::Verifier&) { return true; };
    h.handler_ = [](Session* session, Uint8* buffer, Int32 size)
        {
            Uint32 seq = 0;
            auto* h = reinterpret_cast<Packet::PacketHeader*>(buffer);
            memcpy(&seq, &(h->sequenceNo_), sizeof(seq));
            g_parsed.push_back(seq);
        };
    handlers[TEST_CODE] = h;
    return handlers;
}

// 플래그 조합 헬퍼
//  - 순서보장 채널 : REPEAT | CHANNEL | (channelID << 3)
//  - 순서없는 재전송: REPEAT
static Uint8 ChannelFlags(Uint8 channelID) 
{
    if (channelID > 0)
    {
        return Packet::REPEAT | Packet::CHANNEL | (channelID << 3);
    }
    else
    {
        return Packet::REPEAT;
    }
}

// 가짜 패킷 생성 (페이로드 4바이트에 seq 복제)
static std::vector<Uint8> MakePacket(Uint32 seq, Uint8 flags)
{
    std::vector<Uint8> buf(sizeof(Packet::PacketHeader) + sizeof(Uint32), 0);
    auto* h = reinterpret_cast<Packet::PacketHeader*>(buf.data());
    h->totalSize_ = static_cast<Uint16>(buf.size());
    h->packetCode_ = TEST_CODE;
    h->flags_ = flags;
    h->sequenceNo_ = seq;
    h->sessionId_ = 10;
    // PacketParse가 checksum을 검사한다면 여기서 채울 것:
    // h->checksum_ = Packet::Packet::CRC32(...);
    memcpy(buf.data() + sizeof(Packet::PacketHeader), &seq, sizeof(seq));
    return buf;
}

class RecvDispatchTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        CachePacketManager::GetInstance().Initialize();
        g_parsed.clear();
        handlers_ = MakeTestHandlers();
        session_ = std::make_shared<Session>(&handlers_);
    }
    // 패킷 하나 투입
    bool Feed(Uint32 seq, Uint8 flags)
    {
        auto p = MakePacket(seq, flags);
        return session_->RecvDispatch(p.data(), (Uint16)p.size());
    }
    // 현재 사용 중인 캐시 버퍼 개수 (누수 검증용)
    size_t UsedBuffers()
    {
        return CachePacketManager::GetInstance().GetUseList().size();
    }

    Map<Uint8, Packet::PacketHandler> handlers_;
    std::shared_ptr<Session> session_;
};

// 재전송 x, 최신 순서대로
TEST_F(RecvDispatchTest, NoRepeat_NewestOnly)
{
    const Uint8 flags = Packet::NONE;
    const size_t base = UsedBuffers();

    Feed(1, flags);      // 수락
    Feed(3, flags);      // 수락 (더 최신)
    Feed(2, flags);      // 과거 → 버림
    Feed(3, flags);      // 중복 → 버림
    Feed(100, flags);    // 64칸 이상 전방 점프 (대량 유실 상황) → 수락되어야 함

    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 3, 100}));
    EXPECT_EQ(UsedBuffers(), base);   // 캐싱 자체가 없어야 함
}

// 재전송 x, 경계 되는지 
TEST_F(RecvDispatchTest, NoRepeat_Wrap)
{
    const Uint8 flags = Packet::NONE;
    constexpr Uint32 maxVal = std::numeric_limits<uint32_t>::max();
    session_->rNopsNo_.recvRepeatSeq_ = maxVal - 1;   // 경계 직전 상태로 세팅

    Feed(maxVal, flags);    // 수락
    Feed(0, flags);         // 경계 넘는 최신 → 수락
    Feed(maxVal, flags);    // 이제는 과거 → 버림
    Feed(5, flags);         // 수락

    EXPECT_EQ(g_parsed, (std::vector<Uint32>{maxVal, 0, 5}));
}

// 채널, 순차적용
TEST_F(RecvDispatchTest, Channel_Ord)
{
    const Uint8 flags = ChannelFlags(1);
    Feed(1, flags);
    Feed(2, flags);
    Feed(3, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 2, 3}));
}

// 채널, 중복전송
TEST_F(RecvDispatchTest, Channel_Duplicate)
{
    const Uint8 f = ChannelFlags(1);
    Feed(1, f); 
    Feed(2, f);
    EXPECT_TRUE(Feed(2, f));   // 재전송 — ack만 다시 나가야 함
    EXPECT_TRUE(Feed(2, f));
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 2}));
}

// 채널, 순서 역전 -> 순차로 들어오는지
TEST_F(RecvDispatchTest, Channel_Reorder)
{
    const Uint8 flags = ChannelFlags(1);
    Feed(1, flags);
    Feed(3, flags);                 // 캐싱 (실행되면 안 됨)
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1}));
    Feed(2, flags);                 // 2 실행 + 캐싱된 3 이어서 실행
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 2, 3}));
}

// 캐싱 버퍼 확인(같은 메시지)
TEST_F(RecvDispatchTest, Channel_CachedRetransmit_NoLeak)
{
    const Uint8 flags = ChannelFlags(1);
    const size_t count = UsedBuffers();
    Feed(1, flags);
    Feed(3, flags);                                 // 캐싱 → 버퍼 1개 사용
    Feed(3, flags); 
    Feed(3, flags);                                 // 캐싱된 패킷의 재전송 → 추가 Pop 없어야 함
    EXPECT_EQ(UsedBuffers(), count + 1);
    Feed(2, flags);                                 // 3 소비 → 버퍼 반환
    EXPECT_EQ(UsedBuffers(), count);             // 누수 검증
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 2, 3}));
}

// 채널, 대기확인
TEST_F(RecvDispatchTest, Channel_TwoGaps)
{
    const Uint8 flags = ChannelFlags(1);
    Feed(1, flags); 
    Feed(4, flags); 
    Feed(2, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 2}));   // 4는 아직 대기 (3 없음)
    Feed(3, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 2, 3, 4}));
}

// 재전송만, 순서x 확인
TEST_F(RecvDispatchTest, Repeat_NoOrd)
{
    const Uint8 flags = ChannelFlags(0);
    Feed(1, flags);
    Feed(4, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 4}));
    Feed(3, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 4, 3}));
    Feed(2, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 4, 3, 2}));
}

// 재전송, 중복 패킷 스킵
TEST_F(RecvDispatchTest, Repeat_Skip)
{
    const Uint8 flags = ChannelFlags(0);
    Feed(1, flags); Feed(3, flags); Feed(3, flags); // 3 재전송 → 비트로 스킵
    Feed(2, flags);                                 // 2 실행, while로 recv가 3까지 전진
    Feed(3, flags); Feed(2, flags);                 // 이제 recv 뒤 → 중복 검사에서 스킵
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1, 3, 2}));
}

// 채널, 최대 DEFAULT_WINDOW_COUNT 영역까지 수신 가능 체크 
TEST_F(RecvDispatchTest, WindowExceeded_ReturnsFalse)
{
    const Uint8 flags = ChannelFlags(1);
    EXPECT_TRUE(Feed(1, flags));
    EXPECT_FALSE(Feed(100, flags));   // recv=1에서 99칸 앞 → 거부
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{1}));
}

static constexpr Uint32 U32MAX = 0xFFFFFFFFu;   // 4294967295

// 채널 경계 체크
TEST_F(RecvDispatchTest, Wrap_Channel_InOrder)
{
    const Uint8 flags = ChannelFlags(1);
    auto& rps = session_->cacheAlreadyPackets_[1].reliablePacketState;
    // 4294967293까지 처리한 상태로 세팅
    rps.recvRepeatSeq_ = U32MAX - 2;
    rps.lastRepeatSeq_ = U32MAX - 2;

    Feed(U32MAX - 1, flags);
    Feed(U32MAX, flags);
    Feed(0, flags);
    Feed(1, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{U32MAX - 1, U32MAX, 0, 1}));
}

// 채널, 경계 순서 역전 -> 순서대로 체크
TEST_F(RecvDispatchTest, Wrap_Channel_ReorderAcrossBoundary)
{
    const Uint8 flags = ChannelFlags(1);
    auto& rps = session_->cacheAlreadyPackets_[1].reliablePacketState;
    rps.recvRepeatSeq_ = U32MAX - 2;
    rps.lastRepeatSeq_ = U32MAX - 2;

    Feed(U32MAX - 1, flags);
    Feed(1, flags);              // 경계 너머 미래 → 캐싱되어야 함 (실행 X)
    Feed(0, flags);              // 캐싱
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{U32MAX - 1}));
    Feed(U32MAX, flags);         // max 실행 + 캐싱된 0, 1 이어서 실행
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{U32MAX - 1, U32MAX, 0, 1}));
}

// 순서, 이전패킷 전달 ACK 처리되고, 중복처리 안되는지
TEST_F(RecvDispatchTest, Wrap_Channel_OldDuplicateAcrossBoundary)
{
    const Uint8 flags = ChannelFlags(1);
    auto& rps = session_->cacheAlreadyPackets_[1].reliablePacketState;
    rps.recvRepeatSeq_ = 1;         // 이미 0을 지나 1까지 처리한 상태
    rps.lastRepeatSeq_ = 1;

    EXPECT_TRUE(Feed(U32MAX, flags));   // wrap 이전의 낡은 재전송 → 중복 처리(재실행 X)
    EXPECT_TRUE(Feed(0, flags));
    EXPECT_TRUE(g_parsed.empty());
}

// 재전송, 순서x 처리 되는지, 중복 / 최신 아닌거 스킵 확인
TEST_F(RecvDispatchTest, Wrap_NoOrder)
{
    const Uint8 flags = Packet::REPEAT;
    session_->rpsNo_.recvRepeatSeq_ = U32MAX - 2;
    session_->rpsNo_.lastRepeatSeq_ = U32MAX - 2;

    Feed(U32MAX - 1, flags);
    Feed(0, flags);              // 경계 너머, 즉시 실행 + 비트 기록
    Feed(0, flags);              // 재전송 → 스킵
    Feed(U32MAX, flags);         // 실행, while로 recv가 0까지 전진
    Feed(1, flags);
    EXPECT_EQ(g_parsed, (std::vector<Uint32>{U32MAX - 1, 0, U32MAX, 1}));
}

TEST_F(RecvDispatchTest, Channel_RandomStress)
{
    const Uint8 f = ChannelFlags(1);
    const size_t count = UsedBuffers();
    std::mt19937 rng(12345);            // 시드 고정
    Uint32 next = 1;                    // 아직 투입 안 한 가장 낮은 seq
    std::vector<Uint32> pending;        // 셔플 대기열
    pending.reserve(32);

    while (next <= 10000 || !pending.empty())
    {
        // 윈도우(64) 안에서만 미래 패킷을 대기열에 채움
        while (next <= 10000 && pending.size() < 32)
        {
            pending.push_back(next++);
        }
        std::shuffle(pending.begin(), pending.end(), rng);

        // 절반만 테스트
        while (!pending.empty())
        {
            Uint32 seq = pending.back(); 
            pending.pop_back();
            Feed(seq, f);
            if (rng() % 4 == 0)
            {
                Feed(seq, f); // 25% 확률 중복
            }
        }
    }

    ASSERT_EQ(g_parsed.size(), 10000u);
    for (Uint32 i = 0; i < 10000; ++i)
    {
        ASSERT_EQ(g_parsed[i], i + 1) << "순서 깨짐: index " << i;
    }
    EXPECT_EQ(UsedBuffers(), count); // 누수 체크
}
