/*
 * Simple JSON Parser for WebAssembly
 * Minimal implementation for Stratum protocol
 */

#ifndef SIMPLE_JSON_H
#define SIMPLE_JSON_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <cstdlib>

namespace simple_json {

class Value {
public:
    enum Type { NULL_TYPE, BOOL_TYPE, INT_TYPE, UINT_TYPE, REAL_TYPE, STRING_TYPE, OBJECT_TYPE, ARRAY_TYPE };
    
    Value() : type_(NULL_TYPE) {}
    Value(const std::string& s) : type_(STRING_TYPE), stringValue_(s) {}
    Value(const char* s) : type_(STRING_TYPE), stringValue_(s) {}
    Value(int i) : type_(INT_TYPE), intValue_(i) {}
    Value(uint64_t u) : type_(UINT_TYPE), uintValue_(u) {}
    Value(bool b) : type_(BOOL_TYPE), boolValue_(b) {}
    
    Type type() const { return type_; }
    
    bool isString() const { return type_ == STRING_TYPE; }
    bool isInt() const { return type_ == INT_TYPE || type_ == UINT_TYPE; }
    bool isUInt64() const { return type_ == UINT_TYPE; }
    bool isObject() const { return type_ == OBJECT_TYPE; }
    bool isArray() const { return type_ == ARRAY_TYPE; }
    bool isMember(const std::string& key) const {
        return type_ == OBJECT_TYPE && objectValue_.count(key) > 0;
    }
    
    std::string asString() const {
        if (type_ == STRING_TYPE) return stringValue_;
        if (type_ == INT_TYPE) return std::to_string(intValue_);
        if (type_ == UINT_TYPE) return std::to_string(uintValue_);
        return "";
    }
    
    int asInt() const {
        if (type_ == INT_TYPE) return intValue_;
        if (type_ == UINT_TYPE) return (int)uintValue_;
        if (type_ == STRING_TYPE) return std::atoi(stringValue_.c_str());
        return 0;
    }
    
    uint64_t asUInt64() const {
        if (type_ == UINT_TYPE) return uintValue_;
        if (type_ == INT_TYPE) return (uint64_t)intValue_;
        if (type_ == STRING_TYPE) return std::strtoull(stringValue_.c_str(), nullptr, 10);
        return 0;
    }
    
    Value& operator[](const std::string& key) {
        type_ = OBJECT_TYPE;
        return objectValue_[key];
    }
    
    const Value& operator[](const std::string& key) const {
        static Value nullValue;
        if (type_ == OBJECT_TYPE) {
            auto it = objectValue_.find(key);
            if (it != objectValue_.end()) return it->second;
        }
        return nullValue;
    }
    
    void append(const Value& v) {
        type_ = ARRAY_TYPE;
        arrayValue_.push_back(v);
    }
    
private:
    Type type_;
    std::string stringValue_;
    int intValue_ = 0;
    uint64_t uintValue_ = 0;
    bool boolValue_ = false;
    std::map<std::string, Value> objectValue_;
    std::vector<Value> arrayValue_;
};

class Parser {
public:
    bool parse(const std::string& text, Value& root) {
        size_t pos = 0;
        skipWhitespace(text, pos);
        root = parseValue(text, pos);
        return true; // Simplified - always returns true
    }
    
private:
    void skipWhitespace(const std::string& text, size_t& pos) {
        while (pos < text.length() && isspace(text[pos])) pos++;
    }
    
    Value parseValue(const std::string& text, size_t& pos) {
        skipWhitespace(text, pos);
        if (pos >= text.length()) return Value();
        
        char c = text[pos];
        if (c == '{') return parseObject(text, pos);
        if (c == '[') return parseArray(text, pos);
        if (c == '"') return parseString(text, pos);
        if (c == 't' || c == 'f') return parseBool(text, pos);
        if (c == 'n') return parseNull(text, pos);
        return parseNumber(text, pos);
    }
    
    Value parseObject(const std::string& text, size_t& pos) {
        Value obj;
        pos++; // skip {
        skipWhitespace(text, pos);
        
        while (pos < text.length() && text[pos] != '}') {
            skipWhitespace(text, pos);
            std::string key = parseString(text, pos).asString();
            skipWhitespace(text, pos);
            if (pos < text.length() && text[pos] == ':') pos++;
            Value val = parseValue(text, pos);
            obj[key] = val;
            skipWhitespace(text, pos);
            if (pos < text.length() && text[pos] == ',') pos++;
        }
        if (pos < text.length() && text[pos] == '}') pos++;
        return obj;
    }
    
    Value parseArray(const std::string& text, size_t& pos) {
        Value arr;
        pos++; // skip [
        skipWhitespace(text, pos);
        
        while (pos < text.length() && text[pos] != ']') {
            Value val = parseValue(text, pos);
            arr.append(val);
            skipWhitespace(text, pos);
            if (pos < text.length() && text[pos] == ',') pos++;
        }
        if (pos < text.length() && text[pos] == ']') pos++;
        return arr;
    }
    
    Value parseString(const std::string& text, size_t& pos) {
        pos++; // skip opening quote
        std::string result;
        while (pos < text.length() && text[pos] != '"') {
            if (text[pos] == '\\' && pos + 1 < text.length()) {
                pos++;
                char c = text[pos];
                if (c == 'n') result += '\n';
                else if (c == 't') result += '\t';
                else if (c == 'r') result += '\r';
                else result += c;
            } else {
                result += text[pos];
            }
            pos++;
        }
        if (pos < text.length() && text[pos] == '"') pos++;
        return Value(result);
    }
    
    Value parseNumber(const std::string& text, size_t& pos) {
        size_t start = pos;
        bool isFloat = false;
        while (pos < text.length() && (isdigit(text[pos]) || text[pos] == '.' || 
               text[pos] == '-' || text[pos] == 'e' || text[pos] == 'E')) {
            if (text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E') isFloat = true;
            pos++;
        }
        std::string numStr = text.substr(start, pos - start);
        if (isFloat) {
            return Value((int)(std::atof(numStr.c_str())));
        }
        return Value(std::strtoull(numStr.c_str(), nullptr, 10));
    }
    
    Value parseBool(const std::string& text, size_t& pos) {
        if (text.substr(pos, 4) == "true") {
            pos += 4;
            return Value(true);
        }
        if (text.substr(pos, 5) == "false") {
            pos += 5;
            return Value(false);
        }
        return Value();
    }
    
    Value parseNull(const std::string& text, size_t& pos) {
        if (text.substr(pos, 4) == "null") {
            pos += 4;
        }
        return Value();
    }
};

} // namespace simple_json

#endif // SIMPLE_JSON_H
