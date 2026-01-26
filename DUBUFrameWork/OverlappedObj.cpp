#include "OverlappedObj.h"

void DUBU::OverlappedObj::Initialize()
{
	addrSize_ = sizeof(remoteAddr_);
	ZeroMemory(&overlapped_, sizeof(overlapped_));
}

void DUBU::OverlappedObj::SetType(OverlappedObjType type)
{
	type_ = type;
}
