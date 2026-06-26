#include "json/reader.h"
#include <sstream>
#include <cctype>
#include <cstring>
#include <cassert>

namespace Json {

Reader::Reader() : begin_(nullptr), end_(nullptr), current_(nullptr), line_(1), column_(1), collectComments_(false) {
}

Reader::~Reader() {
}

bool Reader::parse(const std::string& document, Value& root) {
    return parse(document.c_str(), document.c_str() + document.length(), root);
}

bool Reader::parse(const char* beginDoc, const char* endDoc, Value& root) {
    begin_ = beginDoc;
    end_ = endDoc;
    current_ = begin_;
    line_ = 1;
    column_ = 1;
    errors_.clear();

    bool successful = parseValue(root);
    
    if (successful) {
        Token token;
        if (!skipCommentTokens(token)) {
            successful = false;
        } else if (token.type_ != Token::tokenEndOfStream) {
            addError("Extra non-whitespace after JSON value.");
            successful = false;
        }
    }
    
    return successful;
}

bool Reader::parse(std::istream& is, Value& root) {
    std::string document;
    std::getline(is, document, '\0');
    return parse(document, root);
}

std::string Reader::getFormattedErrorMessages() const {
    return errors_;
}

bool Reader::readToken(Token& token) {
    skipSpaces();
    
    if (current_ >= end_) {
        token.type_ = Token::tokenEndOfStream;
        return true;
    }
    
    char c = getNextChar();
    token.line_ = line_;
    token.column_ = column_;
    
    switch (c) {
    case '{':
        token.type_ = Token::tokenObjectBegin;
        break;
    case '}':
        token.type_ = Token::tokenObjectEnd;
        break;
    case '[':
        token.type_ = Token::tokenArrayBegin;
        break;
    case ']':
        token.type_ = Token::tokenArrayEnd;
        break;
    case '"':
        return parseString(token.value_) && (token.type_ = Token::tokenString, true);
    case ',':
        token.type_ = Token::tokenArraySeparator;
        break;
    case ':':
        token.type_ = Token::tokenMemberSeparator;
        break;
    case 't':
        if (match("rue", 3)) {
            token.type_ = Token::tokenTrue;
        } else {
            token.type_ = Token::tokenError;
            addError("Invalid literal name");
            return false;
        }
        break;
    case 'f':
        if (match("alse", 4)) {
            token.type_ = Token::tokenFalse;
        } else {
            token.type_ = Token::tokenError;
            addError("Invalid literal name");
            return false;
        }
        break;
    case 'n':
        if (match("ull", 3)) {
            token.type_ = Token::tokenNull;
        } else {
            token.type_ = Token::tokenError;
            addError("Invalid literal name");
            return false;
        }
        break;
    case '/':
        // Comment handling would go here
        token.type_ = Token::tokenComment;
        break;
    default:
        if (c == '-' || (c >= '0' && c <= '9')) {
            ungetChar(c);
            return parseNumber(token) && (token.type_ = Token::tokenNumber, true);
        } else {
            token.type_ = Token::tokenError;
            addError("Unexpected character");
            return false;
        }
    }
    
    return true;
}

bool Reader::parseValue(Value& value) {
    Token token;
    if (!readToken(token)) {
        return false;
    }
    
    switch (token.type_) {
    case Token::tokenObjectBegin:
        return parseObject(value);
    case Token::tokenArrayBegin:
        return parseArray(value);
    case Token::tokenString:
        value = Value(token.value_);
        return true;
    case Token::tokenNumber:
        // Simple number parsing - could be improved
        if (token.value_.find('.') != std::string::npos) {
            value = Value(std::stod(token.value_));
        } else {
            value = Value(std::stoi(token.value_));
        }
        return true;
    case Token::tokenTrue:
        value = Value(true);
        return true;
    case Token::tokenFalse:
        value = Value(false);
        return true;
    case Token::tokenNull:
        value = Value();
        return true;
    default:
        addError("Syntax error: value expected");
        return false;
    }
}

bool Reader::parseObject(Value& value) {
    value = Value(objectValue);
    Token token;
    
    if (!readToken(token)) return false;
    
    if (token.type_ == Token::tokenObjectEnd) {
        return true; // Empty object
    }
    
    while (true) {
        if (token.type_ != Token::tokenString) {
            addError("Expected string key");
            return false;
        }
        
        std::string key = token.value_;
        
        if (!readToken(token) || token.type_ != Token::tokenMemberSeparator) {
            addError("Expected ':' after key");
            return false;
        }
        
        Value memberValue;
        if (!parseValue(memberValue)) {
            return false;
        }
        
        value[key] = memberValue;
        
        if (!readToken(token)) return false;
        
        if (token.type_ == Token::tokenObjectEnd) {
            return true;
        }
        
        if (token.type_ != Token::tokenArraySeparator) {
            addError("Expected ',' or '}'");
            return false;
        }
        
        if (!readToken(token)) return false;
    }
}

bool Reader::parseArray(Value& value) {
    value = Value(arrayValue);
    Token token;

    skipSpaces();
    if (current_ < end_ && *current_ == ']') {
        getNextChar();
        return true; // Empty array
    }
    
    while (true) {
        Value arrayValue;
        if (!parseValue(arrayValue)) {
            return false;
        }
        
        value.append(arrayValue);
        
        if (!readToken(token)) return false;
        
        if (token.type_ == Token::tokenArrayEnd) {
            return true;
        }
        
        if (token.type_ != Token::tokenArraySeparator) {
            addError("Expected ',' or ']'");
            return false;
        }
        
        if (!readToken(token)) return false;
    }
}

bool Reader::parseString(std::string& value) {
    value.clear();
    
    while (current_ < end_) {
        char c = getNextChar();
        
        if (c == '"') {
            return true;
        }
        
        if (c == '\\') {
            if (current_ >= end_) {
                addError("Unterminated string");
                return false;
            }
            
            char escaped = getNextChar();
            switch (escaped) {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            default:
                addError("Invalid escape sequence");
                return false;
            }
        } else {
            value += c;
        }
    }
    
    addError("Unterminated string");
    return false;
}

bool Reader::parseNumber(Token& token) {
    std::string number;

    while (current_ < end_) {
        char c = *current_;
        if (c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E' || std::isdigit(c)) {
            number += getNextChar();
        } else {
            break;
        }
    }

    token.value_ = number;
    return true;
}

bool Reader::skipCommentTokens(Token& token) {
    do {
        if (!readToken(token)) {
            return false;
        }
    } while (token.type_ == Token::tokenComment);
    
    return true;
}

void Reader::skipSpaces() {
    while (current_ < end_) {
        char c = *current_;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                line_++;
                column_ = 1;
            } else {
                column_++;
            }
            current_++;
        } else {
            break;
        }
    }
}

char Reader::getNextChar() {
    if (current_ >= end_) return 0;
    
    char c = *current_++;
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

void Reader::ungetChar(char c) {
    if (current_ > begin_) {
        current_--;
        if (c == '\n') {
            line_--;
            // Column calculation would be complex here
        } else {
            column_--;
        }
    }
}

bool Reader::match(const char* pattern, int patternLength) {
    if (current_ + patternLength > end_) {
        return false;
    }
    
    if (std::strncmp(current_, pattern, patternLength) == 0) {
        current_ += patternLength;
        column_ += patternLength;
        return true;
    }
    
    return false;
}

void Reader::addError(const std::string& message) {
    std::ostringstream oss;
    oss << "Line " << line_ << ", Column " << column_ << ": " << message << "\n";
    errors_ += oss.str();
}

} // namespace Json
