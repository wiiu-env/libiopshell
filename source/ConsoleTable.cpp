#include "ConsoleTable.h"

void ConsoleTable::AddColumn(const std::string &title, const Alignment align, size_t maxWidth) {
    mCols.push_back({title, align, title.length(), maxWidth});
}

void ConsoleTable::AddRow(const std::vector<std::string> &rowData) {
    if (rowData.size() != mCols.size()) return;
    mRows.push_back(rowData);
    updateWidths(rowData);
}

void ConsoleTable::AddFooter(const std::vector<std::string> &footerData) {
    if (footerData.size() != mCols.size()) return;
    mFooter = footerData;
    updateWidths(footerData);
}

void ConsoleTable::Print() const {
    PrintSeparator();

    // Print Header
    OSReport("|");
    for (const auto &col : mCols) printCell(col.title, col.width, col.align);
    OSReport("\n");

    PrintSeparator();

    size_t totalWidth = 1;
    for (const auto &col : mCols) {
        totalWidth += col.width + 3;
    }
    std::string border(totalWidth, '-');

    // Print Rows (Multi-line support)
    for (const auto &row : mRows) {
        PrintRowWrapped(row);
        OSReport("%s\n", border.c_str());
    }

    // Print Footer
    if (!mFooter.empty()) {
        PrintSeparator();
        PrintRowWrapped(mFooter);
    }
}

void ConsoleTable::updateWidths(const std::vector<std::string> &data) {
    for (size_t i = 0; i < mCols.size(); ++i) {
        size_t len = data[i].length();
        // Cap the column width if a maxWidth is set
        if (mCols[i].maxWidth > 0 && len > mCols[i].maxWidth) {
            len = mCols[i].maxWidth;
        }
        if (len > mCols[i].width) {
            mCols[i].width = len;
        }
    }
}

void ConsoleTable::PrintSeparator() const {
    OSReport("|");
    for (const auto &col : mCols) {
        std::string line(col.width + 2, '-');
        OSReport("%s|", line.c_str());
    }
    OSReport("\n");
}

void ConsoleTable::printCell(const std::string &text, const size_t width, const Alignment align) {
    if (align == LEFT) OSReport(" %-*s |", static_cast<int>(width), text.c_str());
    else
        OSReport(" %*s |", static_cast<int>(width), text.c_str());
}

void ConsoleTable::PrintRowWrapped(const std::vector<std::string> &row) const {
    std::vector<std::vector<std::string>> wrappedCells(row.size());
    size_t maxLines = 1;

    // Step 1: Wrap text for each cell in the row
    for (size_t i = 0; i < row.size(); ++i) {
        wrappedCells[i] = WrapText(row[i], mCols[i].width);
        if (wrappedCells[i].size() > maxLines) {
            maxLines = wrappedCells[i].size();
        }
    }

    // Step 2: Print the row line-by-line
    for (size_t lineIdx = 0; lineIdx < maxLines; ++lineIdx) {
        OSReport("|");
        for (size_t colIdx = 0; colIdx < row.size(); ++colIdx) {
            std::string cellText = "";
            if (lineIdx < wrappedCells[colIdx].size()) {
                cellText = wrappedCells[colIdx][lineIdx];
            }
            printCell(cellText, mCols[colIdx].width, mCols[colIdx].align);
        }
        OSReport("\n");
    }
}

std::vector<std::string> ConsoleTable::WrapText(std::string text, size_t maxWidth) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.emplace_back("");
        return lines;
    }

    // If maxWidth is 0 (unlimited), just return the raw string
    if (maxWidth == 0) {
        lines.push_back(text);
        return lines;
    }

    // Wrap text by spaces
    while (text.length() > maxWidth) {
        size_t spaceIndex = text.rfind(' ', maxWidth);
        if (spaceIndex == std::string::npos || spaceIndex == 0) {
            // No space found within limit, force break the word
            lines.push_back(text.substr(0, maxWidth));
            text = text.substr(maxWidth);
        } else {
            // Break at the last found space
            lines.push_back(text.substr(0, spaceIndex));
            text = text.substr(spaceIndex + 1);
        }
    }
    if (!text.empty()) {
        lines.push_back(text);
    }
    return lines;
}
