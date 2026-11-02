#include "dross/format/toml.h"

#include "dross/type/array.h"
#include "dross/type/boolean.h"
#include "dross/type/dictionary.h"
#include "dross/type/number.h"
#include "dross/type/string.h"
#include "dross/type/timestamp.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <new>
#include <string>

namespace dross::toml {

namespace {

// Well-formed UTF-8 as defined by Unicode Table 3-7: no overlong forms,
// no surrogates, nothing beyond U+10FFFF.
bool is_valid_utf8(const std::string& text)
{
    size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        size_t length = 0;
        unsigned char low = 0x80;
        unsigned char high = 0xBF;
        if (lead <= 0x7F) {
            ++i;
            continue;
        } else if ((lead >= 0xC2) && (lead <= 0xDF)) {
            length = 2;
        } else if (lead == 0xE0) {
            length = 3;
            low = 0xA0;
        } else if (lead == 0xED) {
            length = 3;
            high = 0x9F;
        } else if ((lead >= 0xE1) && (lead <= 0xEF)) {
            length = 3;
        } else if (lead == 0xF0) {
            length = 4;
            low = 0x90;
        } else if (lead == 0xF4) {
            length = 4;
            high = 0x8F;
        } else if ((lead >= 0xF1) && (lead <= 0xF3)) {
            length = 4;
        } else {
            return false;
        }

        if ((text.size() - i) < length) {
            return false;
        }
        for (size_t k = 1; k < length; ++k) {
            const auto byte = static_cast<unsigned char>(text[i + k]);
            if ((byte < low) || (byte > high)) {
                return false;
            }
            low = 0x80;
            high = 0xBF;
        }
        i += length;
    }
    return true;
}

// TOML allows no control character other than tab in strings and comments.
bool is_forbidden_control(char c)
{
    const auto byte = static_cast<unsigned char>(c);
    return ((byte < 0x20) && (byte != '\t')) || (byte == 0x7F);
}

// TOML defines keys and numbers in ASCII, so these do not depend on the locale.
bool is_ascii_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

bool is_ascii_alpha(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}

bool is_ascii_alnum(char c)
{
    return is_ascii_alpha(c) || is_ascii_digit(c);
}

bool is_ascii_xdigit(char c)
{
    return is_ascii_digit(c) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'));
}

// The value of a digit in base 2, 8, 10 or 16, or -1 for a character that is not one.
int digit_value(char c, int base)
{
    int digit = -1;
    if (is_ascii_digit(c)) {
        digit = c - '0';
    } else if ((c >= 'a') && (c <= 'f')) {
        digit = c - 'a' + 10;
    } else if ((c >= 'A') && (c <= 'F')) {
        digit = c - 'A' + 10;
    }
    return (digit < base) ? digit : -1;
}

// The reader and the writer share this rule, so every key written bare reads back.
bool is_bare_key_char(char c)
{
    return is_ascii_alnum(c) || (c == '_') || (c == '-');
}

// Appends a Unicode scalar value (no surrogates, at most U+10FFFF) as UTF-8.
void append_utf8(std::string& text, char32_t code_point)
{
    if (code_point <= 0x7F) {
        text += static_cast<char>(code_point);
    } else if (code_point <= 0x7FF) {
        text += static_cast<char>(0xC0 | (code_point >> 6));
        text += static_cast<char>(0x80 | (code_point & 0x3F));
    } else if (code_point <= 0xFFFF) {
        text += static_cast<char>(0xE0 | (code_point >> 12));
        text += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (code_point & 0x3F));
    } else {
        text += static_cast<char>(0xF0 | (code_point >> 18));
        text += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (code_point & 0x3F));
    }
}

/**
 * @brief TOML parser implementation using recursive descent parsing.
 *
 * This class reads the subset of TOML v1.0.0 that toml.h describes, directly
 * from the input characters.
 */
class parser {
public:
    explicit parser(const std::string& input)
        : _input(input)
        , _pos(0)
        , _line(1)
        , _column(1)
    {
    }

    std::expected<dictionary, error> parse()
    {
        try {
            auto result = parse_document();
            if (! result) {
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

    // The tables that headers create, either named or as the parent of a named
    // one, are kept apart from their key/value pairs so that a header and its
    // pairs update the table in place, and become a dictionary once the
    // document is read.
    struct table_node {
        dictionary pairs;
        std::map<std::string, std::unique_ptr<table_node>> tables;
        // Whether a header named this table, rather than only passing through it
        bool named = false;
        // Whether dotted keys created this table, which only they can add to
        bool dotted = false;
        // Whether [[name]] headers made this an array of tables, whose last
        // element takes the headers and pairs that follow
        bool array = false;
        std::vector<std::unique_ptr<table_node>> elements;
    };

    struct table_header {
        std::vector<std::string> key_path;
        bool array;
    };

    struct position {
        size_t line;
        size_t column;
    };

    position current_position() const
    {
        return { _line, _column };
    }

    error create_error(const std::string& message) const
    {
        return create_error(message, current_position());
    }

    error create_error(const std::string& message, const position& at) const
    {
        return error{ std::errc::invalid_argument, "line " + std::to_string(at.line) + ", column " + std::to_string(at.column) + ": " + message };
    }

    char current_char() const
    {
        return _pos < _input.size() ? _input[_pos] : '\0';
    }

    char peek_char(size_t offset = 1) const
    {
        size_t peek_pos = (_pos + offset);
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

    // TOML whitespace is space and tab, and a CR may only start a CRLF.
    // A control character is left in place for the caller to reject.
    void skip_whitespace()
    {
        while ((current_char() == ' ') || (current_char() == '\t') || ((current_char() == '\r') && (peek_char() == '\n'))) {
            advance();
        }
    }

    void skip_whitespace_and_comments()
    {
        while (_pos < _input.size()) {
            skip_whitespace();
            if (current_char() == '#') {
                // Skip comment to end of line
                while ((_pos < _input.size()) && (current_char() != '\n')) {
                    if (is_forbidden_control(current_char()) && ! ((current_char() == '\r') && (peek_char() == '\n'))) {
                        break;  // left for the caller to reject
                    }
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

    // A key/value pair or a table header ends its line, before an optional comment.
    std::expected<void, error> expect_line_end()
    {
        skip_whitespace();
        if ((_pos < _input.size()) && (current_char() != '\n') && (current_char() != '#')) {
            return std::unexpected(create_error("Expected a newline"));
        }
        return {};
    }

    std::expected<dictionary, error> parse_document()
    {
        table_node result;
        table_node* current = &result;

        while (_pos < _input.size()) {
            skip_whitespace_and_comments();
            if (_pos >= _input.size()) {
                break;
            }

            if (current_char() == '[') {
                const position header_start = current_position();
                auto table_result = parse_table_header();
                if (! table_result) {
                    return std::unexpected(table_result.error());
                }

                auto line_end = expect_line_end();
                if (! line_end) {
                    return std::unexpected(line_end.error());
                }

                auto table = open_table(result, table_result.value(), header_start);
                if (! table) {
                    return std::unexpected(table.error());
                }
                current = table.value();
            } else {
                const position pair_start = current_position();
                auto kv_result = parse_key_value();
                if (! kv_result) {
                    return std::unexpected(kv_result.error());
                }
                auto line_end = expect_line_end();
                if (! line_end) {
                    return std::unexpected(line_end.error());
                }

                auto [key_path, val] = kv_result.value();
                auto added = add_pair(*current, key_path, val, pair_start);
                if (! added) {
                    return std::unexpected(added.error());
                }
            }
        }

        merge_tables(result);
        return result.pairs;
    }

    // Puts each table into the pairs of its parent, so node.pairs holds the whole tree.
    static void merge_tables(table_node& node)
    {
        for (auto& [key, table] : node.tables) {
            if (table->array) {
                array elements;
                for (auto& element : table->elements) {
                    merge_tables(*element);
                    elements.append(value(element->pairs));
                }
                node.pairs[key] = elements;
            } else {
                merge_tables(*table);
                node.pairs[key] = table->pairs;
            }
        }
    }

    std::expected<table_header, error> parse_table_header()
    {
        if (current_char() != '[') {
            return std::unexpected(create_error("Expected '['"));
        }
        advance();  // skip '['

        const bool array = (current_char() == '[');
        if (array) {
            advance();
        }

        std::vector<std::string> key_path;

        while (true) {
            skip_whitespace();
            auto key_result = parse_key();
            if (! key_result) {
                return std::unexpected(key_result.error());
            }

            key_path.push_back(key_result.value());

            skip_whitespace();
            if (current_char() == '.') {
                advance();
                continue;
            } else if (current_char() == ']') {
                advance();
                break;
            } else {
                return std::unexpected(create_error("Expected '.' or ']'"));
            }
        }

        if (array) {
            if (current_char() != ']') {
                return std::unexpected(create_error("Expected ']]'"));
            }
            advance();
        }

        return table_header{ key_path, array };
    }

    std::expected<void, error> add_key(dictionary& table, const std::string& key, const value& val, const position& at)
    {
        if (table.contains(key)) {
            return std::unexpected(create_error("Key defined twice", at));
        }
        table[key] = val;
        return {};
    }

    std::expected<void, error> add_key(table_node& table, const std::string& key, const value& val, const position& at)
    {
        if (table.tables.contains(key)) {
            return std::unexpected(create_error("Key defined twice", at));
        }
        return add_key(table.pairs, key, val, at);
    }

    // A dotted key creates the tables before its last part, and can only pass
    // through a table that dotted keys created.
    std::expected<void, error> add_pair(table_node& table, const std::vector<std::string>& key_path, const value& val, const position& at)
    {
        table_node* node = &table;
        for (size_t i = 0; (i + 1) < key_path.size(); ++i) {
            const std::string& key = key_path[i];
            if (node->pairs.contains(key)) {
                return std::unexpected(create_error("Key defined twice", at));
            }
            auto& child = node->tables[key];
            if (! child) {
                child = std::make_unique<table_node>();
                child->dotted = true;
            } else if (! child->dotted) {
                return std::unexpected(create_error("Key defined twice", at));
            }
            node = child.get();
        }
        return add_key(*node, key_path.back(), val, at);
    }

    // Only a table that headers created can take a header, and only one header can name it.
    // An array of tables takes a new table for each [[name]] header and no [name] header.
    std::expected<table_node*, error> open_table(table_node& root, const table_header& header, const position& header_start)
    {
        table_node* table = &root;
        for (size_t i = 0; i < header.key_path.size(); ++i) {
            const std::string& key = header.key_path[i];
            const bool last = ((i + 1) == header.key_path.size());
            if (table->pairs.contains(key)) {
                return std::unexpected(create_error("Table header names a key that is not a table", header_start));
            }
            auto& child = table->tables[key];
            if (! child) {
                child = std::make_unique<table_node>();
                child->array = (header.array && last);
            }
            table = child.get();
            if (table->array && ! last) {
                table = table->elements.back().get();
            }
        }

        if (header.array) {
            if (! table->array) {
                return std::unexpected(create_error("Table defined twice", header_start));
            }
            table->elements.push_back(std::make_unique<table_node>());
            return table->elements.back().get();
        }
        if (table->named || table->dotted || table->array) {
            return std::unexpected(create_error("Table defined twice", header_start));
        }
        table->named = true;
        return table;
    }

    std::expected<std::pair<std::vector<std::string>, value>, error> parse_key_value()
    {
        std::vector<std::string> key_path;
        while (true) {
            auto key_result = parse_key();
            if (! key_result) {
                return std::unexpected(key_result.error());
            }
            key_path.push_back(key_result.value());

            skip_whitespace();
            if (current_char() != '.') {
                break;
            }
            advance();  // skip '.'
            skip_whitespace();
        }

        if (current_char() != '=') {
            return std::unexpected(create_error("Expected '='"));
        }
        advance();  // skip '='

        skip_whitespace();
        auto value_result = parse_value();
        if (! value_result) {
            return std::unexpected(value_result.error());
        }

        return std::make_pair(key_path, value_result.value());
    }

    std::expected<std::string, error> parse_key()
    {
        if (current_char() == '"') {
            return parse_quoted_key();
        } else if (current_char() == '\'') {
            return parse_literal_string("literal key");
        } else if (is_bare_key_char(current_char())) {
            return parse_bare_key();
        } else {
            return std::unexpected(create_error("Invalid key format"));
        }
    }

    std::string parse_bare_key()
    {
        std::string result;
        while ((_pos < _input.size()) && is_bare_key_char(current_char())) {
            result += current_char();
            advance();
        }

        return result;
    }

    std::expected<std::string, error> parse_quoted_key()
    {
        return parse_basic_string("quoted key");
    }

    // Reads the escape whose letter is the current character, after a backslash.
    std::expected<void, error> append_escape(std::string& result)
    {
        switch (current_char()) {
        case '"':
            result += '"';
            return {};
        case '\\':
            result += '\\';
            return {};
        case 'n':
            result += '\n';
            return {};
        case 't':
            result += '\t';
            return {};
        case 'r':
            result += '\r';
            return {};
        case 'b':
            result += '\b';
            return {};
        case 'f':
            result += '\f';
            return {};
        case 'u':
        case 'U': {
            // \uXXXX or \UXXXXXXXX, naming a Unicode scalar value.
            const size_t digits = (current_char() == 'u') ? 4 : 8;
            for (size_t k = 1; k <= digits; ++k) {
                if (! is_ascii_xdigit(peek_char(k))) {
                    return std::unexpected(create_error("Invalid unicode escape"));
                }
            }
            const auto code_point = static_cast<char32_t>(std::stoul(_input.substr(_pos + 1, digits), nullptr, 16));
            if (((code_point >= 0xD800) && (code_point <= 0xDFFF)) || (code_point > 0x10FFFF)) {
                return std::unexpected(create_error("Invalid unicode escape"));
            }
            append_utf8(result, code_point);
            for (size_t k = 0; k < digits; ++k) {
                advance();
            }
            return {};
        }
        default:
            return std::unexpected(create_error("Invalid escape sequence"));
        }
    }

    // Reads a double-quoted string, which TOML uses for both values and keys.
    std::expected<std::string, error> parse_basic_string(const std::string& kind)
    {
        if (current_char() != '"') {
            return std::unexpected(create_error("Expected '\"'"));
        }
        advance();  // skip opening quote

        std::string result;
        while ((_pos < _input.size()) && (current_char() != '"')) {
            if (current_char() == '\\') {
                advance();
                if (_pos >= _input.size()) {
                    return std::unexpected(create_error("Unterminated " + kind));
                }
                auto escaped = append_escape(result);
                if (! escaped) {
                    return std::unexpected(escaped.error());
                }
            } else if (is_forbidden_control(current_char())) {
                return std::unexpected(create_error("Control character in " + kind));
            } else {
                result += current_char();
            }
            advance();
        }

        if (current_char() != '"') {
            return std::unexpected(create_error("Unterminated " + kind));
        }
        advance();  // skip closing quote

        return result;
    }

    // Reads a single-quoted string, which has no escapes.
    std::expected<std::string, error> parse_literal_string(const std::string& kind)
    {
        advance();  // skip opening quote

        std::string result;
        while ((_pos < _input.size()) && (current_char() != '\'')) {
            if (is_forbidden_control(current_char())) {
                return std::unexpected(create_error("Control character in " + kind));
            }
            result += current_char();
            advance();
        }

        if (current_char() != '\'') {
            return std::unexpected(create_error("Unterminated " + kind));
        }
        advance();  // skip closing quote

        return result;
    }

    bool at_line_end(size_t offset = 0) const
    {
        return (peek_char(offset) == '\n') || ((peek_char(offset) == '\r') && (peek_char(offset + 1) == '\n'));
    }

    void skip_line_end()
    {
        if (current_char() == '\r') {
            advance();
        }
        advance();
    }

    // Reads a string between three quotes, which may span lines. A newline
    // right after the opening quotes is dropped, and up to two quotes may
    // come right before the closing ones. A basic string, between """, also
    // reads escapes and drops the whitespace after a backslash that ends a
    // line; a literal one, between ''', reads its text as it is.
    std::expected<std::string, error> parse_multi_line_string(char quote, const std::string& kind)
    {
        const bool basic = (quote == '"');
        for (int k = 0; k < 3; ++k) {
            advance();  // skip opening quotes
        }
        if (at_line_end()) {
            skip_line_end();
        }

        std::string result;
        while (_pos < _input.size()) {
            const char ch = current_char();
            if ((ch == quote) && (peek_char(1) == quote) && (peek_char(2) == quote)) {
                size_t extra = 0;
                while ((extra < 2) && (peek_char(3 + extra) == quote)) {
                    ++extra;
                }
                result.append(extra, quote);
                for (size_t k = 0; k < (3 + extra); ++k) {
                    advance();  // skip closing quotes
                }
                return result;
            }
            if (basic && (ch == '\\')) {
                size_t k = 1;
                while ((peek_char(k) == ' ') || (peek_char(k) == '\t')) {
                    ++k;
                }
                if (at_line_end(k) || ((_pos + k) >= _input.size())) {
                    advance();  // skip backslash
                    while ((current_char() == ' ') || (current_char() == '\t') || at_line_end()) {
                        if (at_line_end()) {
                            skip_line_end();
                        } else {
                            advance();
                        }
                    }
                    continue;
                }
                advance();  // skip backslash
                auto escaped = append_escape(result);
                if (! escaped) {
                    return std::unexpected(escaped.error());
                }
                advance();
                continue;
            }
            if (at_line_end()) {
                if (ch == '\r') {
                    result += '\r';
                }
                result += '\n';
                skip_line_end();
                continue;
            }
            if (is_forbidden_control(ch)) {
                return std::unexpected(create_error("Control character in " + kind));
            }
            result += ch;
            advance();
        }

        return std::unexpected(create_error("Unterminated " + kind));
    }

    // The floats inf and nan, with or without a sign, ending the word.
    bool at_inf_or_nan() const
    {
        const size_t start = ((current_char() == '+') || (current_char() == '-')) ? 1 : 0;
        const auto is_word = [&](const char* word) {
            return (_input.compare(_pos + start, 3, word) == 0) && ! is_bare_key_char(peek_char(start + 3));
        };
        return is_word("inf") || is_word("nan");
    }

    std::expected<value, error> parse_value()
    {
        skip_whitespace();

        char ch = current_char();

        if (at_inf_or_nan()) {
            return std::unexpected(create_error("number has no infinity and keeps nan for errors"));
        }

        const auto to_value = [](const std::string& text) {
            return value(string{ text });
        };
        if (((ch == '"') || (ch == '\'')) && (peek_char(1) == ch) && (peek_char(2) == ch)) {
            return parse_multi_line_string(ch, (ch == '"') ? "multi-line string" : "multi-line literal string").transform(to_value);
        } else if (ch == '"') {
            return parse_basic_string("string").transform(to_value);
        } else if (ch == '\'') {
            return parse_literal_string("literal string").transform(to_value);
        } else if (ch == '[') {
            return parse_array();
        } else if (ch == '{') {
            return parse_inline_table();
        } else if ((ch == 't') || (ch == 'f')) {
            return parse_boolean();
        } else if (at_date() || at_time()) {
            return parse_datetime();
        } else if (is_ascii_digit(ch) || (ch == '-') || (ch == '+')) {
            return parse_number();
        } else {
            return std::unexpected(create_error("Invalid value format"));
        }
    }

    std::expected<value, error> parse_array()
    {
        if (current_char() != '[') {
            return std::unexpected(create_error("Expected '['"));
        }
        advance();  // skip '['

        array result;

        skip_whitespace_and_comments();
        if (current_char() == ']') {
            advance();
            return value(result);
        }

        while (true) {
            auto value_result = parse_value();
            if (! value_result) {
                return std::unexpected(value_result.error());
            }

            result.append(value_result.value());

            skip_whitespace_and_comments();
            if (current_char() == ',') {
                advance();
                skip_whitespace_and_comments();
                if (current_char() == ']') {
                    advance();  // a comma may follow the last value
                    break;
                }
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
        advance();  // skip '{'

        table_node result;

        skip_whitespace();
        if (current_char() == '}') {
            advance();
            return value(dictionary{});
        }

        while (true) {
            const position pair_start = current_position();
            auto kv_result = parse_key_value();
            if (! kv_result) {
                return std::unexpected(kv_result.error());
            }

            auto [key_path, val] = kv_result.value();
            auto added = add_pair(result, key_path, val, pair_start);
            if (! added) {
                return std::unexpected(added.error());
            }

            // TOML v1.0.0 keeps an inline table on one line, so it has no newline or comment.
            skip_whitespace();
            if ((current_char() == '\n') || (current_char() == '#')) {
                return std::unexpected(create_error("An inline table must stay on one line"));
            }
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

        merge_tables(result);
        return value(result.pairs);
    }

    std::expected<value, error> parse_boolean()
    {
        if (((_pos + 4) <= _input.size()) && _input.substr(_pos, 4) == "true") {
            _pos += 4;
            _column += 4;
            return value(boolean{ true });
        } else if (((_pos + 5) <= _input.size()) && _input.substr(_pos, 5) == "false") {
            _pos += 5;
            _column += 5;
            return value(boolean{ false });
        } else {
            return std::unexpected(create_error("Invalid boolean value"));
        }
    }

    bool digits_ahead(size_t offset, size_t count) const
    {
        for (size_t k = 0; k < count; ++k) {
            if (! is_ascii_digit(peek_char(offset + k))) {
                return false;
            }
        }
        return true;
    }

    bool at_date() const
    {
        return digits_ahead(0, 4) && (peek_char(4) == '-');
    }

    bool at_time() const
    {
        return digits_ahead(0, 2) && (peek_char(2) == ':');
    }

    // Reads a datetime with an offset, as RFC 3339 writes it, into a timestamp.
    // timestamp has an offset, so a local datetime, date or time fails.
    std::expected<value, error> parse_datetime()
    {
        const position start = current_position();
        const auto invalid = [&] {
            return std::unexpected(create_error("Invalid datetime", start));
        };
        const auto without_offset = [&] {
            return std::unexpected(create_error("timestamp cannot hold a datetime without an offset", start));
        };
        if (at_time()) {
            return without_offset();
        }

        std::string text;
        const auto take_digits = [&](size_t count) {
            if (! digits_ahead(0, count)) {
                return false;
            }
            for (size_t k = 0; k < count; ++k) {
                text += current_char();
                advance();
            }
            return true;
        };
        const auto take = [&](char c) {
            if (current_char() != c) {
                return false;
            }
            text += c;
            advance();
            return true;
        };

        if (! (take_digits(4) && take('-') && take_digits(2) && take('-') && take_digits(2))) {
            return invalid();
        }
        const bool time_follows = (current_char() == 'T') || (current_char() == 't') || ((current_char() == ' ') && digits_ahead(1, 2) && (peek_char(3) == ':'));
        if (! time_follows) {
            return without_offset();
        }
        text += 'T';
        advance();
        if (! (take_digits(2) && take(':') && take_digits(2) && take(':') && take_digits(2))) {
            return invalid();
        }
        if (take('.')) {
            while (is_ascii_digit(current_char())) {
                take_digits(1);
            }
        }

        if ((current_char() == 'Z') || (current_char() == 'z')) {
            text += 'Z';
            advance();
        } else if ((current_char() == '+') || (current_char() == '-')) {
            if (! (take(current_char()) && take_digits(2) && take(':') && take_digits(2))) {
                return invalid();
            }
        } else {
            return without_offset();
        }

        auto parsed = timestamp::from_string(text);
        if (! parsed) {
            return invalid();
        }
        return value(*parsed);
    }

    bool at_integer_prefix() const
    {
        return (current_char() == '0') && ((peek_char() == 'x') || (peek_char() == 'o') || (peek_char() == 'b'));
    }

    // Reads an integer after a 0x, 0o or 0b prefix, which may start with zeros.
    std::expected<value, error> parse_prefixed_integer()
    {
        const char prefix = peek_char();
        const int base = (prefix == 'x') ? 16 : ((prefix == 'o') ? 8 : 2);
        advance();
        advance();

        std::string digits;
        const auto count = read_digits(digits, base);
        if (! count) {
            return std::unexpected(count.error());
        }
        if (*count == 0) {
            return std::unexpected(create_error(std::string("Expected a digit after '0") + prefix + "'"));
        }

        // A chunk of digits fits in 63 bits, so number multiplies once per chunk.
        const int bits = (base == 16) ? 4 : ((base == 8) ? 3 : 1);
        const size_t chunk = 63 / bits;
        number result{ 0 };
        for (size_t start = 0; start < digits.size(); start += chunk) {
            const size_t length = std::min(chunk, digits.size() - start);
            std::uint64_t part = 0;
            for (size_t k = 0; k < length; ++k) {
                part = (part << bits) | static_cast<std::uint64_t>(digit_value(digits[start + k], base));
            }
            result = result * number{ std::uint64_t{ 1 } << (bits * length) } + number{ part };
        }
        return value(result);
    }

    // Reads the digits of a base onto text. An underscore separates two digits and is not kept.
    std::expected<size_t, error> read_digits(std::string& text, int base)
    {
        size_t count = 0;
        while (true) {
            if (current_char() == '_') {
                if ((count == 0) || (digit_value(peek_char(), base) < 0)) {
                    return std::unexpected(create_error("An underscore must be between digits"));
                }
                advance();
            }
            if (digit_value(current_char(), base) < 0) {
                return count;
            }
            text += current_char();
            advance();
            ++count;
        }
    }

    std::expected<value, error> parse_number()
    {
        if (at_integer_prefix()) {
            return parse_prefixed_integer();
        }

        std::string num_str;

        // Handle sign
        if ((current_char() == '+') || (current_char() == '-')) {
            num_str += current_char();
            advance();
            if (at_integer_prefix()) {
                return std::unexpected(create_error("An integer with a prefix has no sign"));
            }
        }

        // The integer part has no leading zero, a decimal point has a digit on both sides,
        // and an exponent has a digit.
        const bool leading_zero = (current_char() == '0');
        const auto integer_digits = read_digits(num_str, 10);
        if (! integer_digits) {
            return std::unexpected(integer_digits.error());
        }
        if (*integer_digits == 0) {
            return std::unexpected(create_error("Invalid number format"));
        }
        if (leading_zero && (*integer_digits > 1)) {
            return std::unexpected(create_error("Leading zero in a number"));
        }
        if (current_char() == '.') {
            num_str += current_char();
            advance();
            const auto fraction_digits = read_digits(num_str, 10);
            if (! fraction_digits) {
                return std::unexpected(fraction_digits.error());
            }
            if (*fraction_digits == 0) {
                return std::unexpected(create_error("Expected a digit after '.'"));
            }
        }

        if ((current_char() == 'e') || (current_char() == 'E')) {
            num_str += current_char();
            advance();
            if ((current_char() == '+') || (current_char() == '-')) {
                num_str += current_char();
                advance();
            }
            const auto exponent_digits = read_digits(num_str, 10);
            if (! exponent_digits) {
                return std::unexpected(exponent_digits.error());
            }
            if (*exponent_digits == 0) {
                return std::unexpected(create_error("Expected a digit in the exponent"));
            }
        }

        number n{ num_str };
        if (n.is_nan()) {
            return std::unexpected(create_error("Invalid number format"));
        }
        return value(n);
    }
};

/**
 * @brief TOML serializer implementation.
 *
 * This class converts dross structured data back to TOML format.
 */
class serializer {
public:
    std::expected<data, error> serialize(const dictionary& input)
    {
        try {
            _output.clear();
            _indent_level = 0;

            serialize_dictionary(input, {});

            if (! is_valid_utf8(_output)) {
                return std::unexpected(error{ std::errc::operation_not_supported, "A string or key is not valid UTF-8" });
            }

            return data{ _output };
        } catch (const std::bad_alloc&) {
            // Building a detail would allocate again.
            return std::unexpected(error{ std::errc::operation_not_supported });
        } catch (const std::exception& e) {
            return std::unexpected(error{ std::errc::operation_not_supported, e.what() });
        }
    }

private:
    std::string _output;
    size_t _indent_level;

    // An array whose elements are all dictionaries, written with [[name]] headers.
    static bool is_array_of_tables(const value& val)
    {
        if (! val.is<array>()) {
            return false;
        }
        const auto& arr = val.as<array>();
        if (arr.length() == 0) {
            return false;
        }
        for (size_t i = 0; i < arr.length(); ++i) {
            if (! arr[i].is<dictionary>()) {
                return false;
            }
        }
        return true;
    }

    // A value written under a header rather than as a key/value pair.
    static bool is_written_as_table(const value& val)
    {
        return val.is<dictionary>() || is_array_of_tables(val);
    }

    void serialize_dictionary(const dictionary& dict, const std::vector<std::string>& path)
    {
        // First, output simple key-value pairs
        for (const auto& [key, val] : dict) {
            if (! is_written_as_table(val)) {
                serialize_key_value(key, val);
            }
        }

        // Then output nested tables and arrays of tables
        for (const auto& [key, val] : dict) {
            if (! is_written_as_table(val)) {
                continue;
            }
            auto nested_path = path;
            nested_path.push_back(key);
            if (val.is<dictionary>()) {
                _output += "\n[" + join_path(nested_path) + "]\n";
                serialize_dictionary(val.as<dictionary>(), nested_path);
            } else {
                const auto& arr = val.as<array>();
                for (size_t i = 0; i < arr.length(); ++i) {
                    _output += "\n[[" + join_path(nested_path) + "]]\n";
                    serialize_dictionary(arr[i].as<dictionary>(), nested_path);
                }
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
            const auto& n = val.as<number>();
            if (n.is_nan()) {
                throw std::runtime_error("NaN cannot be written as a TOML value");
            }
            _output += std::string(n);
            // A float keeps its fraction, and TOML integers are 64-bit, so a
            // larger integral value is written as a float.
            static const number int64_max{ "9223372036854775807" };
            static const number int64_min{ "-9223372036854775808" };
            if (n.is_integer() && (n.is_floating_point() || n > int64_max || n < int64_min)) {
                _output += ".0";
            }
        } else if (val.is<boolean>()) {
            _output += val.as<boolean>() ? "true" : "false";
        } else if (val.is<timestamp>()) {
            _output += std::string(val.as<timestamp>());
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
            if (i > 0) {
                _output += ", ";
            }
            serialize_value(arr[i]);
        }
        _output += "]";
    }

    void serialize_inline_table(const dictionary& dict)
    {
        _output += "{";
        bool first = true;
        for (const auto& [key, val] : dict) {
            if (! first) {
                _output += ", ";
            }
            first = false;
            _output += escape_key(key) + " = ";
            serialize_value(val);
        }
        _output += "}";
    }

    std::string escape_key(const std::string& key) const
    {
        // An empty key has no bare form, so it is always quoted.
        bool needs_quotes = key.empty();
        for (char ch : key) {
            if (! is_bare_key_char(ch)) {
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
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\t':
                result += "\\t";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            default:
                if (is_forbidden_control(ch)) {
                    // These have no short escape, so they are written as \u00XX.
                    static constexpr char hex[] = "0123456789ABCDEF";
                    const auto byte = static_cast<unsigned char>(ch);
                    result += "\\u00";
                    result += hex[byte >> 4];
                    result += hex[byte & 0x0F];
                } else {
                    result += ch;
                }
                break;
            }
        }
        return result;
    }

    std::string join_path(const std::vector<std::string>& path) const
    {
        std::string result;
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0) {
                result += ".";
            }
            result += escape_key(path[i]);
        }
        return result;
    }

    bool has_non_dict_values(const dictionary& dict) const
    {
        for (const auto& [key, val] : dict) {
            if (! val.is<dictionary>()) {
                return true;
            }
        }
        return false;
    }
};

}  // anonymous namespace

std::expected<dictionary, error> deserialize(const data& input)
{
    if (input.empty()) {
        return dictionary{};
    }

    // Convert binary data to string
    std::string toml_string = input;

    if (! is_valid_utf8(toml_string)) {
        return std::unexpected(error{ std::errc::invalid_argument, "Input is not valid UTF-8" });
    }

    parser p{ toml_string };
    return p.parse();
}

std::expected<data, error> serialize(const dictionary& input)
{
    serializer s;
    return s.serialize(input);
}

}  // namespace dross::toml
