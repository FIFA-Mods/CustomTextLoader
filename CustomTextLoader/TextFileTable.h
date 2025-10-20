#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

enum eEncoding {
    ENCODING_ANSI,
    ENCODING_UTF8,
    ENCODING_UTF8_BOM,
    ENCODING_UTF16LE_BOM,
    ENCODING_UTF16BE_BOM
};

class TextFileTable {
    std::vector<std::vector<std::string>> mCells;

    static std::string Unquoted(std::string const &str);
    static std::string Quoted(std::string const &str, char separator);
    size_t NumRowsToWrite() const;
    template <typename T>
    static std::string to_string(T const &value) {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
            return value;
        else if constexpr (std::is_arithmetic_v<std::decay_t<T>> || std::is_integral_v<std::decay_t<T>>)
            return std::to_wstring(value);
        else {
            std::stringstream wss;
            wss << value;
            return wss.str();
        }
    }
public:
    size_t NumRows() const;
    size_t NumColumns(size_t row) const;
    bool IsConsistent() const;
    size_t MaxColumns() const;
    std::vector<std::vector<std::string>> const &Rows() const;
    std::vector<std::string> const &Row(size_t row) const;
    std::string const &Cell(size_t column, size_t row) const;
    void AddRow(std::vector<std::string> const &row);
    template <typename... ArgTypes>
    void AddRow(ArgTypes... args) {
        std::vector<std::string> vec;
        (vec.push_back(to_string(args)), ...);
        mCells.push_back(vec);
    }
    void Clear();
    bool Read(std::filesystem::path const &filename, char separator = L',');
    bool ReadCSV(std::filesystem::path const &filename, char separator = L',');
    bool ReadTSV(std::filesystem::path const &filename);
    bool ReadUnicodeText(std::filesystem::path const &filename);
    bool Write(std::filesystem::path const &filename, char separator = L',', eEncoding encoding = ENCODING_UTF8_BOM);
    bool WriteCSV(std::filesystem::path const &filename, char separator = L',');
    bool WriteTSV(std::filesystem::path const &filename);
    bool WriteUnicodeText(std::filesystem::path const &filename);
};
