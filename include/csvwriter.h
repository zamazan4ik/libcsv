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


namespace ios
{
////////////////////////////////////////////////////////////////////////////
//                                 LineReader                             //
////////////////////////////////////////////////////////////////////////////

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



class LineWriter
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


};


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

struct header_missing :
        base,
        with_file_name
{
    void format_error_message() const
    {
        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "Header missing in file \"%s\".", file_name.c_str());
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


template<unsigned column_count,
         char separator = ','
>
class CSVWriter
{
private:
    //LineWriter out;
    std::ofstream out;
    char* (row[column_count]);
    std::string column_names[column_count];

    bool m_force_double_quotes = false;

    template<typename ...ColNames>
    void set_column_names(std::string s, ColNames...cols)
    {
        column_names[column_count - sizeof...(ColNames) - 1] = std::move(s);
        set_column_names(std::forward<ColNames>(cols)...);
    }

    void set_column_names()
    {}


public:
    CSVWriter() = delete;

    CSVWriter(const CSVWriter&) = delete;

    CSVWriter& operator=(const CSVWriter&) = delete;

    CSVWriter(const std::string& filename, bool force_double_quotes = false) : m_force_double_quotes(force_double_quotes)
    {
        out.open(filename, std::ios::binary);
    }


    /*template<typename ...Args>
    explicit CSVWriter(Args&& ...args) : out(std::forward<Args>(args)...)
    {
        std::fill(row, row + column_count, nullptr);
        col_order.resize(column_count);
        for (unsigned i = 0; i < column_count; ++i)
            col_order[i] = i;
        for (unsigned i = 1; i <= column_count; ++i)
            column_names[i - 1] = "col" + std::to_string(i);
    }*/

    template<typename ...ColNames>
    void write_header(ColNames...cols)
    {
        static_assert(sizeof...(ColNames) >= column_count, "not enough column names specified");
        static_assert(sizeof...(ColNames) <= column_count, "too many column names specified");
        try
        {
            set_column_names(std::forward<ColNames>(cols)...);

            /*detail::parse_header_line
                    <column_count, trim_policy, quote_policy>
                    (line, col_order, column_names, ignore_policy);*/
        }
        catch (error::with_file_name& err)
        {
            //err.set_file_name(out.get_truncated_file_name());
            throw;
        }
    }

    template<typename ...ColNames>
    void set_header(ColNames...cols)
    {
        static_assert(sizeof...(ColNames) >= column_count,
                      "not enough column names specified");
        static_assert(sizeof...(ColNames) <= column_count,
                      "too many column names specified");
        set_column_names(std::forward<ColNames>(cols)...);
        std::fill(row, row + column_count, nullptr);
    }

private:

    template<typename T>
    typename std::enable_if<!std::is_pod<T>::value, void>::type parse_helper(T& t)
    {
        if(m_force_double_quotes)
        {
            out << '"' << t << '"' << '\n';
        }
        else
        {
            out << t << '\n';
        }
    }

    template<typename T, typename ...ColType>
    typename std::enable_if<std::is_pod<T>::value, void>::type parse_helper(T& t)
    {
        if(m_force_double_quotes)
        {
            out << '"' << std::to_string(t) << '"' << '\n';
        }
        else
        {
            out << std::to_string(t) << '\n';
        }
    }

    template<typename T, typename ...ColType>
    typename std::enable_if<!std::is_pod<T>::value, void>::type parse_helper(T& t, ColType& ...cols)
    {
        if(m_force_double_quotes)
        {
            out << '"' << t << '"' << separator;
        }
        else
        {
            out << t << separator;
        }
        parse_helper(cols...);
    }

    template<typename T, typename ...ColType>
    typename std::enable_if<std::is_pod<T>::value, void>::type parse_helper(T& t, ColType& ...cols)
    {
        if(m_force_double_quotes)
        {
            out << '"' << std::to_string(t) << '"' << separator;
        }
        else
        {
            out << std::to_string(t) << separator;
        }
        parse_helper(cols...);
    }



public:
    template<typename ...ColType>
    bool write_row(ColType& ...cols)
    {
        static_assert(sizeof...(ColType) >= column_count,
                      "not enough columns specified");
        static_assert(sizeof...(ColType) <= column_count,
                      "too many columns specified");
        try
        {
            try
            {
                parse_helper(cols...);
            }
            catch (error::with_file_name& err)
            {
                //err.set_file_name(out.get_truncated_file_name());
                throw;
            }
        }
        catch (error::with_file_line& err)
        {
            //err.set_file_line(out.get_file_line());
            throw;
        }

        return true;
    }
};


template<unsigned column_count,
         char separator = '\t'
>
using TSVWriter = CSVWriter<column_count, separator>;


template<unsigned column_count,
         char separator = ':'
>
using ColonWriter = CSVWriter<column_count, separator>;
}
#endif

