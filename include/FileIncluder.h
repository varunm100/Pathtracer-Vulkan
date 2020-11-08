#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <vulkan/shaderc.hpp>

// Helper classes taken from glslc: https://github.com/google/shaderc
class FileFinder {
 public:
  std::string FindReadableFilepath( const std::string &filename ) const;
  std::string FindRelativeReadableFilepath( const std::string &requesting_file, const std::string &filename ) const;
  std::vector<std::string> &search_path() { return search_path_; }

 private:
  std::vector<std::string> search_path_;
};
class FileIncluder : public shaderc::CompileOptions::IncluderInterface
{
 public:
  explicit FileIncluder( const FileFinder *file_finder ) : file_finder_( *file_finder ) {}

  ~FileIncluder() override;
  shaderc_include_result *GetInclude( const char *requested_source,
				      shaderc_include_type type,
				      const char *requesting_source,
				      size_t include_depth ) override;
  void ReleaseInclude( shaderc_include_result *include_result ) override;
  const std::unordered_set<std::string> &file_path_trace() const { return included_files_; }

 private:
  const FileFinder &file_finder_;
  struct FileInfo
  {
    const std::string full_path;
    std::vector<char> contents;
  };
  std::unordered_set<std::string> included_files_;
};
