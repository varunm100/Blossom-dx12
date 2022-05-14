#include "d/AssetLibrary.h"
#include "d/Logging.h"

namespace d {
  void AssetLibrary::init() {
    DX_CHECK(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&dxc_library)));
    DX_CHECK(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler)));
  }

  const wchar_t* to_wchar(const char* str) {
    const size_t cSize = strlen(str) + 1;
    const auto path_wc = new wchar_t[cSize];
    mbstowcs(path_wc, str, cSize);
    return path_wc;
  }

  void AssetLibrary::add_shader(const char* path, ShaderType type, const char* asset_name, const char* entry_point) noexcept {
    if(shader_library.contains(asset_name)) {
      spdlog::error("Shader asset named {} already exits", asset_name);
    }
    uint32_t codePage = CP_UTF8;
    ComPtr<IDxcBlobEncoding> sourceBlob;
    const wchar_t* path_wc = to_wchar(path);
    HRESULT hr = dxc_library->CreateBlobFromFile(path_wc, &codePage, &sourceBlob);
    delete[] path_wc;

    if (FAILED(hr)) {
      spdlog::error("Could not find file {}", path);
      return;
    }

    const std::string p(path);
    std::string name = p.substr(p.find_last_of("/\\") + 1);
    std::string e_point;
    const wchar_t* target_profile{};
    if (entry_point == nullptr) {
      switch (type)
      {
      case ShaderType::VERTEX:
        e_point = "VSMain";
        break;
      case ShaderType::FRAGMENT:
        e_point = "PSMain";
        break;
      case ShaderType::COMPUTE:
        e_point = "CSMain";
        break;
      }
    } else {
      e_point = entry_point;
    }
    switch (type)
    {
    case ShaderType::VERTEX:
      target_profile = L"vs_6_6";
      break;
    case ShaderType::FRAGMENT:
      target_profile = L"ps_6_6";
      break;
    case ShaderType::COMPUTE:
      target_profile = L"cs_6_6";
      break;
    }

    const wchar_t* name_wc = to_wchar(name.c_str());
    const wchar_t* entry_point_wc = to_wchar(e_point.c_str());

    ComPtr<IDxcOperationResult> result{};
    hr = dxc_compiler->Compile(sourceBlob.Get(), name_wc, entry_point_wc, target_profile, nullptr, 0, nullptr, 0,
                               nullptr, &result);
    if (SUCCEEDED(hr)) {
      result->GetStatus(&hr);
    } else {
      spdlog::error("Failed to compile {}", name);
      return;
    }
    if (FAILED(hr)) {
      if (result) {
        ComPtr<IDxcBlobEncoding> errorsBlob;
        hr = result->GetErrorBuffer(&errorsBlob);
        if (SUCCEEDED(hr) && errorsBlob) {
          spdlog::error("Compilation error, {}: {}", asset_name, static_cast<const char*>(errorsBlob->GetBufferPointer()));
          delete[] name_wc;
          delete[] entry_point_wc;
          return;
        }
      }
    }
    delete[] name_wc;
    delete[] entry_point_wc;
    ShaderEntry entry{
      .path = path,
      .type = type,
    };
    result->GetResult(&entry.code);
    shader_entries.emplace_back(entry);
    shader_library[asset_name] = static_cast<u32>(shader_entries.size() - 1u);
    spdlog::info("Loaded shader {}", asset_name);
  }

  ShaderEntry& AssetLibrary::get_shader_asset(const char* shader_file_name) {
    if(!shader_library.contains(shader_file_name)) {
      spdlog::error("Could not find shader asset: {}", shader_file_name);
      throw std::exception("Could not found shader asset");
    }
    return shader_entries[shader_library[shader_file_name]];
  }
}
