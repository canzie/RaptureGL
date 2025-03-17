#pragma once
#include "../Events/Events.h"
#include "../Scenes/Scene.h"

#include <string>

namespace Rapture {
	static unsigned int s_layer_ID = 0;

	class Layer {
	public:

		Layer(const std::string name = "Layer_"+std::to_string(s_layer_ID))
			: m_debug_name(name) {
			s_layer_ID++;
		}
		
		virtual void onAttach() = 0;
		virtual void onDetach() = 0;
		virtual void onUpdate(float ts) = 0;
		virtual void onEvent(Event& event) = 0;

		std::string getLayerName() { return m_debug_name; }
		

	private:
		std::string m_debug_name;

	};

}