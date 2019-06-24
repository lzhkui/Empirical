//  This file is part of Empirical, https://github.com/devosoft/Empirical
//  Copyright (C) Michigan State University, 2016=2019.
//  Released under the MIT Software license; see doc/LICENSE
//
//  TypeID provides an easy way to convert types to strings.
//
//
//  Developer notes:
//  * Fill out remaining standard library classes (as possible)
//  * Default to type_traits typeid rather than Unknown

#ifndef EMP_TYPE_ID_H
#define EMP_TYPE_ID_H

#include <sstream>
#include <string>

#include "../base/Ptr.h"
#include "../base/vector.h"

#include "type_traits.h"
#include "TypePack.h"


namespace emp {

  using namespace std::string_literals;

  struct TypeID {
    struct Info {
      bool init = false;                     ///< Has this info been initialized yet?
      std::string name = "[unknown type]";   ///< Unique (ideally human-readable) type name
      bool is_abstract = false;
      bool is_array = false;
      bool is_class = false;
      bool is_const = false;
      bool is_empty = false;
      bool is_object = false;
      bool is_pointer = false;
      bool is_reference = false;
      bool is_trivial = false;
      bool is_volatile = false;

      size_t decay_id = 0;
      size_t remove_const_id = 0;
      size_t remove_cv_id = 0;
      size_t remove_ptr_id = 0;
      size_t remove_ref_id = 0;
      size_t remove_volatile_id = 0;

      Info() { ; }
      Info(const std::string & in_name) : name(in_name) { ; }
      Info(const Info&) = default;
    };

    using info_t = emp::Ptr<TypeID::Info>;
    info_t info_ptr;

    static info_t GetUnknownInfoPtr() { static Info info; return &info; }

    TypeID() : info_ptr(GetUnknownInfoPtr()) { ; }
    TypeID(info_t _info) : info_ptr(_info) { ; }
    TypeID(size_t id) : info_ptr((TypeID::Info *) id) { ; }
    TypeID(const TypeID &) = default;
    ~TypeID() { ; }
    TypeID & operator=(const TypeID &) = default;

    operator size_t() const noexcept { return (info_ptr->init) ? (size_t) info_ptr.Raw() : 0; }
    operator bool() const noexcept { return info_ptr->init; }
    bool operator==(TypeID in) const { return info_ptr == in.info_ptr; }
    bool operator!=(TypeID in) const { return info_ptr != in.info_ptr; }

    const std::string & GetName() const { return info_ptr->name; }
    void SetName(const std::string & in_name) { emp_assert(info_ptr); info_ptr->name = in_name; }

    bool IsInitialized() const { return info_ptr->init ; }
    void SetInitialized(bool _in=true) { info_ptr->init = _in; }

    bool IsAbstract() const { return info_ptr->is_abstract ; }
    bool IsArray() const { return info_ptr->is_array ; }
    bool IsClass() const { return info_ptr->is_class ; }
    bool IsConst() const { return info_ptr->is_const ; }
    bool IsEmpty() const { return info_ptr->is_empty ; }
    bool IsObject() const { return info_ptr->is_object ; }
    bool IsPointer() const { return info_ptr->is_pointer ; }
    bool IsReference() const { return info_ptr->is_reference ; }
    bool IsTrivial() const { return info_ptr->is_trivial ; }
    bool IsVolatile() const { return info_ptr->is_volatile ; }

    TypeID GetDecayTypeID() const { return info_ptr->decay_id; }
    TypeID GetRemoveConstTypeID() const { return info_ptr->remove_const_id; }
    TypeID GetRemoveCVTypeID() const { return info_ptr->remove_cv_id; }
    TypeID GetRemovePointerTypeID() const { return info_ptr->remove_ptr_id; }
    TypeID GetRemoveReferenceTypeID() const { return info_ptr->remove_ref_id; }
    TypeID GetRemoveVolatileTypeID() const { return info_ptr->remove_volatile_id; }
  };

  template <typename T> TypeID::Info BuildInfo();

  template <typename T>
  static TypeID GetTypeID() {
    static TypeID::Info info = BuildInfo<T>();  // Create static info so that it is persistent.
    return TypeID(&info);
  }

  template <typename T>
  static TypeID::Info BuildInfo() {
    static TypeID::Info info;
    if (info.init == false) {
      TypeID type_id(&info);

      info.init = true;
      info.name = typeid(T).name();
      info.is_abstract = std::is_abstract<T>();
      info.is_array = std::is_array<T>();
      info.is_class = std::is_class<T>();
      info.is_const = std::is_const<T>();
      info.is_empty = std::is_empty<T>();
      info.is_object = std::is_object<T>();
      info.is_pointer = emp::is_pointer<T>(); // Not std::is_pointer<T>() to deal with emp::Ptr.
      info.is_reference = std::is_reference<T>();
      info.is_trivial = std::is_trivial<T>();
      info.is_volatile = std::is_volatile<T>();

      using decay_t = std::decay_t<T>;
      if constexpr (std::is_same<T, decay_t>()) info.decay_id = (size_t) &info;
      else info.decay_id = GetTypeID< decay_t >();

      using remove_const_t = std::remove_const_t<T>;
      if constexpr (std::is_same<T, remove_const_t>()) info.remove_const_id = (size_t) &info;
      else info.remove_const_id = GetTypeID< remove_const_t >();

      using remove_cv_t = std::remove_cv_t<T>;
      if constexpr (std::is_same<T, remove_cv_t>()) info.remove_cv_id = (size_t) &info;
      else info.remove_cv_id = GetTypeID< remove_cv_t >();

      using remove_ptr_t = emp::remove_pointer_t<T>;
      if constexpr (std::is_same<T, remove_ptr_t>()) info.remove_ptr_id = (size_t) &info;
      else info.remove_ptr_id = GetTypeID< remove_ptr_t >();

      using remove_ref_t = std::remove_reference_t<T>;
      if constexpr (std::is_same<T, remove_ref_t>()) info.remove_ref_id = (size_t) &info;
      else info.remove_ref_id = GetTypeID< remove_ref_t >();

      using remove_volatile_t = std::remove_volatile_t<T>;
      if constexpr (std::is_same<T, remove_volatile_t>()) info.remove_volatile_id = (size_t) &info;
      else info.remove_volatile_id = GetTypeID< remove_volatile_t >();

      // Now, fix the name if we can be more precise about it.
      if (info.is_const) {
        info.name = "const "s + type_id.GetRemoveConstTypeID().GetName();
      }
      else if (info.is_volatile) {
        info.name = "volatile "s + type_id.GetRemoveVolatileTypeID().GetName();
      }
      else if (info.is_pointer) {
        info.name = type_id.GetRemovePointerTypeID().GetName() + '*';
      }
      else if (info.is_reference) {
        info.name = type_id.GetRemoveReferenceTypeID().GetName() + '&';
      }
    }
    
    return info;
  }

  /// Setup a bunch of standard type names to be more readable.
  void SetupTypeNames() {
    // Built-in types.
    GetTypeID<void>().SetName("void");

    GetTypeID<bool>().SetName("bool");
    GetTypeID<double>().SetName("double");
    GetTypeID<float>().SetName("float");

    GetTypeID<char>().SetName("char");
    GetTypeID<char16_t>().SetName("char16_t");
    GetTypeID<char32_t>().SetName("char32_t");

    GetTypeID<int8_t>().SetName("int8_t");
    GetTypeID<int16_t>().SetName("int16_t");
    GetTypeID<int32_t>().SetName("int32_t");
    GetTypeID<int64_t>().SetName("int64_t");

    GetTypeID<uint8_t>().SetName( "uint8_t");
    GetTypeID<uint16_t>().SetName("uint16_t");
    GetTypeID<uint32_t>().SetName("uint32_t");
    GetTypeID<uint64_t>().SetName("uint64_t");

    // Standard library types.
    GetTypeID<std::string>().SetName("std::string");

    // @CAO -- we can actually establish these links when building types...
    // // Check for type attributes...
    // template<typename T> struct TypeID<T*> {
    //   static std::string GetName() { return TypeID<T>::GetName() + '*'; }
    // };

    // // Tools for using TypePack
    // template<typename T, typename... Ts> struct TypeID<emp::TypePack<T,Ts...>> {
    //   static std::string GetTypes() {
    //     std::string out = TypeID<T>::GetName();
    //     if (sizeof...(Ts) > 0) out += ",";
    //     out += TypeID<emp::TypePack<Ts...>>::GetTypes();
    //     return out;
    //   }
    //   static std::string GetName() {
    //     std::string out = "emp::TypePack<";
    //     out += GetTypes();
    //     out += ">";
    //     return out;
    //   }
    // };
    // template<> struct TypeID< emp::TypePack<> > {
    //   static std::string GetTypes() { return ""; }
    //   static std::string GetName() { return "emp::TypePack<>"; }
    // };

    // // Generic TemplateID structure for when none of the specialty cases trigger.
    // template <typename T> struct TemplateID {
    //   static std::string GetName() { return "UnknownTemplate"; }
    // };

    // template<template <typename...> class TEMPLATE, typename... Ts>
    // struct TypeID<TEMPLATE<Ts...>> {
    //   static std::string GetName() {
    //     return TemplateID<TEMPLATE<Ts...>>::GetName()
    //           + '<' + TypeID<emp::TypePack<Ts...>>::GetTypes() + '>';
    //   }
    // };
  }
}

// namespace emp{


//   // Standard library templates.
//   //  template <typename... Ts> struct TemplateID<std::array<Ts...>> { static std::string GetName() { return "array"; } };

//   template<typename T, typename... Ts> struct TypeID< emp::vector<T,Ts...> > {
//     static std::string GetName() {
//       using simple_vt = emp::vector<T>;
//       using full_vt = emp::vector<T,Ts...>;
//       if (std::is_same<simple_vt,full_vt>::value) {
//         return "emp::vector<" + TypeID<T>::GetName() + ">";
//       }
//       return "emp::vector<" + TypeID<TypePack<T,Ts...>>::GetTypes() + ">";
//     }
//   };

//}

#endif
