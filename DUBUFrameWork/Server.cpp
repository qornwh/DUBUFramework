#include "Server.h"
#include "RUDPSocket.h"
#include "Packet.h"
#include "BufferManager.h"

DUBU::Server::Server() : isRunning_(false), sessionManager_(SessionManager{})
{
	rudpSocket_ = std::make_shared<RUDPSocket>();
	rudpSocket_->SetHandler(this);
}

DUBU::Server::~Server()
{
	rudpSocket_->EndServer();
}

void DUBU::Server::Initialize()
{
	rudpSocket_->StartServer();
	isRunning_ = true;
}

void DUBU::Server::Run()
{
	while (isRunning_)
	{
		assert(rudpSocket_ != nullptr);
		LPOVERLAPPED ptr = nullptr;
		Int32 size = rudpSocket_->Dispatch(&ptr);

		if (ptr == nullptr)
			continue;

		OverlappedObj* ptr2 = reinterpret_cast<OverlappedObj*>(ptr);
		if ((ptr2->type_ & OverlappedObjType::RECVEFROM) == OverlappedObjType::RECVEFROM)
			rudpSocket_->RecvFromComplete(ptr, size);
		else if ((ptr2->type_ & OverlappedObjType::SENDTO) == OverlappedObjType::SENDTO)
			rudpSocket_->SendToComplete(ptr, size);
	}
}

void DUBU::Server::Stop()
{
	isRunning_ = false;
}

void DUBU::Server::OnRecvFrom(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);

	auto buffer = opbPtr->buffer_;
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(buffer);

	auto sessionId = header->sessionId_;
	auto flag = header->flags_;
	Uint64 key = PeerKey(addr);
	auto it = peerMap_.find(key);
	Bool result = false;

	if (flag == Packet::PacketHeaderFlag::SESSION)
	{
		if (it == peerMap_.end())
		{
			// 세션 추가
			Session* session = CreateSession(addr);
			header->sessionId_ = session->GetSessionId();   // ← 추가
			// 세션 추가 완료 응답
			ConnectMessage(session);
			// Peer 생성
			peerMap_.emplace(key, Peer{key, addr, session});
		}
		else
		{
			// 중복으로 오는경우 재전송
			Session* session = it->second.session_;
			ConnectMessage(session);
		}
	}
	else
	{
		// 세션이 있을때
		if (sessionId > 0)
		{
			ReadLockGuard rl(sessionLock_);
			Session* session = sessionManager_.GetSession(sessionId);
			if (session == nullptr)
			{
				spdlog::error("sessionId{} not found", sessionId);
				return;
			}

			// NONE or REPEAT 일때는 패킷에 대한 내용을 처리
			if ((flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT || flag == Packet::PacketHeaderFlag::NONE)
			{
				result = session->RecvDispatch(buffer, size);
			}

			// ACK 일때는 클라쪽에서 제대로 받고 다시 보내왔다는 것이다.
			if ((flag & Packet::PacketHeaderFlag::ACK) == Packet::PacketHeaderFlag::ACK)
			{
				result = session->RecvDispatchACK(buffer, size);
			}
		}
	}

	// repeat ACK전송
	if (result && (flag & Packet::PacketHeaderFlag::REPEAT) == Packet::PacketHeaderFlag::REPEAT)
	{
		auto remote = opbPtr->remoteAddr_;
		auto addSize = opbPtr->size_;
		rudpSocket_->SendTo(remote, buffer, addSize);
	}

	// 일단 실패할때 코드 및 대시보드에 띄울 데이터 큐에 넣기?
}

void DUBU::Server::OnSendTo(const SOCKADDR_IN& addr, Uint8* ptr, Uint16 size)
{
	OverlappedPacketBuffer* opbPtr = reinterpret_cast<OverlappedPacketBuffer*>(ptr);
}

DUBU::Session* DUBU::Server::CreateSession(const SOCKADDR_IN& addr)
{
	WriteLockGuard wl(sessionLock_);
	Session* session = sessionManager_.AddSession();
	session->SetSockAddr(addr);
	return session;
}

void DUBU::Server::ConnectMessage(Session* session)
{
	OverlappedPacketBuffer* opb = PacketManager::GetInstance().PopPacketBuffer();
	opb->SetType(Packet::PacketHeaderFlag::SESSION);
	Packet::PacketHeader* header = reinterpret_cast<Packet::PacketHeader*>(opb->buffer_);

	// 헤더 작성
	header->checksum_ = 0;
	header->flags_ = Packet::PacketHeaderFlag::SESSION;
	header->totalSize_ = sizeof(Packet::PacketHeader);
	header->sessionId_ = session->GetSessionId();
	header->sequenceNo_ = 0;
	header->timestamp_ = 0;

	// 전체 패킷 사이즈 설정
	opb->size_ = header->totalSize_;

	// crc32 암호화
	Uint32 checksum = Packet::Packet::CRC32(opb->buffer_, header->totalSize_);
	header->checksum_ = checksum;
	rudpSocket_->SendTo(session->GetSockAddr(), opb->buffer_, header->totalSize_);
}

void DUBU::Server::CheckSession()
{
	// 시간체크
	Uint64 now = GetCurrentTimeMs();

	// 일단 여기에 한 스레드만 통과되도록 처리가 필요
	// ex) CAS or lock(mutex) write만 할거라 mutex가 나음

	// 끊을 세션
	Uint8 idx = 0;

	// 재전송로직 실행
	for (auto& [_, session] : sessionManager_.GetSessions())
	{
		Uint64 delayTime = now - session->GetTimestamp();
		if (delayTime > SessionTimeout)
		{
			// 끊김 감지
			removeListCache_[idx++] = session->GetSessionId();
		}
		else if (delayTime > PingTimeout)
		{
			// Ping으로 연결 감지 체크
		}

		// 재전송, 왕복시간은 * 2 + DEFAULT_RTT_MS_DELAY
		session->RepeatACK(rudpSocket_.get(), session->GetRttMillisec() * 2 + DEFAULT_RTT_MS_DELAY);
	}

	// 연결 해제 구간
	for (Int32 i = 0; i < idx; ++i) 
	{
		Int32 sessionId = removeListCache_[i];

	}
}

Uint64 DUBU::Server::PeerKey(const SOCKADDR_IN& addr)
{
	// ip를 16비트 시프트후 port와 or연산으로 key관리
	return ((Uint64)addr.sin_addr.s_addr << 16) | (Uint64)ntohs(addr.sin_port);
}
