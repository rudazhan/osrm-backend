//
// Created by ruda on 1/11/17.
// http://stackoverflow.com/a/34109317/5407633
// https://github.com/LizardM4/ballin-octo-tribble/blob/master/csv/csv.h
//

#ifndef OSRM_CSV_READER_HPP
#define OSRM_CSV_READER_HPP

#include <string>
#include <sstream>
#include <tuple>
#include <iterator>
#include <type_traits>

namespace csvtools {
    /// Read the last element of the tuple without calling recursively
    template <std::size_t idx, class... fields>
    // index should not past the last field
    typename std::enable_if<idx == std::tuple_size<std::tuple<fields...>>::value - 1>::type
    read_tuple(std::istream &in, std::tuple<fields...> &out, const char delimiter) {
        std::string cell;
        std::getline(in, cell, delimiter);
        std::stringstream cell_stream(cell);
        cell_stream >> std::get<idx>(out);
    }

    /// Read the @p idx-th element of the tuple and then calls itself with @p idx + 1 to
    /// read the next element of the tuple. Automatically falls in the previous case when
    /// reaches the last element of the tuple thanks to enable_if
    template <std::size_t idx, class... fields>
    typename std::enable_if<idx < std::tuple_size<std::tuple<fields...>>::value - 1>::type
    read_tuple(std::istream &in, std::tuple<fields...> &out, const char delimiter) {
        std::string cell;
        std::getline(in, cell, delimiter);
        std::stringstream cell_stream(cell);
        cell_stream >> std::get<idx>(out);
        read_tuple<idx + 1, fields...>(in, out, delimiter);
    }
}

/// Iterable csv wrapper around a stream. @p fields the list of types that form up a row.
template <class... fields>
class csv {
    std::istream &_in;
    const char _delim;
public:
    typedef std::tuple<fields...> value_type;
    class iterator;

    /// Construct from a stream.
    inline csv(std::istream &in, const char delim) : _in(in), _delim(delim) {}

    /// Status of the underlying stream
    /// @{
    inline bool good() const {
        return _in.good();
    }
    inline const std::istream &underlying_stream() const {
        return _in;
    }
    /// @}

    inline iterator begin() { return iterator(*this); };
    inline iterator end() { return iterator(); };
private:

    /// Reads a line into a stringstream, and then reads the line into a tuple, that is returned
    inline value_type read_row() {
        std::string line;
        std::getline(_in, line);
        std::stringstream line_stream(line);
        std::tuple<fields...> retval;
        csvtools::read_tuple<0, fields...>(line_stream, retval, _delim);
        return retval;
    }
};

/// Iterator; just calls recursively @ref csv::read_row and stores the result.
template <class... fields>
class csv<fields...>::iterator {
    csv::value_type _row;
    csv *_parent;
public:
    typedef std::input_iterator_tag iterator_category;
    typedef csv::value_type         value_type;
    typedef std::size_t             difference_type;
    typedef csv::value_type *       pointer;
    typedef csv::value_type &       reference;

    /// Construct an empty/end iterator
    inline iterator() : _parent(nullptr) {}
    /// Construct an iterator at the beginning of the @p parent csv object.
    inline iterator(csv &parent) : _parent(parent.good() ? &parent : nullptr) {
        ++(*this);
    }

    /// Read one row, if possible. Set to end if parent is not good anymore.
    inline iterator &operator++() {
        if (_parent != nullptr) {
            _row = _parent->read_row();
            if (!_parent->good()) {
                _parent = nullptr;
            }
        }
        return *this;
    }

    inline iterator operator++(int) {
        iterator copy = *this;
        ++(*this);
        return copy;
    }

    inline csv::value_type const &operator*() const {
        return _row;
    }

    inline csv::value_type const *operator->() const {
        return &_row;
    }

    bool operator==(iterator const &other) {
        return (this == &other) or (_parent == nullptr and other._parent == nullptr);
    }
    bool operator!=(iterator const &other) {
        return not (*this == other);
    }
};


#endif //OSRM_CSV_READER_HPP
