#pragma once
#include "pch.h"

namespace DUBU
{
	template <typename T>
	class Singleton
	{
	public:
		virtual ~Singleton() {};
		static T& GetInstance();
	};

	template<typename T>
	inline T& Singleton<T>::GetInstance()
	{
		static T instance;
		return instance;
	}
}
