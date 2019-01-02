/*
 *	JSON Tokenizer and builder.
 *
 *	Copyright © 2012  Rasmus Andersson.
 *	Copyright © 2016 Flamewing <flamewing.sonic@gmail.com>
 *
 *	All rights reserved. Use of this source code is governed by a
 *	MIT-style license that can be found in the copying.md file.
 */

#include "jsont.hh"

using std::stod;
using std::stoll;
using std::string;
using std::string_view;
using namespace std::literals::string_view_literals;

namespace jsont {
    static inline bool safe_isalnum(const char c) {
        return isalnum(static_cast<unsigned char>(c)) != 0;
    }
    static inline bool safe_isdigit(const char c) {
        return isdigit(static_cast<unsigned char>(c)) != 0;
    }
    static inline bool is_plus_minus(const char c) {
        return c == '+' || c == '-';
    }
    static inline bool is_exponent_introducer(const char c) {
        return c == 'E' || c == 'e';
    }

    inline Token Tokenizer::readAtom(string_view atom, Token token) noexcept {
        if (availableInput() < atom.length()) {
            return setError(Tokenizer::PrematureEndOfInput);
        }
        if (_input.compare(_offset, atom.length(), atom) != 0) {
            return setError(Tokenizer::InvalidByte);
        }
        if (availableInput() > atom.length() &&
            safe_isalnum(_input[_offset + atom.length()])) {
            return setError(Tokenizer::SyntaxError);
        }
        _offset += atom.length();
        return setToken(token);
    }

    __attribute__((pure)) string_view Tokenizer::errorMessage() const noexcept {
        switch (_error) {
        case UnexpectedComma:
            return "Unexpected comma"sv;
        case UnexpectedTrailingComma:
            return "Unexpected trailing comma"sv;
        case InvalidByte:
            return "Invalid input byte"sv;
        case PrematureEndOfInput:
            return "Premature end of input"sv;
        case MalformedUnicodeEscapeSequence:
            return "Malformed Unicode escape sequence"sv;
        case MalformedNumberLiteral:
            return "Malformed number literal"sv;
        case UnterminatedString:
            return "Unterminated string"sv;
        case SyntaxError:
            return "Illegal JSON (syntax error)"sv;
        case UnspecifiedError:
            return "Unspecified error"sv;
        }
        return "Unspecified error"sv;
    }

    string_view Tokenizer::dataValue() const noexcept {
        if (!hasValue()) {
            return translateToken(_token);
        }
        return _value;
    }

    double Tokenizer::floatValue() const noexcept {
        if (!hasValue()) {
            return _token == jsont::True ? 1.0 : 0.0;
        }

        if (availableInput() == 0) {
            // In this case where the data lies at the edge of the buffer, we
            // can't pass it directly to atof, since there will be no sentinel
            // byte. We are fine with a copy, since this is an edge case (only
            // happens either for broken JSON or when the whole document is just
            // a number).
            return stod(string(_value), nullptr);
        }
        return strtod(_value.data(), nullptr);
    }

    int64_t Tokenizer::intValue() const noexcept {
        if (!hasValue()) {
            return _token == jsont::True ? 1LL : 0LL;
        }
        constexpr const int base10 = 10;
        if (availableInput() == 0) {
            // In this case where the data lies at the edge of the buffer, we
            // can't pass it directly to atof, since there will be no sentinel
            // byte. We are fine with a copy, since this is an edge case (only
            // happens either for broken JSON or when the whole document is just
            // a number).
            return stoll(string(_value), nullptr, base10);
        }
        return strtoll(_value.data(), nullptr, base10);
    }

    inline void Tokenizer::skipWS() noexcept {
        while (!endOfInput()) {
            char b = _input[_offset++];
            switch (b) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                // IETF RFC4627
                // ignore whitespace
                break;
            default:
                // Put character back and return
                --_offset;
                return;
            }
        }
    }

    inline bool Tokenizer::readDigits(size_t digits) noexcept {
        while (!endOfInput() && safe_isdigit(_input[_offset++])) {
            digits++;
        }
        // Rewind the byte that terminated this digit sequence, unless it was
        // terminated by end-of-input.
        if (!endOfInput()) {
            --_offset;
        }
        return digits > 0;
    }

    inline bool Tokenizer::readFraction() noexcept {
        if (endOfInput() || _input[_offset] != '.') {
            return true;
        }
        setToken(jsont::Float);
        // Skip '.'
        _offset++;
        return readDigits(0);
    }

    inline bool Tokenizer::readExponent() noexcept {
        if (endOfInput() || !is_exponent_introducer(_input[_offset])) {
            return true;
        }
        setToken(jsont::Float);
        // Skip '.'
        _offset++;
        if (is_plus_minus(_input[_offset])) {
            // Skip optional '+'/'-'
            _offset++;
        }
        return readDigits(0);
    }

    inline Token Tokenizer::readNumber(char b, size_t token_start) noexcept {
        bool have_digit = safe_isdigit(b);
        if (!have_digit && b != '-') {
            return setError(InvalidByte);
        }
        setToken(jsont::Integer);
        // Note: JSON grammar allows for a JSON file with a single number
        // surrounded by optional whitespace. Thus, we cannot return
        // end-of-input here or we will miss the number.
        if (!readDigits(static_cast<size_t>(have_digit)) || !readFraction() ||
            !readExponent()) {
            return setError(MalformedNumberLiteral);
        }
        _value = _input.substr(token_start, _offset - token_start);
        return current();
    }

    Token Tokenizer::readString(char b, size_t token_start) noexcept {
        while (!endOfInput()) {
            b = _input[_offset++];

            if (b == '\\') {
                if (endOfInput()) {
                    return setError(PrematureEndOfInput);
                }
                _offset++;
            } else if (b == '"') {
                break;
            } else if (b == 0) {
                return setError(InvalidByte);
            }
        }

        if (b != '"') {
            return setError(UnterminatedString);
        }
        // Note: double-quotes are included in the token value.
        _value = _input.substr(token_start, _offset - token_start);

        skipWS();
        // is this a field name?
        if (!endOfInput()) {
            b = _input[_offset++];
            switch (b) {
            case ':':
                return setToken(FieldName);
            case ',':
            case ']':
            case '}':
                --_offset; // rewind
                return setToken(jsont::String);
            case 0:
                return setError(InvalidByte);
            default:
                // Expected a comma or a colon
                return setError(SyntaxError);
            }
        }
        return setToken(jsont::String);
    }

    Token Tokenizer::next() noexcept {
        //
        // { } [ ] n t f "
        //         | | | |
        //         | | | +- /[^"]*/ "
        //         | | +- a l s e
        //         | +- r u e
        //         +- u l l
        //
        skipWS();
        while (!endOfInput()) {
            size_t token_start = _offset;
            char   b           = _input[_offset++];
            switch (b) {
            case '{':
                return setToken(ObjectStart);
            case '}':
                return readEndBracket(ObjectEnd);
            case '[':
                return setToken(ArrayStart);
            case ']':
                return readEndBracket(ArrayEnd);
            case 'n':
                return readAtom("ull"sv, jsont::Null);
            case 't':
                return readAtom("rue"sv, jsont::True);
            case 'f':
                return readAtom("alse"sv, jsont::False);
            case 0:
                return setError(InvalidByte);
            // when we read a value, we don't produce a token until we
            // either reach end of input, a colon (then the value is a field
            // name), a comma, or an array or object terminator.
            case '"':
                return readString(b, token_start);
            case ',':
                return readComma();
            default:
                return readNumber(b, token_start);
            }
        }

        return setToken(End);
    }

} // namespace jsont
