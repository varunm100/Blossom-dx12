#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include <d/stdafx.h>
#include <d/Types.h>
#include <dxc/dxcapi.h>

namespace d {

  enum class ShaderType {
    VERTEX,
    FRAGMENT,
    COMPUTE,
  };

  struct ShaderEntry {
    std::string path;
    ShaderType type;
    ComPtr<IDxcBlob> code;
  };

  struct AssetLibrary {
    ComPtr<IDxcLibrary> dxc_library;
    ComPtr<IDxcCompiler> dxc_compiler;

    std::vector<ShaderEntry> shader_entries;
    std::unordered_map<std::string, u32> shader_library;

    void init();
    void add_shader(const char* path, ShaderType type, const char* asset_name, const char* entry_point = nullptr) noexcept;
    ShaderEntry& get_shader_asset(const char* shader_file_name);
  };
}
