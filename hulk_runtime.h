#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <cstdlib>
#include <ctime>
#include <sstream>

using namespace std;

// ── Clase base para tipos HULK definidos por el usuario ──
struct HulkObject {
    virtual ~HulkObject() = default;
    virtual string toString() const { return "<object>"; }
};

// ── Mapeo de tipos primitivos ──
using Number  = double;
using Boolean = bool;
using String  = string;

// ── Funciones builtin ──
template<typename T>
HulkObject* hulk_print(const T& value) {
    cout << value << "\n";
    return nullptr;
}

// especialización para HulkObject
template<>
HulkObject* hulk_print(HulkObject* const& value) {
    cout << (value ? value->toString() : "null") << "\n";
    return nullptr;
}


inline Number hulk_sqrt(Number x)           { return sqrt(x); }
inline Number hulk_sin(Number x)            { return sin(x); }
inline Number hulk_cos(Number x)            { return cos(x); }
inline Number hulk_exp(Number x)            { return exp(x); }
inline Number hulk_log(Number x, Number y)  { return log(y) / log(x); }
inline Number hulk_rand()                   { return (Number)rand() / RAND_MAX; }

// ── Range — devuelve un vector iterable ──
inline vector<Number> hulk_range(Number min, Number max) {
    vector<Number> result;
    for (Number i = min; i < max; i++)
        result.push_back(i);
    return result;
}

template<typename T>
String to_hulk_string(const T& v) {
    ostringstream oss;
    oss << v;
    return oss.str();
}

template<>
String to_hulk_string(HulkObject* const& v) { return v ? v->toString() : "null"; }

// ── Concatenación ──
template<typename A, typename B>
String hulk_concat(const A& a, const B& b) {
    return to_hulk_string(a) + to_hulk_string(b);
}

template<typename A, typename B>
String hulk_concat_space(const A& a, const B& b) {
    return to_hulk_string(a) + " " + to_hulk_string(b);
}

// ── Protocolo Iterable ──
template<typename T = HulkObject*>
struct Iterable {
    virtual ~Iterable() = default;
    virtual bool next() = 0;
    virtual T current() = 0;
};
