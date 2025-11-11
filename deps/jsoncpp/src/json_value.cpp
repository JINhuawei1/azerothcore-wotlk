#include "json/value.h"
#include <algorithm>
#include <cstring>
#include <cassert>

namespace Json {

Value::Value(ValueType type) : type_(type), string_(nullptr), array_(nullptr), object_(nullptr) {
    initBasic(type);
}

Value::Value(int value) : type_(intValue), string_(nullptr), array_(nullptr), object_(nullptr) {
    value_.int_ = value;
}

Value::Value(unsigned int value) : type_(uintValue), string_(nullptr), array_(nullptr), object_(nullptr) {
    value_.uint_ = value;
}

Value::Value(long long value) : type_(int64Value), string_(nullptr), array_(nullptr), object_(nullptr) {
    value_.int64_ = value;
}

Value::Value(unsigned long long value) : type_(uint64Value), string_(nullptr), array_(nullptr), object_(nullptr) {
    value_.uint64_ = value;
}

Value::Value(double value) : type_(realValue), string_(nullptr), array_(nullptr), object_(nullptr) {
    value_.real_ = value;
}

Value::Value(const char* value) : type_(stringValue), array_(nullptr), object_(nullptr) {
    string_ = new std::string(value ? value : "");
}

Value::Value(const std::string& value) : type_(stringValue), array_(nullptr), object_(nullptr) {
    string_ = new std::string(value);
}

Value::Value(bool value) : type_(booleanValue), string_(nullptr), array_(nullptr), object_(nullptr) {
    value_.bool_ = value;
}

Value::Value(const Value& other) : type_(nullValue), string_(nullptr), array_(nullptr), object_(nullptr) {
    dupPayload(other);
}

Value::~Value() {
    releasePayload();
}

Value& Value::operator=(const Value& other) {
    if (this != &other) {
        releasePayload();
        dupPayload(other);
    }
    return *this;
}

ValueType Value::type() const {
    return type_;
}

bool Value::isNull() const { return type_ == nullValue; }
bool Value::isBool() const { return type_ == booleanValue; }
bool Value::isInt() const { return type_ == intValue; }
bool Value::isUInt() const { return type_ == uintValue; }
bool Value::isIntegral() const { return type_ == intValue || type_ == uintValue || type_ == int64Value || type_ == uint64Value; }
bool Value::isDouble() const { return type_ == realValue; }
bool Value::isNumeric() const { return isIntegral() || isDouble(); }
bool Value::isString() const { return type_ == stringValue; }
bool Value::isArray() const { return type_ == arrayValue; }
bool Value::isObject() const { return type_ == objectValue; }

int Value::asInt() const {
    switch (type_) {
    case intValue: return value_.int_;
    case uintValue: return static_cast<int>(value_.uint_);
    case int64Value: return static_cast<int>(value_.int64_);
    case uint64Value: return static_cast<int>(value_.uint64_);
    case realValue: return static_cast<int>(value_.real_);
    case booleanValue: return value_.bool_ ? 1 : 0;
    default: return 0;
    }
}

unsigned int Value::asUInt() const {
    switch (type_) {
    case intValue: return static_cast<unsigned int>(value_.int_);
    case uintValue: return value_.uint_;
    case int64Value: return static_cast<unsigned int>(value_.int64_);
    case uint64Value: return static_cast<unsigned int>(value_.uint64_);
    case realValue: return static_cast<unsigned int>(value_.real_);
    case booleanValue: return value_.bool_ ? 1 : 0;
    default: return 0;
    }
}

long long Value::asInt64() const {
    switch (type_) {
    case intValue: return static_cast<long long>(value_.int_);
    case uintValue: return static_cast<long long>(value_.uint_);
    case int64Value: return value_.int64_;
    case uint64Value: return static_cast<long long>(value_.uint64_);
    case realValue: return static_cast<long long>(value_.real_);
    case booleanValue: return value_.bool_ ? 1 : 0;
    default: return 0;
    }
}

unsigned long long Value::asUInt64() const {
    switch (type_) {
    case intValue: return static_cast<unsigned long long>(value_.int_);
    case uintValue: return static_cast<unsigned long long>(value_.uint_);
    case int64Value: return static_cast<unsigned long long>(value_.int64_);
    case uint64Value: return value_.uint64_;
    case realValue: return static_cast<unsigned long long>(value_.real_);
    case booleanValue: return value_.bool_ ? 1 : 0;
    default: return 0;
    }
}

double Value::asDouble() const {
    switch (type_) {
    case intValue: return static_cast<double>(value_.int_);
    case uintValue: return static_cast<double>(value_.uint_);
    case int64Value: return static_cast<double>(value_.int64_);
    case uint64Value: return static_cast<double>(value_.uint64_);
    case realValue: return value_.real_;
    case booleanValue: return value_.bool_ ? 1.0 : 0.0;
    default: return 0.0;
    }
}

std::string Value::asString() const {
    if (type_ == stringValue && string_) {
        return *string_;
    }
    return "";
}

bool Value::asBool() const {
    switch (type_) {
    case booleanValue: return value_.bool_;
    case intValue: return value_.int_ != 0;
    case uintValue: return value_.uint_ != 0;
    case int64Value: return value_.int64_ != 0;
    case uint64Value: return value_.uint64_ != 0;
    case realValue: return value_.real_ != 0.0;
    default: return false;
    }
}

Value& Value::operator[](int index) {
    if (type_ != arrayValue) {
        releasePayload();
        type_ = arrayValue;
        array_ = new Array();
    }
    
    if (index < 0) return *this;
    
    if (static_cast<unsigned int>(index) >= array_->size()) {
        array_->resize(index + 1);
    }
    return (*array_)[index];
}

const Value& Value::operator[](int index) const {
    static Value null;
    if (type_ != arrayValue || index < 0 || static_cast<unsigned int>(index) >= array_->size()) {
        return null;
    }
    return (*array_)[index];
}

Value& Value::operator[](const std::string& key) {
    if (type_ != objectValue) {
        releasePayload();
        type_ = objectValue;
        object_ = new Object();
    }
    return (*object_)[key];
}

const Value& Value::operator[](const std::string& key) const {
    static Value null;
    if (type_ != objectValue) return null;
    Object::const_iterator it = object_->find(key);
    return it != object_->end() ? it->second : null;
}

Value& Value::operator[](const char* key) {
    return (*this)[std::string(key)];
}

const Value& Value::operator[](const char* key) const {
    return (*this)[std::string(key)];
}

unsigned int Value::size() const {
    switch (type_) {
    case arrayValue: return array_ ? static_cast<unsigned int>(array_->size()) : 0;
    case objectValue: return object_ ? static_cast<unsigned int>(object_->size()) : 0;
    default: return 0;
    }
}

bool Value::empty() const {
    return size() == 0;
}

void Value::clear() {
    releasePayload();
    type_ = nullValue;
}

Value& Value::append(const Value& value) {
    if (type_ != arrayValue) {
        releasePayload();
        type_ = arrayValue;
        array_ = new Array();
    }
    array_->push_back(value);
    return *this;
}

void Value::resize(unsigned int size) {
    if (type_ != arrayValue) {
        releasePayload();
        type_ = arrayValue;
        array_ = new Array();
    }
    array_->resize(size);
}

bool Value::isMember(const std::string& key) const {
    if (type_ != objectValue) return false;
    return object_->find(key) != object_->end();
}

bool Value::isMember(const char* key) const {
    return isMember(std::string(key));
}

std::vector<std::string> Value::getMemberNames() const {
    std::vector<std::string> names;
    if (type_ == objectValue && object_) {
        for (Object::const_iterator it = object_->begin(); it != object_->end(); ++it) {
            names.push_back(it->first);
        }
    }
    return names;
}

Value::iterator Value::begin() {
    if (type_ == arrayValue && array_) {
        return array_->begin();
    }
    return iterator();
}

Value::iterator Value::end() {
    if (type_ == arrayValue && array_) {
        return array_->end();
    }
    return iterator();
}

Value::const_iterator Value::begin() const {
    if (type_ == arrayValue && array_) {
        return array_->begin();
    }
    return const_iterator();
}

Value::const_iterator Value::end() const {
    if (type_ == arrayValue && array_) {
        return array_->end();
    }
    return const_iterator();
}

void Value::initBasic(ValueType type) {
    type_ = type;
    switch (type) {
    case nullValue: break;
    case intValue: value_.int_ = 0; break;
    case uintValue: value_.uint_ = 0; break;
    case int64Value: value_.int64_ = 0; break;
    case uint64Value: value_.uint64_ = 0; break;
    case realValue: value_.real_ = 0.0; break;
    case booleanValue: value_.bool_ = false; break;
    case stringValue: string_ = new std::string(); break;
    case arrayValue: array_ = new Array(); break;
    case objectValue: object_ = new Object(); break;
    }
}

void Value::releasePayload() {
    switch (type_) {
    case stringValue: delete string_; break;
    case arrayValue: delete array_; break;
    case objectValue: delete object_; break;
    default: break;
    }
    string_ = nullptr;
    array_ = nullptr;
    object_ = nullptr;
}

void Value::dupPayload(const Value& other) {
    type_ = other.type_;
    switch (type_) {
    case nullValue: break;
    case intValue: value_.int_ = other.value_.int_; break;
    case uintValue: value_.uint_ = other.value_.uint_; break;
    case int64Value: value_.int64_ = other.value_.int64_; break;
    case uint64Value: value_.uint64_ = other.value_.uint64_; break;
    case realValue: value_.real_ = other.value_.real_; break;
    case booleanValue: value_.bool_ = other.value_.bool_; break;
    case stringValue: string_ = new std::string(*other.string_); break;
    case arrayValue: array_ = new Array(*other.array_); break;
    case objectValue: object_ = new Object(*other.object_); break;
    }
}

} // namespace Json
