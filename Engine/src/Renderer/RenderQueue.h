#pragma once

#include <vector>
#include <algorithm>
#include "RenderCommands.h"
#include <string>
#include <queue>
#include <variant>

namespace Rapture {

    enum class RenderQueueType {
        GEOMETRY,
        POSTPROCESS,
        SHADOWMAP
    };

    //using CommandVariant = std::variant<RenderCommand, PostProcessCommand>;
    using CommandVariant = std::variant<std::monostate, RenderCommand, PostProcessCommand, AnimationSetupCommand>;

    class RenderQueue {
    public:

        RenderQueue(const std::string& name, RenderQueueType type) : m_name(name), m_type(type) {}

        void clear() {
            while (!m_queue.empty()) {
                m_queue.pop();
            };
        }
        
        void add(const CommandVariant& cmd) {
            m_queue.push(cmd);
        }
        
        void sort() {
            GE_RENDER_ERROR("Sorting not implemented for RenderQueue");
        }
        
        const CommandVariant serve() {
            if (m_queue.empty()) {
                return std::monostate();
            }

            const CommandVariant cmd = m_queue.front();
            m_queue.pop();
            return cmd;
        }

        const CommandVariant& peek() const {
            return m_queue.front();
        }

        bool empty() const {
            return m_queue.empty();
        }


    public:
        const std::string m_name;
        const RenderQueueType m_type;

    private:

        // commands to render
        std::queue<CommandVariant> m_queue;
        
    };

        // class which takes in a scene and returns a Queue of commands
    class CommandQueueBuilder {
        public:
            static RenderQueue buildGeometryCommandQueue(const std::shared_ptr<Scene>& scene);
            static RenderQueue buildPostProcessCommandQueue(const std::shared_ptr<Scene>& scene);

    };



}
