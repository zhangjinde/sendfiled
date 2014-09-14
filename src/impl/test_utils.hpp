#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <cstdio>
#include <stdexcept>
#include <string>

#include "../attributes.h"

namespace test {

/**
 * A RAII temporary file.
 */
class DSO_EXPORT TmpFile {
    TmpFile(const TmpFile&) = delete;
    TmpFile(TmpFile&&) = delete;
    TmpFile& operator=(const TmpFile&) = delete;
    TmpFile& operator=(TmpFile&&) = delete;
public:
    TmpFile();

    explicit TmpFile(const std::string& contents);

    ~TmpFile();

    void close();

    const std::string& name() const noexcept;

    operator int();

    operator FILE*();
private:
    std::string name_;
    FILE* fp_;
};

} // namespace test

#endif
