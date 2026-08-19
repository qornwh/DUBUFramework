#pragma once
#include "pch.h"

namespace DUBU 
{
    namespace Packet
    {
        /*
        * Subheader : 서버간 연결시 기본 헤더 정보외에, 추가적인 정보에 사용하기 위한 헤더 구조체
        *  - 패킷에 넣기 애매한 정보를 담는다.
        *  - 세션 id 정보 교환 등.
        *  - send시 직접 데이터를 넣어야됨. 
        *  - 기본으로 타입, 2개의 함수로 이루어진다.
        */

        template<typename T, Uint8 Type>
        struct Subheader
        {
            Uint8 type_ = Type;
            Uint8 size_ = 0;
            const Subheader<T, Type>* GetSubheader() const;
            Uint8 GetSize();
        };

        // 베이스 <= 이건 타입 추론용으로만 사용됨
        struct SubheaderBase : public Subheader<SubheaderBase, 1>
        {
        };

        template<typename T, Uint8 Type>
        inline const Subheader<T, Type>* Subheader<T, Type>::GetSubheader() const
        {
            return this;
        }

        template<typename T, Uint8 Type>
        inline Uint8 Subheader<T, Type>::GetSize()
        {
            return size_;
        }
    }
}

