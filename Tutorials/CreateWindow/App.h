﻿#pragma once

#include "pch.h"

namespace CreateWindow
{
	// 응용 프로그램의 주 진입점입니다. 응용 프로그램을 Windows 셸과 연결하고 응용 프로그램 수명 주기 이벤트를 처리합니다.
	ref class App sealed : public IFrameworkView
	{
	private:
		bool m_windowClosed;
		bool m_windowVisible;

	public:
		App();

		// IFrameworkView 메서드.
		virtual void Initialize(CoreApplicationView^ applicationView);
		virtual void SetWindow(CoreWindow^ window);
		virtual void Load(Platform::String^ entryPoint);
		virtual void Run();
		virtual void Uninitialize();

	protected:
		// 응용 프로그램 수명 주기 이벤트 처리기입니다.
		void OnActivated(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, Windows::ApplicationModel::Activation::IActivatedEventArgs^ args);
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ args);
		void OnResuming(Platform::Object^ sender, Platform::Object^ args);

		// 창 이벤트 처리기입니다.
		void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);
		void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

		// 입력 이벤트 처리기입니다.
		void OnPointerPressed(CoreWindow^ Window, PointerEventArgs^ Args);
		void OnPointerWheelChanged(CoreWindow ^ Window, PointerEventArgs ^ Args);
		void OnKeyDown(CoreWindow^ Window, KeyEventArgs^ Args);
		void OnKeyUp(CoreWindow^ Window, KeyEventArgs^ Args);
	

	};

	
}
