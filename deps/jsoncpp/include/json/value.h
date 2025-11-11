#ifndef JSON_VALUE_H_INCLUDED
#define JSON_VALUE_H_INCLUDED

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Json {

enum ValueType {
    nullValue = 0,
    intValue,
    uintValue,
    int64Value,
    uint64Value,
    realValue,
    stringValue,
    booleanValue,
    arrayValue,
    objectValue
};

class Value {
public:
    typedef std::vector<Value> Array;
    typedef std::map<std::string, Value> Object;
    typedef Array::iterator iterator;
    typedef Array::const_iterator const_iterator;
    typedef unsigned int ArrayIndex;

    // Constructors
    Value(ValueType type = nullValue);
    Value(int value);
    Value(unsigned int value);
    Value(long long value);
    Value(unsigned long long value);
    Value(double value);
    Value(const char* value);
    Value(const std::string& value);
    Value(bool value);
    Value(const Value& other);

    // Destructor
    ~Value();

    // Assignment
    Value& operator=(const Value& other);

    // Type checking
    ValueType type() const;
    bool isNull() const;
    bool isBool() const;
    bool isInt() const;
    bool isUInt() const;
    bool isIntegral() const;
    bool isDouble() const;
    bool isNumeric() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    // Value access
    int asInt() const;
    unsigned int asUInt() const;
    long long asInt64() const;
    unsigned long long asUInt64() const;
    double asDouble() const;
    std::string asString() const;
    bool asBool() const;

    // Array operations
    Value& operator[](int index);
    const Value& operator[](int index) const;
    Value& operator[](const std::string& key);
    const Value& operator[](const std::string& key) const;
    Value& operator[](const char* key);
    const Value& operator[](const char* key) const;

    // Array/Object size
    unsigned int size() const;
    bool empty() const;
    void clear();

    // Array operations
    Value& append(const Value& value);
    void resize(unsigned int size);

    // Object operations
    bool isMember(const std::string& key) const;
    bool isMember(const char* key) const;
    std::vector<std::string> getMemberNames() const;

    // Iterators
    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

private:
    ValueType type_;
    union {
        int int_;
        unsigned int uint_;
        long long int64_;
        unsigned long long uint64_;
        double real_;
        bool bool_;
    } value_;
    std::string* string_;
    Array* array_;
    Object* object_;

    void initBasic(ValueType type);
    void releasePayload();
    void dupPayload(const Value& other);
};

} // namespace Json

#endif // JSON_VALUE_H_INCLUDED
