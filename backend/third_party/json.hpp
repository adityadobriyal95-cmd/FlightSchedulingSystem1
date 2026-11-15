#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <initializer_list>

namespace nlohmann {

class json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

private:
    Type type_ = Type::Null;
    bool boolVal_ = false;
    double numVal_ = 0.0;
    bool isInt_ = false;
    long long intVal_ = 0;
    std::string strVal_;
    std::vector<json> arrVal_;
    std::map<std::string, json> objVal_;

public:
    json() : type_(Type::Null) {}
    json(std::nullptr_t) : type_(Type::Null) {}
    json(bool b) : type_(Type::Bool), boolVal_(b) {}
    json(int v) : type_(Type::Number), numVal_(static_cast<double>(v)), isInt_(true), intVal_(v) {}
    json(long v) : type_(Type::Number), numVal_(static_cast<double>(v)), isInt_(true), intVal_(v) {}
    json(long long v) : type_(Type::Number), numVal_(static_cast<double>(v)), isInt_(true), intVal_(v) {}
    json(unsigned int v) : type_(Type::Number), numVal_(static_cast<double>(v)), isInt_(true), intVal_(v) {}
    json(size_t v) : type_(Type::Number), numVal_(static_cast<double>(v)), isInt_(true), intVal_(static_cast<long long>(v)) {}
    json(double v) : type_(Type::Number), numVal_(v), isInt_(false) {}
    json(const char* s) : type_(Type::String), strVal_(s ? s : "") {}
    json(const std::string& s) : type_(Type::String), strVal_(s) {}

    json(std::initializer_list<json> init) {
        bool isObj = true;
        if (init.size() == 0) {
            type_ = Type::Object;
            return;
        }
        for (const auto& el : init) {
            if (el.type_ != Type::Array || el.arrVal_.size() != 2 || el.arrVal_[0].type_ != Type::String) {
                isObj = false;
                break;
            }
        }
        if (isObj) {
            type_ = Type::Object;
            for (const auto& el : init) {
                objVal_[el.arrVal_[0].strVal_] = el.arrVal_[1];
            }
        } else {
            type_ = Type::Array;
            arrVal_.assign(init.begin(), init.end());
        }
    }

    static json array() {
        json j;
        j.type_ = Type::Array;
        return j;
    }

    static json object() {
        json j;
        j.type_ = Type::Object;
        return j;
    }

    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    void push_back(const json& val) {
        if (type_ == Type::Null) type_ = Type::Array;
        if (type_ != Type::Array) throw std::runtime_error("json::push_back called on non-array");
        arrVal_.push_back(val);
    }

    bool contains(const std::string& key) const {
        return type_ == Type::Object && objVal_.find(key) != objVal_.end();
    }

    json& operator[](const std::string& key) {
        if (type_ == Type::Null) type_ = Type::Object;
        if (type_ != Type::Object) throw std::runtime_error("json::operator[] called on non-object");
        return objVal_[key];
    }

    const json& operator[](const std::string& key) const {
        static json nullJson;
        if (type_ != Type::Object) return nullJson;
        auto it = objVal_.find(key);
        return (it != objVal_.end()) ? it->second : nullJson;
    }

    std::string value(const std::string& key, const char* def) const {
        return value(key, std::string(def));
    }

    std::string value(const std::string& key, const std::string& def) const {
        if (!contains(key)) return def;
        const auto& v = objVal_.at(key);
        if (v.is_string()) return v.strVal_;
        return def;
    }

    int value(const std::string& key, int def) const {
        if (!contains(key)) return def;
        const auto& v = objVal_.at(key);
        if (v.is_number()) return v.isInt_ ? static_cast<int>(v.intVal_) : static_cast<int>(v.numVal_);
        if (v.is_string() && !v.strVal_.empty()) {
            try { return std::stoi(v.strVal_); } catch (...) {}
        }
        return def;
    }

    double value(const std::string& key, double def) const {
        if (!contains(key)) return def;
        const auto& v = objVal_.at(key);
        if (v.is_number()) return v.numVal_;
        if (v.is_string() && !v.strVal_.empty()) {
            try { return std::stod(v.strVal_); } catch (...) {}
        }
        return def;
    }

    bool value(const std::string& key, bool def) const {
        if (!contains(key)) return def;
        const auto& v = objVal_.at(key);
        if (v.is_bool()) return v.boolVal_;
        return def;
    }

    std::vector<json>::const_iterator begin() const { return arrVal_.begin(); }
    std::vector<json>::const_iterator end() const { return arrVal_.end(); }

    static std::string escapeString(const std::string& s) {
        std::ostringstream oss;
        for (unsigned char c : s) {
            switch (c) {
                case '"': oss << "\""; break;
                case '\': oss << "\\"; break;
                case '': oss << "\b"; break;
                case '': oss << "\f"; break;
                case '
': oss << "\n"; break;
                case '': oss << "\r"; break;
                case '	': oss << "\t"; break;
                default:
                    if (c < 0x20) {
                        oss << "\u00";
                        const char* hex = "0123456789abcdef";
                        oss << hex[(c >> 4) & 0xF] << hex[c & 0xF];
                    } else {
                        oss << static_cast<char>(c);
                    }
            }
        }
        return oss.str();
    }

    std::string dump() const {
        std::ostringstream oss;
        switch (type_) {
            case Type::Null: oss << "null"; break;
            case Type::Bool: oss << (boolVal_ ? "true" : "false"); break;
            case Type::Number:
                if (isInt_) oss << intVal_;
                else if (std::isnan(numVal_) || std::isinf(numVal_)) oss << "0";
                else oss << numVal_;
                break;
            case Type::String:
                oss << """ << escapeString(strVal_) << """;
                break;
            case Type::Array: {
                oss << "[";
                for (size_t i = 0; i < arrVal_.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << arrVal_[i].dump();
                }
                oss << "]";
                break;
            }
            case Type::Object: {
                oss << "{";
                size_t i = 0;
                for (const auto& kv : objVal_) {
                    if (i++ > 0) oss << ",";
                    oss << """ << escapeString(kv.first) << "":" << kv.second.dump();
                }
                oss << "}";
                break;
            }
        }
        return oss.str();
    }

    static json parse(const std::string& text) {
        size_t pos = 0;
        return parseValue(text, pos);
    }

private:
    static void skipWs(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) pos++;
    }

    static json parseValue(const std::string& s, size_t& pos) {
        skipWs(s, pos);
        if (pos >= s.size()) throw std::runtime_error("Unexpected end of JSON");
        char c = s[pos];
        if (c == '{') return parseObject(s, pos);
        if (c == '[') return parseArray(s, pos);
        if (c == '"') return json(parseString(s, pos));
        if (c == 't' && s.compare(pos, 4, "true") == 0) { pos += 4; return json(true); }
        if (c == 'f' && s.compare(pos, 5, "false") == 0) { pos += 5; return json(false); }
        if (c == 'n' && s.compare(pos, 4, "null") == 0) { pos += 4; return json(nullptr); }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(s, pos);
        throw std::runtime_error("Invalid JSON token");
    }

    static std::string parseString(const std::string& s, size_t& pos) {
        pos++; // skip opening '"'
        std::string out;
        while (pos < s.size()) {
            char c = s[pos++];
            if (c == '"') return out;
            if (c == '\' && pos < s.size()) {
                char esc = s[pos++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\': out.push_back('\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back(''); break;
                    case 'f': out.push_back(''); break;
                    case 'n': out.push_back('
'); break;
                    case 'r': out.push_back(''); break;
                    case 't': out.push_back('	'); break;
                    case 'u':
                        if (pos + 4 <= s.size()) {
                            unsigned int code = std::stoul(s.substr(pos, 4), nullptr, 16);
                            if (code < 0x80) out.push_back(static_cast<char>(code));
                            pos += 4;
                        }
                        break;
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        throw std::runtime_error("Unterminated JSON string");
    }

    static json parseNumber(const std::string& s, size_t& pos) {
        size_t start = pos;
        bool isFloat = false;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) pos++;
        if (pos < s.size() && s[pos] == '.') {
            isFloat = true;
            pos++;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) pos++;
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            isFloat = true;
            pos++;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
            while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) pos++;
        }
        std::string numStr = s.substr(start, pos - start);
        if (isFloat) return json(std::stod(numStr));
        return json(std::stoll(numStr));
    }

    static json parseArray(const std::string& s, size_t& pos) {
        pos++; // '['
        json arr = json::array();
        skipWs(s, pos);
        if (pos < s.size() && s[pos] == ']') { pos++; return arr; }
        while (pos < s.size()) {
            arr.push_back(parseValue(s, pos));
            skipWs(s, pos);
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == ']') { pos++; return arr; }
            throw std::runtime_error("Expected ',' or ']' in JSON array");
        }
        throw std::runtime_error("Unterminated JSON array");
    }

    static json parseObject(const std::string& s, size_t& pos) {
        pos++; // '{'
        json obj = json::object();
        skipWs(s, pos);
        if (pos < s.size() && s[pos] == '}') { pos++; return obj; }
        while (pos < s.size()) {
            skipWs(s, pos);
            if (pos >= s.size() || s[pos] != '"') throw std::runtime_error("Expected string key in JSON object");
            std::string key = parseString(s, pos);
            skipWs(s, pos);
            if (pos >= s.size() || s[pos] != ':') throw std::runtime_error("Expected ':' in JSON object");
            pos++;
            obj[key] = parseValue(s, pos);
            skipWs(s, pos);
            if (pos < s.size() && s[pos] == ',') { pos++; continue; }
            if (pos < s.size() && s[pos] == '}') { pos++; return obj; }
            throw std::runtime_error("Expected ',' or '}' in JSON object");
        }
        throw std::runtime_error("Unterminated JSON object");
    }
};

} // namespace nlohmann
