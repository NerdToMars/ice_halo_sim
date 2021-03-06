#include "files.h"

#include <cstdio>
#include <algorithm>


namespace IceHalo {

bool exists(const char* filename) {
  boost::filesystem::path p(filename);
  return exists(p);
}


std::vector<File> ListDataFiles(const char* dir) {
  namespace f = boost::filesystem;

  std::vector<File> files;
  f::path p(dir);
  std::vector<f::path> paths;
  copy(f::directory_iterator(p), f::directory_iterator(), back_inserter(paths));
  for (auto& x : paths) {
    if (x.extension() == ".bin") {
      files.emplace_back(x.c_str());
    }
  }

  return files;
}


std::string PathJoin(const std::string& p1, const std::string& p2) {
  boost::filesystem::path p(p1);
  p /= (p2);
  return p.string();
}


File::File(const char* filename)
    : file_(nullptr), file_opened_(false),
      path_(filename) {}


File::File(const char* path, const char* filename)
    : file_(nullptr), file_opened_(false),
      path_(path) {
  path_ /= filename;
}


File::~File() {
  if (file_opened_) {
    fclose(file_);
    file_opened_ = false;
  }
}


bool File::Open(uint8_t mode) {
  if (!exists(path_.parent_path())) {
    create_directories(path_.parent_path());
  }

  char modeBuffer[32];
  const char* m1;
  if (mode & OpenMode::kRead) {
    m1 = "r";
  } else if (mode & OpenMode::kWrite) {
    m1 = "w";
  } else if (mode & OpenMode::kAppend) {
    m1 = "a";
  } else {
    m1 = "r";
  }
  const char* m2 = (mode & OpenMode::kBinary) ? "b" : "";
  std::sprintf(modeBuffer, "%s%s", m1, m2);

  file_ = std::fopen(path_.c_str(), modeBuffer);
  file_opened_ = file_ != nullptr;
  return file_opened_;
}


bool File::Close() {
  if (file_opened_) {
    std::fclose(file_);
    file_ = nullptr;
    file_opened_ = false;
  }
  return true;
}


size_t File::GetSize() {
  auto size = file_size(path_);
  if (size == static_cast<uintmax_t>(-1)) {
    return 0;
  } else {
    return static_cast<size_t>(size);
  }
}

}  // namespace IceHalo
