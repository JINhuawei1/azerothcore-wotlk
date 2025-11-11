#ifndef JSON_WRITER_H_INCLUDED
#define JSON_WRITER_H_INCLUDED

#include "value.h"
#include <string>
#include <ostream>

namespace Json {

class Writer {
public:
    virtual ~Writer();
    virtual std::string write(const Value& root) = 0;
};

class FastWriter : public Writer {
public:
    FastWriter();
    virtual ~FastWriter();

    void enableYAMLCompatibility();
    void dropNullPlaceholders();
    void omitEndingLineFeed();

    virtual std::string write(const Value& root);

private:
    void writeValue(const Value& value, std::string& document);
    std::string valueToString(const Value& value);
    std::string valueToQuotedString(const char* value);

    bool yamlCompatibilityEnabled_;
    bool dropNullPlaceholders_;
    bool omitEndingLineFeed_;
};

class StyledWriter : public Writer {
public:
    StyledWriter();
    virtual ~StyledWriter();

    virtual std::string write(const Value& root);

private:
    void writeValue(const Value& value);
    void writeArrayValue(const Value& value);
    bool isMultineArray(const Value& value);
    void pushValue(const std::string& value);
    void writeIndent();
    void writeWithIndent(const std::string& value);
    void indent();
    void unindent();
    void writeCommentBeforeValue(const Value& root);
    void writeCommentAfterValueOnSameLine(const Value& root);
    bool hasCommentForValue(const Value& value);
    static std::string normalizeEOL(const std::string& text);

    typedef std::vector<std::string> ChildValues;

    ChildValues childValues_;
    std::string document_;
    std::string indentString_;
    int rightMargin_;
    int indentSize_;
    bool addChildValues_;
};

std::string valueToString(int value);
std::string valueToString(unsigned int value);
std::string valueToString(long long value);
std::string valueToString(unsigned long long value);
std::string valueToString(double value);
std::string valueToString(bool value);
std::string valueToQuotedString(const char* value);

std::ostream& operator<<(std::ostream&, const Value& root);

} // namespace Json

#endif // JSON_WRITER_H_INCLUDED
