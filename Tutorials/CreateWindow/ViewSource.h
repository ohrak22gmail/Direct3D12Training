#pragma once

namespace CreateWindow
{
	ref class ViewSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
	{
	public:
		virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView();
	};
}