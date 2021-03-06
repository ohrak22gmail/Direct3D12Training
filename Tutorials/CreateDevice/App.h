﻿#pragma once

#include "pch.h"
#include "Device.h"

namespace CreateDevice
{
	// 응용 프로그램의 주 진입점입니다. 응용 프로그램을 Windows 셸과 연결하고 응용 프로그램 수명 주기 이벤트를 처리합니다.
	ref class App sealed : public Windows::ApplicationModel::Core::IFrameworkView
	{
	public:
		App();

		// IFrameworkView 메서드.
		virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
		virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
		virtual void Load(Platform::String^ entryPoint);
		virtual void Run();
		virtual void Uninitialize();

	protected:
		// 응용 프로그램 수명 주기 이벤트 처리기입니다.
		void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);

		// 창 이벤트 처리기입니다.
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
		void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

	private:
		// m_deviceResources에 대한 전용 접근자는 장치 제거 오류로부터 보호합니다.
		bool m_windowClosed;
		bool m_windowVisible;
		std::shared_ptr<Device> m_device;
	};
}

ref class Direct3DApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
};
