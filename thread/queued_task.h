#pragma once

namespace dd {
	class QueuedTask
	{
	public:
		virtual ~QueuedTask() = default;
		virtual bool Run() = 0;
	};

}	// namespace dd
