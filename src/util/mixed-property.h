// SPDX-License-Identifier: GPL-2.0-or-later
//
// Authors:
//   Michael Kowalski
//
// Copyright (c) 2026 Authors
//

#ifndef INKSCAPE_MIXED_PROPERTY_H
#define INKSCAPE_MIXED_PROPERTY_H

#include <utility>

namespace Inkscape {

// A property that may be unset, uniform across a selection, or mixed.
// T must be default-constructible and equality-comparable.
template<typename T>
class mixed_property {
public:
    mixed_property() = default;
    explicit mixed_property(T default_value) : _value(std::move(default_value)) {}

    // --- state queries ---
    bool is_unset()  const { return _state == PropState::Unset; }
    bool is_single() const { return _state == PropState::Single; }
    bool is_mixed()  const { return _state == PropState::Mixed; }

    // Returns the value. For Mixed, this is the first encountered value.
    const T& value() const { return _value; }
    T&       value()       { return _value; }

    // Merge a new observed value into this property.
    // Call once per visited item; order matters only for the stored value on Mixed.
    template<typename U>
    void merge(U&& v) {
        if (_state == PropState::Unset) {
            _value = std::forward<U>(v);
            _state = PropState::Single;
        } else if (_state == PropState::Single && _value != v) {
            _state = PropState::Mixed;
        }
    }

    // Explicitly set to a known single value (e.g. when resetting).
    void set(const T& v) {
        _value = v;
        _state = PropState::Single;
    }

    void reset() {
        _value = T{};
        _state = PropState::Unset;
    }

private:
    enum class PropState {
        Unset,   // no items visited yet
        Single,  // all items agree on the value
        Mixed,   // at least two items differ
    };

    T         _value{};
    PropState _state = PropState::Unset;
};

} // namespace Inkscape

#endif // INKSCAPE_MIXED_PROPERTY_H
