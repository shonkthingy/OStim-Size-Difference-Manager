#pragma once

namespace Graph
{
	class Node
	{
	public:
		virtual const char* getNodeID() = 0;
		virtual ~Node() = default;
	};
}
