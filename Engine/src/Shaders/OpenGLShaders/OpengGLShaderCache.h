#include <string>
#include <unordered_map>
#include <memory>
#include "../Shader.h"

/*

namespace Rapture {

class OpenGLShaderCache {
    public:
        static void initialize(const std::string& cacheDir);
        static std::shared_ptr<Shader> getShader(const std::string& name);
        static void saveShader(const std::string& name, const Shader& shader);
        static void clearCache();
        
    private:
        static std::string m_cacheDir;
        static std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
        
        static std::string generateShaderKey(const Shader& shader);
        static bool validateShaderCache(const std::string& key);
    };

}

*/