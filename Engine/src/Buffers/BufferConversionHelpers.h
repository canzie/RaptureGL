#pragma once

#include "Buffers.h"
#include "glad/glad.h"
#include <vector>
#include <unordered_map>
#include "VertexArray.h"

namespace Rapture {

    // Helper functions for buffer usage
	inline GLenum convertBufferUsage(BufferUsage usage) {
		switch (usage) {
			case BufferUsage::Static: return GL_STATIC_DRAW;
			case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
			case BufferUsage::Stream: return GL_STREAM_DRAW;
			default: return GL_STATIC_DRAW;
		}
	}

	inline GLenum convertBufferStorageFlags(BufferUsage usage) {
		switch (usage) {
			case BufferUsage::Static: 
				return GL_DYNAMIC_STORAGE_BIT;
			case BufferUsage::Dynamic: 
				return GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
			case BufferUsage::Stream: 
				return GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
			default: 
				return GL_DYNAMIC_STORAGE_BIT;
		}
	}

    /**
     * @brief Helper functions for converting between different buffer formats
     */
    namespace BufferConversionHelpers {

        /**
         * @brief Convert non-interleaved vertex data to interleaved format
         * 
         * @param attributes Map of attribute names to their data
         * @param vertexCount Number of vertices
         * @return std::vector<unsigned char> Interleaved vertex data
         */
        inline std::vector<unsigned char> convertToInterleaved(
            const std::unordered_map<std::string, std::vector<unsigned char>>& attributes,
            size_t vertexCount) 
        {
            // Calculate vertex size
            size_t vertexSize = 0;
            for (const auto& [name, data] : attributes) {
                vertexSize += data.size() / vertexCount;
            }

            // Prepare output buffer
            std::vector<unsigned char> result(vertexCount * vertexSize);

            // Fill interleaved data
            for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
                size_t outputOffset = vertexIndex * vertexSize;
                
                for (const auto& [name, data] : attributes) {
                    size_t attributeSize = data.size() / vertexCount;
                    size_t inputOffset = vertexIndex * attributeSize;
                    
                    // Copy this attribute's data for this vertex
                    std::memcpy(
                        result.data() + outputOffset,
                        data.data() + inputOffset,
                        attributeSize
                    );
                    
                    outputOffset += attributeSize;
                }
            }

            return result;
        }

        /**
         * @brief Convert interleaved vertex data to non-interleaved format
         * 
         * @param interleavedData Interleaved vertex data
         * @param layout Buffer layout containing attribute information
         * @param vertexCount Number of vertices
         * @return std::unordered_map<std::string, std::vector<unsigned char>> Map of attribute names to their data
         */
        inline std::unordered_map<std::string, std::vector<unsigned char>> convertToNonInterleaved(
            const std::vector<unsigned char>& interleavedData,
            const BufferLayout& layout,
            size_t vertexCount)
        {
            std::unordered_map<std::string, std::vector<unsigned char>> result;
            
            if (!layout.isInterleaved || layout.vertexSize == 0) {
                GE_CORE_ERROR("convertToNonInterleaved: Layout must be interleaved with valid vertex size");
                return result;
            }

            // Initialize output buffers for each attribute
            for (const auto& attrib : layout.buffer_attribs) {
                size_t attributeSize = attrib.getSizeInBytes();
                result[attrib.name].resize(attributeSize * vertexCount);
            }

            // Extract each attribute into its own buffer
            for (size_t vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
                size_t inputOffset = vertexIndex * layout.vertexSize;
                
                for (const auto& attrib : layout.buffer_attribs) {
                    size_t attributeSize = attrib.getSizeInBytes();
                    size_t attributeOffset = attrib.offset;
                    size_t outputOffset = vertexIndex * attributeSize;
                    
                    // Copy this attribute's data for this vertex
                    std::memcpy(
                        result[attrib.name].data() + outputOffset,
                        interleavedData.data() + inputOffset + attributeOffset,
                        attributeSize
                    );
                }
            }

            return result;
        }
        
        /**
         * @brief Create a buffer layout for interleaved vertex data
         * 
         * @param attributes Vector of buffer attributes
         * @return BufferLayout Interleaved buffer layout
         */
        inline BufferLayout createInterleavedLayout(const std::vector<BufferAttribute>& attributes) {
            BufferLayout layout;
            layout.isInterleaved = true;
            layout.buffer_attribs = attributes;
            layout.updateOffsets(); // This will calculate offsets and vertex size
            return layout;
        }
    }
} // namespace Rapture