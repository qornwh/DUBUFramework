# DUBU Framework

Windows IOCP 기반 **RUDP(Reliable UDP)** 게임 서버 프레임워크.
UDP 위에 신뢰성 계층을 직접 구현하여, **패킷 단위로 신뢰성 여부를 선택**할 수 있습니다.

> 상세 아키텍처 문서: `~/.claude/docs/DUBU-Framework.md`

---

## 구조

### 솔루션 구성 (`DUBU.sln`)

| 프로젝트 | 종류 | 역할 |
|----------|------|------|
| **DUBUFrameWork** | static lib | 핵심 프레임워크 |
| **DUBUMain** | exe | 최소 서버+클라이언트 스모크 테스트 |
| **DUBUEchoServer** | exe | 에코/채팅 서버 샘플 (핸들러 + 브로드캐스트) |
| **DUBUEchoClient** | exe | 대화형 신뢰-정렬 클라이언트 샘플 |
| **GateWay** | exe | 프록시 계층 (다운스트림 서버 + 업스트림 InternalClient) |
| **DUBUTest** | exe | gtest 단위 테스트 |

### 프레임워크 모듈 (`DUBUFrameWork/`)

```
소켓 / IOCP
  RUDPSocket          Overlapped WSARecvFrom/WSASendTo 엔진, ISocketHandler
  SocketConfig        UDP 소켓 생성/바인드, SIO_UDP_CONNRESET 처리
  OverlappedObj       IOCP 완료 라우팅 태그 (RECVEFROM/SENDTO/RELIABLE)

서버 / 세션
  Server              인바운드 상태 머신, 세션 생명주기 소유, CheckSession tick
  SessionManager      세션 레지스트리 (lock-free, Server가 직렬화)
  Session             연결별 상태, 재정렬/중복제거/재전송
  Peer                (key, addr, session) 매핑

클라이언트
  Client              Server의 상대편, ping 응답자
  InternalClient      서버-투-서버 클라이언트 (Gateway 업스트림용)

패킷 / 신뢰성
  Packet              헤더/플래그/CRC32, PacketHandler 디스패치
  Subheader           다중 홉 세션 상관관계용 서브헤더
  PacketStateBase     시퀀스 장부 (수신 비트마스크)
  ReliablePacketState 64-슬롯 슬라이딩 윈도우, ACK/RTT 처리
  PendingPacket       송신 재전송 슬롯
  CachePacket         수신 순서-대기 슬롯

메모리 / 동기화
  Pool                커스텀 슬랩 할당자 (크기 클래스별 free-list)
  ObjectPool          placement-new 오브젝트 풀
  BufferManager       고정 1KB 버퍼 풀 (송수신 / 캐시)
  RWLock              스핀 기반 reader/writer 락

공통
  Config / SocketConfig   JSON 런타임 설정
  Singleton, Types, ConnectionType, pch
```

### 의존성 (`vcpkg.json`)

- **flatbuffers** 25.1.21 — 패킷 직렬화
- **spdlog** 1.17.0 — 로깅
- **nlohmann-json** 3.12.0 — 설정 파싱
- gtest — 단위 테스트

### 패킷 흐름

```
[인바운드]
  WSARecvFrom 완료 → RUDPSocket::Dispatch → RecvFromComplete (헤더+CRC32 검증)
  → OnRecvFrom (플래그 상태 머신) → Session::RecvDispatch (seq/채널 재정렬)
  → PacketParse → FlatBuffers 검증 → 사용자 핸들러  (REPEAT면 SendAck)

[아웃바운드]
  SendPacket(opt) → seq로 헤더 구성 → CRC32
  → SendToReliable(유지) / SendTo(free) → 신뢰면 AddPendingPacket

[신뢰성 루프]
  미-ACK pending → CheckSession → RepeatMessageAll → SendToRepeat (RTT*2+20ms)
  ACK 수신 → AckProcess (슬롯 free, 윈도우 슬라이드, RTT EWMA 0.8/0.2)

[Liveness]
  3s idle → 서버 SendPing → 클라이언트 PONG응답 / 30s idle → disconnect
```

### 채널 (순서 보장)

신뢰-정렬 패킷은 **채널** 단위로 순서를 보장한다. 채널은 별도 필드가 아니라
헤더 `flags_` 바이트 안에 인코딩된다.

**1) flags_ 바이트에 채널 인코딩** — 예시 `0x15` = 채널 2 · 신뢰(REPEAT) 패킷

| bit | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
|-----|---|---|---|---|---|---|---|---|
| 의미 | ACK | – | ch id | ch id | ch id | CHANNEL | CHUNK | REPEAT |
| `0x15` | 0 | 0 | 0 | 1 | 0 | 1 | 0 | 1 |

bits 5~3 = `010₂` = **채널 2**, bit2 = CHANNEL, bit0 = REPEAT.

**2) 채널 인덱스로 라우팅**

```
수신 REPEAT 패킷
   │
   ▼
채널 id = (flags >> 3) & 0b0111          // 0b0001_0101 → 2
   │
   ▼
cacheAlreadyPackets_[채널 id]            // 채널별 독립 상태
```

**3) 채널별 독립 상태** — `cacheAlreadyPackets_[g_channelMask + 1]`

| | 채널 0 | 채널 1 | 채널 2 | 채널 3 |
|---|---|---|---|---|
| 시퀀스 | 독립 | 독립 | 독립 | 독립 |
| 재정렬 버퍼 | `CachePacket[64]` | `[64]` | `[64]` | `[64]` |

각 채널이 시퀀스와 재정렬 버퍼를 따로 가지므로, **한 채널에서 패킷이 유실돼
대기하더라도 다른 채널은 막히지 않는다** (head-of-line blocking 제거).

---

## RUDP 프로토콜 구현

### 배경
기본적으로 TCP 기반으로 서버를 구현하였으며,
모든 패킷을 신뢰성 기반으로 처리하는 구조를 사용했습니다.

게임 서버를 구현하는 과정에서 패킷의 성격에 따라
신뢰성이 반드시 필요하지 않은 경우도 존재한다고 판단했습니다.

예를 들어 이동/회전과 같은 입력은 최신 상태 반영이 중요하며,
이전 패킷의 재전송이 반드시 필요하지 않을 수 있습니다.

---

### 설계

이러한 상황을 고려하여
패킷 단위로 신뢰성 여부를 선택할 수 있는 구조를 직접 구현했습니다.

- UDP 기반 통신 위에 RUDP(Reliable UDP) 프로토콜 구현
- 패킷 Header에 flag를 두어 신뢰성 여부 선택 가능
  - NONE / ACK / REPEAT / PING / PONG
- 시퀀스 번호 기반 패킷 순서 관리
- ACK 기반 재전송 구조 구현
- Pending Packet 큐를 활용한 재전송 관리 (슬라이딩 윈도우 방식)

---

### 구현 내용

- 패킷 단위로 신뢰성 옵션을 분리하여 처리
- ACK 미수신 시 일정 시간 이후 재전송
- RTT 기반 재전송 타이밍 설정 (RTT * 2 + 여유 시간)
- 주기적인 ping/pong을 통해 연결 상태 확인
- 시퀀스 번호를 활용한 중복 및 순서 관리
- 채널(channel) 단위 순서 보장 — 채널별 독립 시퀀스/재정렬 버퍼

---

### 결과

- TCP 기반 구조 외에,
  패킷 특성에 따라 신뢰성 여부를 선택할 수 있는 구조를 설계 및 구현

- 단순 API 사용이 아닌,
  네트워크 프로토콜 레벨에서의 동작을 직접 구현하며
  통신 구조에 대한 이해도를 높일 수 있었습니다.
