#include "LayerStack.h"

void Rapture::LayerStack::pushLayer(Layer* layer)
{
	m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
	m_LayerInsertIndex++;
}

void Rapture::LayerStack::pushOverlay(Layer* overlay)
{
}

void Rapture::LayerStack::popLayer(Layer* layer)
{
}

void Rapture::LayerStack::popOverlay(Layer* overlay)
{
}
