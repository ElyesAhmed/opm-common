// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *

 * \brief This file specializes the dense-AD Evaluation class for 7 derivatives.

 *
 * \attention THIS FILE GETS AUTOMATICALLY GENERATED BY THE "genEvalSpecializations.py"
 *            SCRIPT. DO NOT EDIT IT MANUALLY!
 */

#ifndef OPM_DENSEAD_EVALUATION7_HPP
#define OPM_DENSEAD_EVALUATION7_HPP


#include "Math.hpp"

#include <opm/common/Valgrind.hpp>

#include <dune/common/version.hh>

#include <array>
#include <cmath>
#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace Opm {
namespace DenseAd {


template <class ValueT>
class Evaluation<ValueT, 7>

{
public:
    //! field type
    typedef ValueT ValueType;

    //! number of derivatives

    static constexpr int size = 7;


protected:
    //! length of internal data vector
    static constexpr int length_ = size + 1;

    //! position index for value
    static constexpr int valuepos_ = 0;
    //! start index for derivatives
    static constexpr int dstart_ = 1;
    //! end+1 index for derivatives
    static constexpr int dend_ = length_;

public:

    //! default constructor
    Evaluation() : data_()
    {}

    //! copy other function evaluation
    Evaluation(const Evaluation& other)

    {

        data_[0] = other.data_[0];
        data_[1] = other.data_[1];
        data_[2] = other.data_[2];
        data_[3] = other.data_[3];
        data_[4] = other.data_[4];
        data_[5] = other.data_[5];
        data_[6] = other.data_[6];
        data_[7] = other.data_[7];
    }


    // create an evaluation which represents a constant function
    //
    // i.e., f(x) = c. this implies an evaluation with the given value and all
    // derivatives being zero.
    template <class RhsValueType>
    Evaluation(const RhsValueType& c)
    {
        setValue( c );
        clearDerivatives();
        Valgrind::CheckDefined( data_ );
    }

    // create an evaluation which represents a constant function
    //
    // i.e., f(x) = c. this implies an evaluation with the given value and all
    // derivatives being zero.
    template <class RhsValueType>
    Evaluation(const RhsValueType& c, int varPos)
    {
        setValue( c );
        clearDerivatives();
        // The variable position must be in represented by the given variable descriptor
        assert(0 <= varPos && varPos < size);

        data_[varPos + dstart_] = 1.0;
        Valgrind::CheckDefined(data_);
    }

    // set all derivatives to zero
    void clearDerivatives()
    {


        data_[1] = 0.0;
        data_[2] = 0.0;
        data_[3] = 0.0;
        data_[4] = 0.0;
        data_[5] = 0.0;
        data_[6] = 0.0;
        data_[7] = 0.0;

    }

    // create a function evaluation for a "naked" depending variable (i.e., f(x) = x)
    template <class RhsValueType>
    static Evaluation createVariable(const RhsValueType& value, int varPos)
    {
        // copy function value and set all derivatives to 0, except for the variable
        // which is represented by the value (which is set to 1.0)
        return Evaluation( value, varPos );
    }

    // "evaluate" a constant function (i.e. a function that does not depend on the set of
    // relevant variables, f(x) = c).
    template <class RhsValueType>
    static Evaluation createConstant(const RhsValueType& value)
    {
        return Evaluation( value );
    }

    // print the value and the derivatives of the function evaluation
    void print(std::ostream& os = std::cout) const
    {
        // print value
        os << "v: " << value() << " / d:";
        // print derivatives
        for (int varIdx = 0; varIdx < size; ++varIdx)
            os << " " << derivative(varIdx);
    }

    // copy all derivatives from other
    void copyDerivatives(const Evaluation& other)
    {
        for (int i = dstart_; 0 < dend_; ++i)
            data_[i] = other.data_[i];
    }


    // add value and derivatives from other to this values and derivatives
    Evaluation& operator+=(const Evaluation& other)
    {


        data_[0] += other.data_[0];
        data_[1] += other.data_[1];
        data_[2] += other.data_[2];
        data_[3] += other.data_[3];
        data_[4] += other.data_[4];
        data_[5] += other.data_[5];
        data_[6] += other.data_[6];
        data_[7] += other.data_[7];


        return *this;
    }

    // add value from other to this values
    template <class RhsValueType>
    Evaluation& operator+=(const RhsValueType& other)
    {
        // value is added, derivatives stay the same
        data_[valuepos_] += other;
        return *this;
    }

    // subtract other's value and derivatives from this values
    Evaluation& operator-=(const Evaluation& other)
    {


        data_[0] -= other.data_[0];
        data_[1] -= other.data_[1];
        data_[2] -= other.data_[2];
        data_[3] -= other.data_[3];
        data_[4] -= other.data_[4];
        data_[5] -= other.data_[5];
        data_[6] -= other.data_[6];
        data_[7] -= other.data_[7];

        return *this;
    }

    // subtract other's value from this values
    template <class RhsValueType>
    Evaluation& operator-=(const RhsValueType& other)
    {
        // for constants, values are subtracted, derivatives stay the same
        data_[ valuepos_ ] -= other;

        return *this;
    }

    // multiply values and apply chain rule to derivatives: (u*v)' = (v'u + u'v)
    Evaluation& operator*=(const Evaluation& other)
    {
        // while the values are multiplied, the derivatives follow the product rule,
        // i.e., (u*v)' = (v'u + u'v).
        const ValueT u = this->value();
        const ValueT v = other.value();

        // value
        this->data_[valuepos_] *= v ;

        //  derivatives


        this->data_[1] = this->data_[1]*v + other.data_[1] * u;
        this->data_[2] = this->data_[2]*v + other.data_[2] * u;
        this->data_[3] = this->data_[3]*v + other.data_[3] * u;
        this->data_[4] = this->data_[4]*v + other.data_[4] * u;
        this->data_[5] = this->data_[5]*v + other.data_[5] * u;
        this->data_[6] = this->data_[6]*v + other.data_[6] * u;
        this->data_[7] = this->data_[7]*v + other.data_[7] * u;


        return *this;
    }

    // m(c*u)' = c*u'
    template <class RhsValueType>
    Evaluation& operator*=(const RhsValueType& other)
    {


        data_[0] *= other;
        data_[1] *= other;
        data_[2] *= other;
        data_[3] *= other;
        data_[4] *= other;
        data_[5] *= other;
        data_[6] *= other;
        data_[7] *= other;


        return *this;
    }

    // m(u*v)' = (v'u + u'v)
    Evaluation& operator/=(const Evaluation& other)
    {
        // values are divided, derivatives follow the rule for division, i.e., (u/v)' = (v'u - u'v)/v^2.
        const ValueT v_vv = 1.0 / other.value();
        const ValueT u_vv = value() * v_vv * v_vv;

        // value
        data_[valuepos_] *= v_vv;

        //  derivatives


        data_[1] = data_[1]*v_vv - other.data_[1]*u_vv;
        data_[2] = data_[2]*v_vv - other.data_[2]*u_vv;
        data_[3] = data_[3]*v_vv - other.data_[3]*u_vv;
        data_[4] = data_[4]*v_vv - other.data_[4]*u_vv;
        data_[5] = data_[5]*v_vv - other.data_[5]*u_vv;
        data_[6] = data_[6]*v_vv - other.data_[6]*u_vv;
        data_[7] = data_[7]*v_vv - other.data_[7]*u_vv;


        return *this;
    }

    // divide value and derivatives by value of other
    template <class RhsValueType>
    Evaluation& operator/=(const RhsValueType& other)
    {
        ValueType tmp = 1.0/other;


        data_[0] *= tmp;
        data_[1] *= tmp;
        data_[2] *= tmp;
        data_[3] *= tmp;
        data_[4] *= tmp;
        data_[5] *= tmp;
        data_[6] *= tmp;
        data_[7] *= tmp;


        return *this;
    }

    // division of a constant by an Evaluation
    template <class RhsValueType>
    static inline Evaluation divide(const RhsValueType& a, const Evaluation& b)
    {
        Evaluation result;
        ValueType tmp = 1.0/b.value();
        result.setValue( a*tmp );
        const ValueT df_dg = - result.value()*tmp;



        result.data_[1] = df_dg*b.data_[1];
        result.data_[2] = df_dg*b.data_[2];
        result.data_[3] = df_dg*b.data_[3];
        result.data_[4] = df_dg*b.data_[4];
        result.data_[5] = df_dg*b.data_[5];
        result.data_[6] = df_dg*b.data_[6];
        result.data_[7] = df_dg*b.data_[7];

        return result;
    }

    // add two evaluation objects
    Evaluation operator+(const Evaluation& other) const
    {
        Evaluation result(*this);
        result += other;
        return result;
    }

    // add constant to this object
    template <class RhsValueType>
    Evaluation operator+(const RhsValueType& other) const
    {
        Evaluation result(*this);
        result += other;
        return result;
    }

    // subtract two evaluation objects
    Evaluation operator-(const Evaluation& other) const
    {
        Evaluation result(*this);
        return (result -= other);
    }

    // subtract constant from evaluation object
    template <class RhsValueType>
    Evaluation operator-(const RhsValueType& other) const
    {
        Evaluation result(*this);
        return (result -= other);
    }

    // negation (unary minus) operator
    Evaluation operator-() const
    {
        Evaluation result;
        // set value and derivatives to negative


        result.data_[0] = - data_[0];
        result.data_[1] = - data_[1];
        result.data_[2] = - data_[2];
        result.data_[3] = - data_[3];
        result.data_[4] = - data_[4];
        result.data_[5] = - data_[5];
        result.data_[6] = - data_[6];
        result.data_[7] = - data_[7];


        return result;
    }

    Evaluation operator*(const Evaluation& other) const
    {
        Evaluation result(*this);
        result *= other;
        return result;
    }

    template <class RhsValueType>
    Evaluation operator*(const RhsValueType& other) const
    {
        Evaluation result(*this);
        result *= other;
        return result;
    }

    Evaluation operator/(const Evaluation& other) const
    {
        Evaluation result(*this);
        result /= other;
        return result;
    }

    template <class RhsValueType>
    Evaluation operator/(const RhsValueType& other) const
    {
        Evaluation result(*this);
        result /= other;
        return result;
    }

    template <class RhsValueType>
    Evaluation& operator=(const RhsValueType& other)
    {
        setValue( other );
        clearDerivatives();
        return *this;
    }

    // copy assignment from evaluation
    Evaluation& operator=(const Evaluation& other)
    {


        data_[0] = other.data_[0];
        data_[1] = other.data_[1];
        data_[2] = other.data_[2];
        data_[3] = other.data_[3];
        data_[4] = other.data_[4];
        data_[5] = other.data_[5];
        data_[6] = other.data_[6];
        data_[7] = other.data_[7];


        return *this;
    }

    template <class RhsValueType>
    bool operator==(const RhsValueType& other) const
    { return value() == other; }

    bool operator==(const Evaluation& other) const
    {
        for (int idx = 0; idx < length_; ++idx)
            if (data_[idx] != other.data_[idx])
                return false;

        return true;
    }

    bool operator!=(const Evaluation& other) const
    { return !operator==(other); }

    template <class RhsValueType>
    bool operator>(RhsValueType other) const
    { return value() > other; }

    bool operator>(const Evaluation& other) const
    { return value() > other.value(); }

    template <class RhsValueType>
    bool operator<(RhsValueType other) const
    { return value() < other; }

    bool operator<(const Evaluation& other) const
    { return value() < other.value(); }

    template <class RhsValueType>
    bool operator>=(RhsValueType other) const
    { return value() >= other; }

    bool operator>=(const Evaluation& other) const
    { return value() >= other.value(); }

    template <class RhsValueType>
    bool operator<=(RhsValueType other) const
    { return value() <= other; }

    bool operator<=(const Evaluation& other) const
    { return value() <= other.value(); }

    // return value of variable
    const ValueType& value() const
    { return data_[valuepos_]; }

    // set value of variable
    template <class RhsValueType>
    void setValue(const RhsValueType& val)
    { data_[valuepos_] = val; }

    // return varIdx'th derivative
    const ValueType& derivative(int varIdx) const
    {
        assert(0 <= varIdx && varIdx < size);
        return data_[dstart_ + varIdx];
    }

    // set derivative at position varIdx
    void setDerivative(int varIdx, const ValueType& derVal)
    {
        assert(0 <= varIdx && varIdx < size);
        data_[dstart_ + varIdx] = derVal;
    }

private:
    std::array<ValueT, length_> data_;
};



} } // namespace DenseAd, Opm


#endif // OPM_DENSEAD_EVALUATION7_HPP
