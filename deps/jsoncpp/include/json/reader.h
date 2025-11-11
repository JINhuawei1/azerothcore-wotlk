#ifndef JSON_READER_H_INCLUDED
#define JSON_READER_H_INCLUDED

#include "value.h"
#include <string>
#include <istream>

namespace Json {

class Reader {
public:
    Reader();
    ~Reader();

    bool parse(const std::string& document, Value& root);
    bool parse(const char* beginDoc, const char* endDoc, Value& root);
    bool parse(std::istream& is, Value& root);

    std::string getFormattedErrorMessages() const;

private:
    struct Token {
        enum Type {
            tokenEndOfStream = 0,
            tokenObjectBegin,
            tokenObjectEnd,
            tokenArrayBegin,
            tokenArrayEnd,
            tokenString,
            tokenNumber,
            tokenTrue,
            tokenFalse,
            tokenNull,
            tokenArraySeparator,
            tokenMemberSeparator,
            tokenComment,
            tokenError
        };

        Type type_;
        std::string value_;
        int line_;
        int column_;
    };

    bool readToken(Token& token);
    bool parseValue(Value& value);
    bool parseObject(Value& value);
    bool parseArray(Value& value);
    bool parseString(std::string& value);
    bool parseNumber(Token& token);
    bool skipCommentTokens(Token& token);
    void skipSpaces();
    char getNextChar();
    void ungetChar(char c);
    bool match(const char* pattern, int patternLength);
    void addError(const std::string& message);

    const char* begin_;
    const char* end_;
    const char* current_;
    int line_;
    int column_;
    std::string errors_;
    bool collectComments_;
};

} // namespace Json

#endif // JSON_READER_H_INCLUDED
