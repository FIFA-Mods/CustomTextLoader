#include "TextFileTable.h"
#include <fcntl.h>
#include <io.h>
#include "plugin.h"

std::string TextFileTable::Unquoted(std::string const &str) {
    if (str.size() > 1 && str[0] == '"' && str[str.size() - 1] == '"') {
        std::string unquoted = str.substr(1, str.size() - 2);
        std::string result;
        for (size_t i = 0; i < unquoted.size(); i++) {
            if (unquoted[i] == '"' && (i + 1) < unquoted.size() && unquoted[i + 1] == '"')
                i++;
            result += unquoted[i];
        }
        return result;
    }
    return str;
}

std::string TextFileTable::Quoted(std::string const &str, char separator) {
    bool addQuotes = false;
    for (char c : str) {
        if (c == '\r' || c == '\n' || c == '"' || c == separator) {
            addQuotes = true;
            break;
        }
    }
    if (!addQuotes)
        return str;
    std::string result;
    result += '"';
    for (char c : str) {
        result += c;
        if (c == '"')
            result += c;
    }
    result += '"';
    return result;
}

size_t TextFileTable::NumRowsToWrite() const {
    size_t result = 0;
    for (size_t r = 0; r < mCells.size(); r++) {
        bool validRow = false;
        if (!mCells[r].empty()) {
            for (size_t c = 0; c < mCells[r].size(); c++) {
                if (!mCells[r][c].empty()) {
                    validRow = true;
                    break;
                }
            }
        }
        if (validRow)
            result = r + 1;
    }
    return result;
}

size_t TextFileTable::NumRows() const {
    return mCells.size();
}

size_t TextFileTable::NumColumns(size_t row) const {
    if (row < mCells.size())
        return mCells[row].size();
    return 0;
}

bool TextFileTable::IsConsistent() const {
    if (mCells.size() > 1) {
        for (size_t i = 1; i < mCells.size(); i++) {
            if (mCells[i - 1].size() != mCells[i].size())
                return false;
        }
    }
    return true;
}

size_t TextFileTable::MaxColumns() const {
    size_t maxColumns = 0;
    for (auto const &row : mCells) {
        if (row.size() > maxColumns)
            maxColumns = row.size();
    }
    return maxColumns;
}

std::vector<std::vector<std::string>> const &TextFileTable::Rows() const {
    return mCells;
}

std::vector<std::string> const &TextFileTable::Row(size_t row) const {
    static std::vector<std::string> emptyRow;
    if (row < mCells.size())
        return mCells[row];
    return emptyRow;
}

std::string const &TextFileTable::Cell(size_t column, size_t row) const {
    static std::string emptyCell;
    if (row < mCells.size() && column < mCells[row].size())
        return mCells[row][column];
    return emptyCell;
}

void TextFileTable::AddRow(std::vector<std::string> const &row) {
    mCells.push_back(row);
}

void TextFileTable::Clear() {
    mCells.clear();
}

bool TextFileTable::Read(std::wstring const &filename, char separator) {
    Clear();
    FILE *file = _wfopen(filename.c_str(), L"rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        auto fileSizeWithBom = ftell(file);
        if (fileSizeWithBom == 0) {
            fclose(file);
            return false;
        }
        fseek(file, 0, SEEK_SET);
        eEncoding enc = ENCODING_UTF8;
        long numBytesToSkip = 0;
        if (fileSizeWithBom >= 2) {
            unsigned char bom[3];
            bom[0] = 0;
            fread(&bom, 1, 2, file);
            fseek(file, 0, SEEK_SET);
            if (bom[0] == 0xFE && bom[1] == 0xFF) {
                enc = ENCODING_UTF16BE_BOM;
                numBytesToSkip = 2;
            }
            else if (bom[0] == 0xFF && bom[1] == 0xFE) {
                enc = ENCODING_UTF16LE_BOM;
                numBytesToSkip = 2;
            }
            else if (fileSizeWithBom >= 3) {
                bom[0] = 0;
                fread(&bom, 1, 3, file);
                fseek(file, 0, SEEK_SET);
                if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
                    enc = ENCODING_UTF8_BOM;
                    numBytesToSkip = 3;
                }
            }
        }
        long totalSize = fileSizeWithBom - numBytesToSkip;
        char *fileData = new char[totalSize];
        fseek(file, numBytesToSkip, SEEK_SET);
        auto numRead = fread(fileData, 1, totalSize, file);
        fclose(file);
        if (numRead != totalSize) {
            delete[] fileData;
            return false;
        }
        long numChars = totalSize;
        if (enc == ENCODING_UTF16LE_BOM || enc == ENCODING_UTF16BE_BOM)
            numChars = WideCharToMultiByte(CP_UTF8, 0, (wchar_t const *)fileData, totalSize, NULL, 0, NULL, NULL);
        if (numChars == 0) {
            delete[] fileData;
            return true;
        }
        char *data = new char[numChars];
        memset(data, 0, numChars * sizeof(char));
        if (enc == ENCODING_UTF16LE_BOM || enc == ENCODING_UTF16BE_BOM) {
            if (enc == ENCODING_UTF16BE_BOM) {
                auto numWideChars = totalSize / 2;
                wchar_t* wideData = (wchar_t *)fileData;
                for (long i = 0; i < numWideChars; i++)
                    wideData[i] = (wideData[i] >> 8) | (wideData[i] << 8);
            }
            WideCharToMultiByte(CP_UTF8, 0, (wchar_t const*)fileData, totalSize, data, numChars, NULL, NULL);
        }
        else
            memcpy(data, fileData, totalSize);
        delete[] fileData;

        std::vector<std::string> mLines;
        std::string currentLine;
        unsigned int numQuotes = 0;
        for (long i = 0; i < numChars; i++) {
            if (data[i] == '\n') {
                if (numQuotes % 2)
                    currentLine += data[i];
                else {
                    mLines.push_back(currentLine);
                    currentLine.clear();
                }
            }
            else if (data[i] == '\r') {
                if (numQuotes % 2)
                    currentLine += data[i];
                if ((i + 1) < numChars && data[i + 1] == '\n') {
                    i++;
                    if (numQuotes % 2)
                        currentLine += data[i];
                }
                if ((numQuotes % 2) == 0) {
                    mLines.push_back(currentLine);
                    currentLine.clear();
                }
            }
            else {
                if (data[i] == '"')
                    numQuotes++;
                currentLine += data[i];
            }
        }
        if (!currentLine.empty() || !mLines.empty())
            mLines.push_back(currentLine);
        delete[] data;

        mCells.resize(mLines.size());

        for (size_t l = 0; l < mLines.size(); l++) {
            numQuotes = 0;
            auto const &line = mLines[l];
            std::string value;
            for (size_t i = 0; i < line.size(); i++) {
                if (line[i] == '"')
                    numQuotes++;
                else if (line[i] == separator && ((numQuotes % 2) == 0)) {
                    mCells[l].push_back(Unquoted(value));
                    value.clear();
                    continue;
                }
                value += line[i];
            }
            mCells[l].push_back(Unquoted(value));
            value.clear();
        }

        mCells.resize(NumRowsToWrite());
    }
    return true;
}

bool TextFileTable::ReadCSV(std::wstring const &filename, char separator) {
    return Read(filename, separator);
}

bool TextFileTable::ReadTSV(std::wstring const &filename) {
    return Read(filename, L'\t');
}

bool TextFileTable::ReadUnicodeText(std::wstring const &filename) {
    return Read(filename, L'\t');
}

bool TextFileTable::Write(std::wstring const &filename, char separator, eEncoding encoding) {
    // not implemented
    return false;
}

bool TextFileTable::WriteCSV(std::wstring const &filename, char separator) {
    return Write(filename, separator, ENCODING_UTF8_BOM);
}

bool TextFileTable::WriteTSV(std::wstring const &filename) {
    return Write(filename, L'\t', ENCODING_UTF8_BOM);
}

bool TextFileTable::WriteUnicodeText(std::wstring const &filename) {
    return Write(filename, L'\t', ENCODING_UTF16LE_BOM);
}
