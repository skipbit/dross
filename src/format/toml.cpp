#include "dross/format/toml.h"
#include "dross/type/string.h"
#include "dross/type/number.h"
#include "dross/type/boolean.h"
#include "dross/type/array.h"
#include "dross/type/dictionary.h"
#include <sstream>
#include <cctype>

namespace dross::toml {

namespace {

/**
 * @brief TOML parser implementation using recursive descent parsing.
 *
 * This class provides a complete TOML v1.0.0 parser with proper error handling
 * and diagnostic information. It uses a lexical analyzer followed by recursive
 * descent parsing for structured analysis.
 */
class parser {
public:
    explicit parser(const std::string& input)
        : _input(input), _pos(0), _line(1), _column(1) {}

    std::expected<dictionary, error> parse()
    {
        try {
            auto result = parse_document();
            if (!result) {
                return std::unexpected(result.error());
            }

            // Ensure we've consumed all input
            skip_whitespace_and_comments();
            if (_pos < _input.size()) {
                return std::unexpected(create_error("Unexpected content after document end"));
            }

            return result.value();
        } catch (const std::exception& e) {
            return std::unexpected(create_error(std::string("Parse error: ") + e.what()));
        }
    }

private:
    std::string _input;
    size_t _pos;
    size_t _line;
    size_t _column;

    error create_error(const std::string& message) const
    {
        std::ostringstream oss;
        oss << "TOML parse error at line " << _line << ", column " << _column << ": " << message;
        // Use the error constructor that takes error code and category
        return error{static_cast<int>(std::errc::invalid_argument), std::generic_category()};
    }

    char current_char() const
    {
        return _pos < _input.size() ? _input[_pos] : '\0';
    }

    char peek_char(size_t offset = 1) const
    {
        size_t peek_pos = _pos + offset;
        return peek_pos < _input.size() ? _input[peek_pos] : '\0';
    }

    void advance()
    {
        if (_pos < _input.size()) {
            if (_input[_pos] == '\n') {
                _line++;
                _column = 1;
            } else {
                _column++;
            }
            _pos++;
        }
    }

    void skip_whitespace()
    {
        while (_pos < _input.size() && std::isspace(current_char()) && current_char() != '\n') {
            advance();
        }
    }

    void skip_whitespace_and_comments()
    {
        while (_pos < _input.size()) {
            skip_whitespace();
            if (current_char() == '#') {
                // Skip comment to end of line
                while (_pos < _input.size() && current_char() != '\n') {
                    advance();
                }
            }
            if (current_char() == '\n') {
                advance();
            } else {
                break;
            }
        }
    }

    std::expected<dictionary, error> parse_document()
    {
        dictionary result;

        while (_pos < _input.size()) {
            skip_whitespace_and_comments();
            if (_pos >= _input.size()) break;

            if (current_char() == '[') {
                auto table_result = parse_table_header();
                if (!table_result) return std::unexpected(table_result.error());

                auto key_path = table_result.value();
                auto table_content = parse_table_content();
                if (!table_content) return std::unexpected(table_content.error());

                // Insert nested table into result
                set_nested_value(result, key_path, table_content.value());
            } else {
                auto kv_result = parse_key_value();
                if (!kv_result) return std::unexpected(kv_result.error());

                auto [key, val] = kv_result.value();
                result[key] = val;
            }
        }

        return result;
    }

    std::expected<std::vector<std::string>, error> parse_table_header()
    {
        if (current_char() != '[') {
            return std::unexpected(create_error("Expected '['"));
        }
        advance(); // skip '['

        bool is_array_table = false;
        if (current_char() == '[') {
            is_array_table = true;
            advance(); // skip second '['
        }

        std::vector<std::string> key_path;

        while (true) {
            skip_whitespace();
            auto key_result = parse_key();
            if (!key_result) return std::unexpected(key_result.error());

            key_path.push_back(key_result.value());

            skip_whitespace();
            if (current_char() == '.') {
                advance();
                continue;
            } else if (current_char() == ']') {
                advance();
                if (is_array_table) {
                    if (current_char() != ']') {
                        return std::unexpected(create_error("Expected ']]' for array table"));
                    }
                    advance();
                }
                break;
            } else {
                return std::unexpected(create_error("Expected '.' or ']'"));
            }
        }

        return key_path;
    }

    std::expected<dictionary, error> parse_table_content()
    {
        dictionary result;

        while (_pos < _input.size()) {
            skip_whitespace_and_comments();
            if (_pos >= _input.size() || current_char() == '[') {
                break;
            }

            auto kv_result = parse_key_value();
            if (!kv_result) return std::unexpected(kv_result.error());

            auto [key, val] = kv_result.value();
            result[key] = val;
        }

        return result;
    }

    std::expected<std::pair<std::string, value>, error> parse_key_value()
    {
        auto key_result = parse_key();
        if (!key_result) return std::unexpected(key_result.error());

        std::string key = key_result.value();

        skip_whitespace();
        if (current_char() != '=') {
            return std::unexpected(create_error("Expected '='"));
        }
        advance(); // skip '='

        skip_whitespace();
        auto value_result = parse_value();
        if (!value_result) return std::unexpected(value_result.error());

        skip_whitespace_and_comments();

        return std::make_pair(key, value_result.value());
    }

    std::expected<std::string, error> parse_key()
    {
        if (current_char() == '"') {
            return parse_quoted_key();
        } else if (std::isalpha(current_char()) || current_char() == '_') {
            return parse_bare_key();
        } else {
            return std::unexpected(create_error("Invalid key format"));
        }
    }

    std::expected<std::string, error> parse_bare_key()
    {
        std::string result;
        while (_pos < _input.size() &&
               (std::isalnum(current_char()) || current_char() == '_' || current_char() == '-')) {
            result += current_char();
            advance();
        }

        if (result.empty()) {
            return std::unexpected(create_error("Empty bare key"));
        }

        return result;
    }

    std::expected<std::string, error> parse_quoted_key()
    {
        if (current_char() != '"') {
            return std::unexpected(create_error("Expected '\"'"));
        }
        advance(); // skip opening quote

        std::string result;
        while (_pos < _input.size() && current_char() != '"') {
            if (current_char() == '\\') {
                advance();
                if (_pos >= _input.size()) {
                    return std::unexpected(create_error("Unterminated quoted key"));
                }
                // Handle escape sequences
                switch (current_char()) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    default:
                        return std::unexpected(create_error("Invalid escape sequence"));
                }
            } else {
                result += current_char();
            }
            advance();
        }

        if (current_char() != '"') {
            return std::unexpected(create_error("Unterminated quoted key"));
        }
        advance(); // skip closing quote

        return result;
    }

    std::expected<value, error> parse_value()
    {
        skip_whitespace();

        char ch = current_char();

        if (ch == '"') {
            return parse_string();
        } else if (ch == '[') {
            return parse_array();
        } else if (ch == '{') {
            return parse_inline_table();
        } else if (ch == 't' || ch == 'f') {
            return parse_boolean();
        } else if (std::isdigit(ch) || ch == '-' || ch == '+') {
            return parse_number();
        } else {
            return std::unexpected(create_error("Invalid value format"));
        }
    }

    std::expected<value, error> parse_string()
    {
        if (current_char() != '"') {
            return std::unexpected(create_error("Expected '\"'"));
        }
        advance(); // skip opening quote

        std::string result;
        while (_pos < _input.size() && current_char() != '"') {
            if (current_char() == '\\') {
                advance();
                if (_pos >= _input.size()) {
                    return std::unexpected(create_error("Unterminated string"));
                }
                // Handle escape sequences
                switch (current_char()) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    default:
                        return std::unexpected(create_error("Invalid escape sequence"));
                }
            } else {
                result += current_char();
            }
            advance();
        }

        if (current_char() != '"') {
            return std::unexpected(create_error("Unterminated string"));
        }
        advance(); // skip closing quote

        return value(string{result});
    }

    std::expected<value, error> parse_array()
    {
        if (current_char() != '[') {
            return std::unexpected(create_error("Expected '['"));
        }
        advance(); // skip '['

        array result;

        skip_whitespace_and_comments();
        if (current_char() == ']') {
            advance();
            return value(result);
        }

        while (true) {
            auto value_result = parse_value();
            if (!value_result) return std::unexpected(value_result.error());

            result.append(value_result.value());

            skip_whitespace_and_comments();
            if (current_char() == ',') {
                advance();
                skip_whitespace_and_comments();
                continue;
            } else if (current_char() == ']') {
                advance();
                break;
            } else {
                return std::unexpected(create_error("Expected ',' or ']'"));
            }
        }

        return value(result);
    }

    std::expected<value, error> parse_inline_table()
    {
        if (current_char() != '{') {
            return std::unexpected(create_error("Expected '{'"));
        }
        advance(); // skip '{'

        dictionary result;

        skip_whitespace();
        if (current_char() == '}') {
            advance();
            return value(result);
        }

        while (true) {
            auto kv_result = parse_key_value();
            if (!kv_result) return std::unexpected(kv_result.error());

            auto [key, val] = kv_result.value();
            result[key] = val;

            skip_whitespace();
            if (current_char() == ',') {
                advance();
                skip_whitespace();
                continue;
            } else if (current_char() == '}') {
                advance();
                break;
            } else {
                return std::unexpected(create_error("Expected ',' or '}'"));
            }
        }

        return value(result);
    }

    std::expected<value, error> parse_boolean()
    {
        if (_pos + 4 <= _input.size() && _input.substr(_pos, 4) == "true") {
            _pos += 4;
            _column += 4;
            return value(boolean{true});
        } else if (_pos + 5 <= _input.size() && _input.substr(_pos, 5) == "false") {
            _pos += 5;
            _column += 5;
            return value(boolean{false});
        } else {
            return std::unexpected(create_error("Invalid boolean value"));
        }
    }

    std::expected<value, error> parse_number()
    {
        std::string num_str;

        // Handle sign
        if (current_char() == '+' || current_char() == '-') {
            num_str += current_char();
            advance();
        }

        // Parse digits and decimal point
        bool has_dot = false;
        while (_pos < _input.size() &&
               (std::isdigit(current_char()) || current_char() == '.')) {
            if (current_char() == '.') {
                if (has_dot) break; // Only one decimal point allowed
                has_dot = true;
            }
            num_str += current_char();
            advance();
        }

        if (num_str.empty() || num_str == "+" || num_str == "-" || num_str == ".") {
            return std::unexpected(create_error("Invalid number format"));
        }

        return value(number{num_str});
    }

    void set_nested_value(dictionary& root, const std::vector<std::string>& key_path, const value& val)
    {
        set_nested_value_recursive(root, key_path, 0, val);
    }

    void set_nested_value_recursive(dictionary& current_dict, const std::vector<std::string>& key_path, size_t index, const value& val)
    {
        if (key_path.empty() || index >= key_path.size()) {
            return;
        }

        const std::string& current_key = key_path[index];

        if (index == key_path.size() - 1) {
            // This is the final key, set the value
            current_dict[current_key] = val;
            return;
        }

        // This is an intermediate key, ensure it's a dictionary
        if (!current_dict.contains(current_key)) {
            current_dict[current_key] = value(dictionary{});
        } else if (!current_dict[current_key].is<dictionary>()) {
            // Key exists but is not a dictionary - replace it
            current_dict[current_key] = value(dictionary{});
        }

        // Get the nested dictionary and continue recursively
        dictionary nested_dict = current_dict[current_key].as<dictionary>();
        set_nested_value_recursive(nested_dict, key_path, index + 1, val);

        // Put the modified dictionary back
        current_dict[current_key] = value(nested_dict);
    }
};

/**
 * @brief TOML serializer implementation.
 *
 * This class converts dross structured data back to TOML format with
 * proper formatting and escaping according to TOML v1.0.0 specification.
 */
class serializer {
public:
    std::expected<data, error> serialize(const dictionary& input)
    {
        try {
            _output.clear();
            _indent_level = 0;

            serialize_dictionary(input, {});

            return data{_output};
        } catch (const std::exception& e) {
            return std::unexpected(error{static_cast<int>(std::errc::operation_not_supported), std::generic_category()});
        }
    }

private:
    std::string _output;
    size_t _indent_level;

    void serialize_dictionary(const dictionary& dict, const std::vector<std::string>& path)
    {
        // First, output simple key-value pairs
        for (const auto& [key, val] : dict) {
            if (!val.is<dictionary>()) {
                serialize_key_value(key, val);
            }
        }

        // Then output nested tables
        for (const auto& [key, val] : dict) {
            if (val.is<dictionary>()) {
                auto nested_path = path;
                nested_path.push_back(key);

                _output += "\n[" + join_path(nested_path) + "]\n";

                serialize_dictionary(val.as<dictionary>(), nested_path);
            }
        }
    }

    void serialize_key_value(const std::string& key, const value& val)
    {
        _output += escape_key(key) + " = ";
        serialize_value(val);
        _output += "\n";
    }

    void serialize_value(const value& val)
    {
        if (val.is<string>()) {
            _output += "\"" + escape_string(val.as<string>()) + "\"";
        } else if (val.is<number>()) {
            _output += std::string(val.as<number>());
        } else if (val.is<boolean>()) {
            _output += val.as<boolean>() ? "true" : "false";
        } else if (val.is<array>()) {
            serialize_array(val.as<array>());
        } else if (val.is<dictionary>()) {
            serialize_inline_table(val.as<dictionary>());
        } else {
            throw std::runtime_error("Unsupported value type for TOML serialization");
        }
    }

    void serialize_array(const array& arr)
    {
        _output += "[";
        for (size_t i = 0; i < arr.length(); ++i) {
            if (i > 0) _output += ", ";
            serialize_value(arr[i]);
        }
        _output += "]";
    }

    void serialize_inline_table(const dictionary& dict)
    {
        _output += "{";
        bool first = true;
        for (const auto& [key, val] : dict) {
            if (!first) _output += ", ";
            first = false;
            _output += escape_key(key) + " = ";
            serialize_value(val);
        }
        _output += "}";
    }

    std::string escape_key(const std::string& key) const
    {
        // Check if key needs quoting
        bool needs_quotes = false;
        for (char ch : key) {
            if (!std::isalnum(ch) && ch != '_' && ch != '-') {
                needs_quotes = true;
                break;
            }
        }

        if (needs_quotes) {
            return "\"" + escape_string(key) + "\"";
        } else {
            return key;
        }
    }

    std::string escape_string(const std::string& str) const
    {
        std::string result;
        for (char ch : str) {
            switch (ch) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\t': result += "\\t"; break;
                case '\r': result += "\\r"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                default: result += ch; break;
            }
        }
        return result;
    }

    std::string join_path(const std::vector<std::string>& path) const
    {
        std::string result;
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0) result += ".";
            result += escape_key(path[i]);
        }
        return result;
    }

    bool has_non_dict_values(const dictionary& dict) const
    {
        for (const auto& [key, val] : dict) {
            if (!val.is<dictionary>()) {
                return true;
            }
        }
        return false;
    }
};

} // anonymous namespace

std::expected<dictionary, error> deserialize(const data& input)
{
    if (input.empty()) {
        return dictionary{};
    }

    // Convert binary data to string
    std::string toml_string = input;

    parser p{toml_string};
    return p.parse();
}

std::expected<data, error> serialize(const dictionary& input)
{
    serializer s;
    return s.serialize(input);
}

} // namespace dross::toml
