#pragma once

#include <coreinit/debug.h>
#include <string>
#include <vector>

class ConsoleTable {
public:
    enum Alignment { LEFT,
                     RIGHT };

    void AddColumn(const std::string &title, Alignment align = LEFT, size_t maxWidth = 0);

    void AddRow(const std::vector<std::string> &rowData);

    void AddFooter(const std::vector<std::string> &footerData);

    void Print() const;

private:
    struct Column {
        std::string title;
        Alignment align;
        size_t width;
        size_t maxWidth;
    };

    std::vector<Column> mCols;
    std::vector<std::vector<std::string>> mRows;
    std::vector<std::string> mFooter;

    void updateWidths(const std::vector<std::string> &data);

    void PrintSeparator() const;

    static void printCell(const std::string &text, size_t width, Alignment align);

    void PrintRowWrapped(const std::vector<std::string> &row) const;

    static std::vector<std::string> WrapText(std::string text, size_t maxWidth);
};