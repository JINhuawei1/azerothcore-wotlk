#include "json/writer.h"
#include <sstream>
#include <iomanip>

namespace Json {

Writer::~Writer() {
}

// FastWriter implementation
FastWriter::FastWriter() 
    : yamlCompatibilityEnabled_(false)
    , dropNullPlaceholders_(false)
    , omitEndingLineFeed_(false) {
}

FastWriter::~FastWriter() {
}

void FastWriter::enableYAMLCompatibility() {
    yamlCompatibilityEnabled_ = true;
}

void FastWriter::dropNullPlaceholders() {
    dropNullPlaceholders_ = true;
}

void FastWriter::omitEndingLineFeed() {
    omitEndingLineFeed_ = true;
}

std::string FastWriter::write(const Value& root) {
    std::string document;
    writeValue(root, document);
    if (!omitEndingLineFeed_) {
        document += "\n";
    }
    return document;
}

void FastWriter::writeValue(const Value& value, std::string& document) {
    switch (value.type()) {
    case nullValue:
        document += "null";
        break;
    case intValue:
        document += valueToString(value.asInt());
        break;
    case uintValue:
        document += valueToString(value.asUInt());
        break;
    case int64Value:
        document += valueToString(static_cast<long long>(value.asInt64()));
        break;
    case uint64Value:
        document += valueToString(static_cast<unsigned long long>(value.asUInt64()));
        break;
    case realValue:
        document += valueToString(value.asDouble());
        break;
    case stringValue:
        document += valueToQuotedString(value.asString().c_str());
        break;
    case booleanValue:
        document += valueToString(value.asBool());
        break;
    case arrayValue:
        document += "[";
        for (unsigned int index = 0; index < value.size(); ++index) {
            if (index > 0) {
                document += yamlCompatibilityEnabled_ ? ", " : ",";
            }
            writeValue(value[static_cast<int>(index)], document);
        }
        document += "]";
        break;
    case objectValue:
        {
            std::vector<std::string> members = value.getMemberNames();
            document += "{";
            for (std::vector<std::string>::iterator it = members.begin(); it != members.end(); ++it) {
                const std::string& name = *it;
                if (it != members.begin()) {
                    document += yamlCompatibilityEnabled_ ? ", " : ",";
                }
                document += valueToQuotedString(name.c_str());
                document += yamlCompatibilityEnabled_ ? ": " : ":";
                writeValue(value[name], document);
            }
            document += "}";
        }
        break;
    }
}

std::string FastWriter::valueToString(const Value& value) {
    switch (value.type()) {
    case nullValue: return "null";
    case intValue: return valueToString(value.asInt());
    case uintValue: return valueToString(value.asUInt());
    case realValue: return valueToString(value.asDouble());
    case stringValue: return valueToQuotedString(value.asString().c_str());
    case booleanValue: return valueToString(value.asBool());
    default: return "";
    }
}

std::string FastWriter::valueToQuotedString(const char* value) {
    std::string result = "\"";
    
    while (*value) {
        char c = *value++;
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c < ' ') {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                result += oss.str();
            } else {
                result += c;
            }
            break;
        }
    }
    
    result += "\"";
    return result;
}

// StyledWriter implementation
StyledWriter::StyledWriter() 
    : rightMargin_(74)
    , indentSize_(3)
    , addChildValues_(false) {
}

StyledWriter::~StyledWriter() {
}

std::string StyledWriter::write(const Value& root) {
    document_.clear();
    addChildValues_ = false;
    indentString_.clear();
    writeCommentBeforeValue(root);
    writeValue(root);
    writeCommentAfterValueOnSameLine(root);
    document_ += "\n";
    return document_;
}

void StyledWriter::writeValue(const Value& value) {
    switch (value.type()) {
    case nullValue:
        pushValue("null");
        break;
    case intValue:
        pushValue(valueToString(value.asInt()));
        break;
    case uintValue:
        pushValue(valueToString(value.asUInt()));
        break;
    case realValue:
        pushValue(valueToString(value.asDouble()));
        break;
    case stringValue:
        pushValue(valueToQuotedString(value.asString().c_str()));
        break;
    case booleanValue:
        pushValue(valueToString(value.asBool()));
        break;
    case arrayValue:
        writeArrayValue(value);
        break;
    case objectValue:
        {
            std::vector<std::string> members = value.getMemberNames();
            if (members.empty()) {
                pushValue("{}");
            } else {
                writeWithIndent("{");
                indent();
                for (std::vector<std::string>::iterator it = members.begin(); it != members.end(); ++it) {
                    const std::string& name = *it;
                    const Value& childValue = value[name];
                    writeCommentBeforeValue(childValue);
                    writeWithIndent(valueToQuotedString(name.c_str()));
                    document_ += " : ";
                    writeValue(childValue);
                    if (it != members.end() - 1) {
                        document_ += ",";
                    }
                    writeCommentAfterValueOnSameLine(childValue);
                }
                unindent();
                writeWithIndent("}");
            }
        }
        break;
    }
}

void StyledWriter::writeArrayValue(const Value& value) {
    unsigned int size = value.size();
    if (size == 0) {
        pushValue("[]");
    } else {
        bool isMultiLine = isMultineArray(value);
        if (isMultiLine) {
            writeWithIndent("[");
            indent();
            bool hasChildValue = !childValues_.empty();
            unsigned int index = 0;
            while (index < size) {
                const Value& childValue = value[static_cast<int>(index)];
                writeCommentBeforeValue(childValue);
                if (hasChildValue) {
                    writeWithIndent(childValues_[index]);
                } else {
                    writeIndent();
                    writeValue(childValue);
                }
                if (index < size - 1) {
                    document_ += ",";
                }
                writeCommentAfterValueOnSameLine(childValue);
                ++index;
            }
            unindent();
            writeWithIndent("]");
        } else {
            document_ += "[ ";
            for (unsigned int index = 0; index < size; ++index) {
                if (index > 0) {
                    document_ += ", ";
                }
                writeValue(value[static_cast<int>(index)]);
            }
            document_ += " ]";
        }
    }
}

bool StyledWriter::isMultineArray(const Value& value) {
    int size = static_cast<int>(value.size());
    bool isMultiLine = size * 3 >= rightMargin_;
    return isMultiLine;
}

void StyledWriter::pushValue(const std::string& value) {
    if (addChildValues_) {
        childValues_.push_back(value);
    } else {
        document_ += value;
    }
}

void StyledWriter::writeIndent() {
    if (!document_.empty()) {
        char last = document_[document_.length() - 1];
        if (last == ' ') {
            return;
        }
        if (last != '\n') {
            document_ += '\n';
        }
    }
    document_ += indentString_;
}

void StyledWriter::writeWithIndent(const std::string& value) {
    writeIndent();
    document_ += value;
}

void StyledWriter::indent() {
    indentString_ += std::string(indentSize_, ' ');
}

void StyledWriter::unindent() {
    if (indentString_.size() >= static_cast<size_t>(indentSize_)) {
        indentString_.resize(indentString_.size() - indentSize_);
    }
}

void StyledWriter::writeCommentBeforeValue(const Value& root) {
    // Comment handling would go here
}

void StyledWriter::writeCommentAfterValueOnSameLine(const Value& root) {
    // Comment handling would go here
}

bool StyledWriter::hasCommentForValue(const Value& value) {
    return false;
}

std::string StyledWriter::normalizeEOL(const std::string& text) {
    std::string normalized;
    normalized.reserve(text.length());
    
    for (std::string::const_iterator it = text.begin(); it != text.end(); ++it) {
        char c = *it;
        if (c == '\r') {
            if (it + 1 != text.end() && *(it + 1) == '\n') {
                normalized += '\n';
                ++it;
            } else {
                normalized += '\n';
            }
        } else {
            normalized += c;
        }
    }
    
    return normalized;
}

// Utility functions
std::string valueToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string valueToString(unsigned int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string valueToString(long long value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string valueToString(unsigned long long value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string valueToString(double value) {
    std::ostringstream oss;
    oss << std::setprecision(17) << value;
    return oss.str();
}

std::string valueToString(bool value) {
    return value ? "true" : "false";
}

std::string valueToQuotedString(const char* value) {
    std::string result = "\"";

    while (*value) {
        char c = *value++;
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c < ' ') {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                result += oss.str();
            } else {
                result += c;
            }
            break;
        }
    }

    result += "\"";
    return result;
}

std::ostream& operator<<(std::ostream& sout, const Value& root) {
    FastWriter writer;
    sout << writer.write(root);
    return sout;
}

} // namespace Json
