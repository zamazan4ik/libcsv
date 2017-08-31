// Copyright: Ben Strasser <code@ben-strasser.net>,  2012-2015
//            Alexander Zaitsev <zamazan4ik@tut.by>, 2017
// License: BSD-3
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
//2. Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
//3. Neither the name of the copyright holder nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <utility>
#include <cstdio>
#include <exception>
#include <memory>
#include <cassert>
#include <cerrno>
#include <istream>
#include <fstream>
#include <type_traits>
#include <tuple>


#if (__cplusplus >= 201703L)

#include <filesystem>

using std::apply;

#else

namespace detail
{
template<class F, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<I...>)
{
    return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
}


template<class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t)
{
    return detail::apply_impl(
            std::forward<F>(f), std::forward<Tuple>(t),
            std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});
}
}

using detail::apply;

#endif





namespace libcsv
{



namespace error
{
struct base : public std::exception
{
    virtual void format_error_message() const = 0;

    const char* what() const noexcept
    {
        format_error_message();
        return error_message_buffer;
    }

    mutable char error_message_buffer[256];
};

struct with_file_name
{
    void set_file_name(const char* file_name)
    {
        this->file_name = file_name;
    }

    std::string file_name;
};

struct with_file_line
{
    with_file_line()
    {
        file_line = -1;
    }

    void set_file_line(int file_line)
    {
        this->file_line = file_line;
    }

    int file_line;
};

struct with_errno
{
    with_errno()
    {
        errno_value = 0;
    }

    void set_errno(int errno_value)
    {
        this->errno_value = errno_value;
    }

    int errno_value;
};

struct cannot_open_file :
        base,
        with_file_name,
        with_errno
{
    void format_error_message() const
    {
        if (errno_value != 0)
        {
            std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                          "Can not open file \"%s\" because \"%s\".", file_name.c_str(), std::strerror(errno_value));
        }
        else
        {
            std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                          "Can not open file \"%s\".", file_name.c_str());
        }
    }
};


}



/*class LineWriter
{
private:
    static const int block_len = 1 << 24;
    std::unique_ptr<char[]> buffer; // must be constructed before (and thus destructed after) the reader!
    std::ofstream writer;
    int data_begin;
    int data_end;

    std::string file_name;
    unsigned file_line;

    void open_file(const char* file_name)
    {
        // We open the file in binary mode as it makes no difference under *nix
        // and under Windows we handle \r\n newlines ourself.
        writer.open(file_name, std::ios::binary);
        if (!writer.is_open())
        {
            int x = errno; // store errno as soon as possible, doing it after constructor call can fail.
            error::cannot_open_file err;
            err.set_errno(x);
            err.set_file_name(file_name);
            throw err;
        }
    }

public:
    LineWriter() = delete;

    LineWriter(const LineWriter&) = delete;

    LineWriter& operator=(const LineWriter&) = delete;

    explicit LineWriter(const char* file_name)
    {
        open_file(file_name);
    }


};*/


////////////////////////////////////////////////////////////////////////////
//                                 CSV                                    //
////////////////////////////////////////////////////////////////////////////

namespace error
{

struct with_column_name
{
    void set_column_name(const char* column_name)
    {
        this->column_name = column_name;
    }

    std::string column_name;
};


struct with_column_content
{
    void set_column_content(const char* column_content)
    {
        this->column_content = column_content;
    }

    std::string column_content;
};


struct extra_column_in_header :
        base,
        with_file_name,
        with_column_name
{
    void format_error_message() const
    {
        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "Extra column \"%s\" in header of file \"%s\".", column_name.c_str(), file_name.c_str());
    }
};

struct missing_column_in_header :
        base,
        with_file_name,
        with_column_name
{
    void format_error_message() const
    {
        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "Missing column \"%s\" in header of file \"%s\".", column_name.c_str(), file_name.c_str());
    }
};

struct duplicated_column_in_header :
        base,
        with_file_name,
        with_column_name
{
    void format_error_message() const
    {
        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "Duplicated column \"%s\" in header of file \"%s\".", column_name.c_str(), file_name.c_str());
    }
};

struct too_few_columns :
        base,
        with_file_name,
        with_file_line
{
    void format_error_message() const
    {
        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "Too few columns in line %d in file \"%s\".", file_line, file_name.c_str());
    }
};

struct too_many_columns :
        base,
        with_file_name,
        with_file_line
{
    void format_error_message() const
    {
        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "Too many columns in line %d in file \"%s\".", file_line, file_name.c_str());
    }
};


}


//Quote policies
template<typename separator>
struct quote_all
{
    template<typename ... ColType>
    static std::string process(const std::string& str, const separator& sep, ColType ...other_special_chars)
    {
        return sep + str + sep;
    }
};

template<typename separator>
struct quote_non_numeric
{
    template<typename ...ColType>
    static std::string process(const std::string& str, const separator& sep, ColType ...other_special_chars)
    {
        return sep + str + sep;
    }
};

template<typename separator>
struct quote_minimal
{
private:
    static bool is_special_char(separator)
    {
        return false;
    }

    template<typename ...ColType>
    static bool is_special_char(separator c, separator trim_char, ColType ...other_special_chars)
    {
        return c == trim_char || is_special_char(c, other_special_chars...);
    }

    template <typename ...ColType>
    static bool is_special_string(const std::string& str, ColType& ...cols)
    {
        for(const char glyph : str)
        {
            if(is_special_char(glyph, cols...))
            {
                return true;
            }
        }

        return false;
    }

public:
    template <typename ...ColType>
    static std::string process(const std::string& str, const separator& sep, ColType& ...cols)
    {
        if(is_special_string(str, cols...))
        {
            return sep + str + sep;
        }
        return str;
    }
};


struct quote_none
{
public:
    template<typename ...ColType>
    std::string operator()(const std::string& str, ColType ...other_special_chars)
    {
        return str;
    }
};

//Dialects
template <typename Char, typename quote_policy>
struct Dialect
{
    using char_type = Char;
    using quote_policy_type = quote_policy;

    constexpr Dialect() = default;

    constexpr Dialect(char_type delimiter1, char_type lineTerminator1, char_type quote1, char_type comment1,
                      quote_policy quot_pol) :
            delimiter(delimiter1), lineTerminator(lineTerminator1), quote(quote1), comment(comment1), q_pol(quot_pol)
    {}

    char_type delimiter;
    char_type lineTerminator;
    char_type quote;
    char_type comment;

    quote_policy q_pol;
};

constexpr const Dialect<char, quote_none> CSV     = Dialect<char, quote_none>(',', '\n', '"', '#', quote_none());
constexpr const Dialect<char, quote_none> TSV     = Dialect<char, quote_none>('\t', '\n', '"', '#', quote_none());
constexpr const Dialect<char, quote_none> SCSV    = Dialect<char, quote_none>(';', '\n', '"', '#', quote_none());
constexpr const Dialect<char, quote_none> PSV     = Dialect<char, quote_none>('|', '\n', '"', '#', quote_none());
constexpr const Dialect<char, quote_none> ColonSV = Dialect<char, quote_none>(',', '\n', '"', '#', quote_none());



template <size_t column_count,
        typename OutputStreamType = std::ofstream,
        typename DialectType = Dialect<char, quote_none>
>
class DSVWriter
{
public:
    using char_type = typename DialectType::char_type;
    using quote_policy = typename DialectType::quote_policy_type;
private:
    DialectType dl;
    OutputStreamType out;

public:
    DSVWriter() = delete;

    DSVWriter(const DSVWriter&) = delete;

    DSVWriter& operator=(const DSVWriter&) = delete;
#if (__cplusplus < 201703L)
    DSVWriter(const std::string& filename, const DialectType& dialect = CSV)
            : dl(dialect)
    {
        out.open(filename, std::ios::binary);
    }
#else
    DSVWriter(const std::experimental::filesystem::path& filename, const DialectType& dialect = CSV)
            : dl(dialect)
    {
        out.open(filename, std::ios::binary);
    }
#endif
    DialectType get_dialect() const
    {
        return dl;
    }
private:

    template<typename T>
    typename std::enable_if<!std::is_pod<T>::value, void>::type write_helper(T& t)
    {
        out << dl.q_pol(t, dl.quote) << dl.lineTerminator;
    }

    template<typename T, typename ...ColType>
    typename std::enable_if<std::is_pod<T>::value, void>::type write_helper(T& t)
    {
        out << dl.q_pol(std::to_string(t), dl.delimiter, dl.quote, dl.lineTerminator) << dl.lineTerminator;
    }

    template<typename T, typename ...ColType>
    typename std::enable_if<!std::is_pod<T>::value, void>::type write_helper(T& t, ColType& ...cols)
    {
        out << dl.q_pol(t, dl.quote, dl.delimiter, dl.quote, dl.lineTerminator) << dl.delimiter;
        write_helper(cols...);
    }

    template<typename T, typename ...ColType>
    typename std::enable_if<std::is_pod<T>::value, void>::type write_helper(T& t, ColType& ...cols)
    {
        out << dl.q_pol(std::to_string(t), dl.quote) << dl.delimiter;
        write_helper(cols...);
    }


public:
    template<typename ...ColType>
    bool write_row(ColType& ...cols)
    {
        static_assert(sizeof...(ColType) >= column_count,
                      "not enough columns specified");
        static_assert(sizeof...(ColType) <= column_count,
                      "too many columns specified");
        write_helper(cols...);
        return true;
    }

    template<typename ...Args>
    bool write_row_from_object(std::tuple<Args...>&& t)
    {
        apply([this] (auto&& ...args) { this->write_row(args...); }, std::forward<std::tuple<Args...>>(t));
        return true;
    }

    template<typename ...ColType>
    bool write_comment(ColType& ...cols)
    {
        static_assert(sizeof...(ColType) >= column_count,
                      "not enough columns specified");
        static_assert(sizeof...(ColType) <= column_count,
                      "too many columns specified");

        out << dl.comment;
        write_helper(cols...);
        return true;
    }
};

}
#endif

