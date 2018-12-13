/**
 *  @note This file is part of Empirical, https://github.com/devosoft/Empirical
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2016-2018
 *
 *  @file  ValPack.h
 *  @brief A set of values that can be manipulated at compile time (good for metaprogramming)
 *
 *  Any built-in type can be added to ValPack to be manipulated at compile time.
 */


#ifndef EMP_VAL_PACK_H
#define EMP_VAL_PACK_H

#include "meta.h"

#include <iostream>
#include <string>

namespace emp {

  // Pre-declaration of ValPack
  template <auto... Ts> struct ValPack;

  // Anonymous implementations of ValPack interface.
  namespace internal {
    template <bool DONE, auto START, auto END, auto STEP, auto... VALS>
    struct vp_range {
      static constexpr auto NEXT = START + STEP;
      using type = typename vp_range<(NEXT >= END), NEXT, END, STEP, VALS..., START>::type;
    };
    template <auto START, auto END, auto STEP, auto... VALS>
    struct vp_range <true, START, END, STEP, VALS...> {
      using type = ValPack<VALS...>;
    };

    template <typename T1, typename T2> struct vp_concat;
    template <auto... T1s, auto... T2s>
    struct vp_concat<ValPack<T1s...>, ValPack<T2s...>> {
      using result = ValPack<T1s..., T2s...>;
    };

    template <typename T_IN, typename T_OUT=ValPack<>, bool DONE=false, auto VERSION=0>
    struct vp_loop {
      // Helpers...
      using in_pop = typename T_IN::pop;
      template <auto V> using out_pbin = typename T_OUT::template push_back_if_not<T_IN::first, V>;
      template <auto V, bool D=false> using pnext = vp_loop< in_pop, out_pbin<V>, D, VERSION >;  // Prune

      // Main operations...
      template <auto V> using pop_val = typename pnext<V, T_IN::first==V>::template pop_val<V>;
      template <auto V> using remove = typename pnext<V>::template remove<V>;
      template <auto V> using uniq = typename pnext<V>::template uniq<T_IN::first>;
    };
    template <typename T_IN, typename T_OUT, auto VERSION>
    struct vp_loop<T_IN, T_OUT, true, VERSION> {
      template <auto V> using pop_val = typename T_OUT::template append<T_IN>; // Pop done!
    };
    template <typename T_OUT, auto VERSION>
    struct vp_loop<ValPack<>, T_OUT, false, VERSION> {
      template <auto V> using pop_val = T_OUT;  // Nothing to pop! (error?)
      template <auto V> using remove = T_OUT;   // Nothing left to remove!
      template <auto V> using uniq = T_OUT;     // Nothing left to check!
    };

    // Implement == ip_push_if_not ==
    template <auto V, auto X, typename T>
    struct vp_push_if_not {
      using result = typename T::template push<V>;
      using back = typename T::template push_back<V>;
    };
    template <auto V, typename T>
    struct vp_push_if_not<V,V,T> {
      using result = T;
      using back = T;
    };

    // Setup ==reverse== operation.
    template <typename T> struct vp_reverse;
    template <auto V1, auto... Vs> struct vp_reverse<ValPack<V1, Vs...>> {
      using result = typename vp_reverse< ValPack<Vs...> >::result::template push_back<V1>;
    };
    template <> struct vp_reverse<ValPack<>> { using result = ValPack<>; };

    // Setup ==uniq== operation.
    template <typename T> struct vp_uniq;
    template <auto V1, auto... Vs> struct vp_uniq<ValPack<V1, Vs...>> {
      using result = typename vp_loop<ValPack<V1, Vs...>, ValPack<>, false, 1>::template uniq<V1+1>;
    };
    template <> struct vp_uniq<ValPack<>> { using result = ValPack<>; };

    // Setup ==sort== operation.
    template <typename T_IN, typename T_OUT>
    struct vp_sort_impl {
      template <auto V> using spop = typename T_IN::template pop_val<V>;
      template <auto V> using snext = vp_sort_impl< spop<V>, typename T_OUT::template push_back<V> >;
      template <auto V> using sort = typename snext<V>::template sort< spop<V>::Min(T_IN::first) >;
    };
    template <typename T_OUT>
    struct vp_sort_impl<ValPack<>, T_OUT> {
      template <auto V> using sort = T_OUT;     // Nothing left to sort!
    };

    template <typename T> struct vp_sort;
    template <auto V1, auto... Vs> struct vp_sort<ValPack<V1, Vs...>> {
      using vp = ValPack<V1, Vs...>;
      using result = typename vp_sort_impl<vp, ValPack<>>::template sort<vp::Min()>;
    };
    template <> struct vp_sort<ValPack<>> { using result = ValPack<>; };
  } // End internal namespace

  // Generate an IntPack with a specified range of values.
  template <auto START, auto END, auto STEP=1>
  using ValPackRange = typename internal::vp_range<(START >= END), START, END, STEP>::type;

  // IntPack with at least one value.
  template <int V1, int... Vs>
  struct IntPack<V1,Vs...> {
    /// First value in IntPack
    static constexpr int first = V1;

    /// Number of values in IntPack    
    constexpr static int SIZE = 1+sizeof...(Vs);

    using this_t = IntPack<V1,Vs...>;
    using pop = IntPack<Vs...>;

    /// Add an int to the front of an IntPack
    template <int V> using push = IntPack<V, V1, Vs...>;

    /// Add an int to the back of an IntPack
    template <int V> using push_back = IntPack<V1, Vs..., V>;

    /// Push V onto front of an IntPack if it does not equal X
    template <int V, int X> using push_if_not = typename internal::ip_push_if_not<V,X,this_t>::result;

    /// Push V onto back of an IntPack if it does not equal X
    template <int V, int X> using push_back_if_not = typename internal::ip_push_if_not<V,X,this_t>::back;

    /// Remove the first time value V appears from an IntPack
    template <int V> using pop_val = typename internal::ip_loop<this_t, IntPack<>, false, 2>::template pop_val<V>;

    /// Remove the all appearances of value V from an IntPack
    template <int V> using remove = typename internal::ip_loop<this_t, IntPack<>, false, 3>::template remove<V>;

    /// Append one whole IntPack to the end of another.
    template <typename T> using append = typename internal::ip_concat<this_t,T>::result;

    /// Apply to a specified template with IntPack as template arguments.
    template <template <int...> class TEMPLATE> using apply = TEMPLATE<V1, Vs...>;


    /// ---=== Member Functions ===---

    /// Return wheter an IntPack contains the value V.
    constexpr static bool Has(int V) { return (V==V1) | pop::Has(V); }

    /// Count the number of occurances of value V in IntPack.
    constexpr static int Count(int V) { return pop::Count(V) + (V==V1); }

    /// Determine the position at which V appears in IntPack.
    constexpr static int GetID(int V) { return (V==V1) ? 0 : (1+pop::GetID(V)); }

    /// Function to retrieve number of elements in IntPack
    constexpr static int GetSize() { return SIZE; }

    /// Determine if there are NO value in an IntPack
    constexpr static bool IsEmpty() { return false; }

    /// Determine if all values in IntPack are different from each other.
    constexpr static bool IsUnique() { return pop::IsUnique() && !pop::Has(V1); }

    /// Add up all values in an IntPack
    constexpr static int Sum() { return V1 + pop::Sum(); }

    /// Multiply together all value in an IntPack
    constexpr static int Product() { return V1 * pop::Product(); }

    /// Find the smallest value in an IntPack, to a maximum of cap.
    constexpr static int Min(int cap) { return cap < pop::Min(V1) ? cap : pop::Min(V1); }

    /// Find the overall smallest value in an IntPack
    constexpr static int Min() { return pop::Min(V1); }

    /// Find the maximum value in an IntPack, to a minimum of floor.
    constexpr static int Max(int floor) { return floor > pop::Max(V1) ? floor : pop::Max(V1); }

    /// Find the overall maximum value in an IntPack.
    constexpr static int Max() { return pop::Max(V1); }

    /// Use each value in an IntPack as an index and return results as a tuple.
    template <typename T>
    constexpr static auto ApplyIndex(T && container) {
      return std::make_tuple(container[V1], container[Vs]...);
    }

    /// Convert all values from an IntPack into a string, treating each as a char.
    static std::string ToString() {
      return std::string(1, (char) V1) + pop::ToString();
    }

    /// Print all values in an IntPack into a stream.
    static void PrintInts(std::ostream & os=std::cout) {
      os << V1;
      if (GetSize() > 1) os << ',';
      pop::PrintInts(os);
    }
  };

  // IntPack with no values.
  template <> struct IntPack<> {
    using this_t = IntPack<>;

    template <int V> using push = IntPack<V>;
    template <int V> using push_back = IntPack<V>;
    template <int V, int X> using push_if_not = typename internal::ip_push_if_not<V,X,IntPack<>>::result;
    template <int V, int X> using push_back_if_not = typename internal::ip_push_if_not<V,X,IntPack<>>::back;
    template <int V> using pop_val = IntPack<>;  // No value to pop!  Faulure?
    template <int V> using remove = IntPack<>;
    template <typename T> using append = T;

    constexpr static bool Has(int) { return false; }
    constexpr static int Count(int) { return 0; }
    constexpr static int GetID(int V) { return -100000; }

    constexpr static int SIZE = 0;
    constexpr static int GetSize() { return 0; }

    constexpr static bool IsEmpty() { return true; }
    constexpr static bool IsUnique() { return true; }

    constexpr static int Sum() { return 0; }
    constexpr static int Product() { return 1; }
    constexpr static int Min(int cap) { return cap; }
    constexpr static int Max(int floor) { return floor; }

    static std::string ToString() { return ""; }

    static void PrintInts(std::ostream & os=std::cout) { ; }
  };

  namespace pack {
    template <typename T> using reverse = typename internal::ip_reverse<T>::result;
    template <typename T> using uniq = typename internal::ip_uniq<T>::result;

    template <typename T> using sort = typename internal::ip_sort<T>::result;
    template <typename T> using Rsort = reverse< sort<T> >;
    template <typename T> using Usort = uniq< sort<T> >;
    template <typename T> using RUsort = reverse< Usort<T> >;
  }
}

#endif
