/* Copyright (c) 2009-2012 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include "watchman/watchman_string.h" // Needed for w_string_t

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <atomic>
#include <cstdlib> /* for size_t */
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "jansson_config.h" // @manual=//watchman/thirdparty/jansson:config_h
#include "watchman/thirdparty/jansson/utf.h"

/* types */

typedef enum {
  JSON_OBJECT,
  JSON_ARRAY,
  JSON_STRING,
  JSON_INTEGER,
  JSON_REAL,
  JSON_TRUE,
  JSON_FALSE,
  JSON_NULL
} json_type;

struct json_t {
  json_type type;
  std::atomic<size_t> refcount;

  explicit json_t(json_type type);

  struct SingletonHack {};
  // true, false, null are never heap allocated, always
  // reference a global singleton value with a bogus refcount
  json_t(json_type type, SingletonHack&&);
};

#define JSON_INTEGER_FORMAT PRId64
using json_int_t = int64_t;

class json_ref {
  json_t* ref_;

  static inline json_t* incref(json_t* json) {
    if (json && json->refcount != (size_t)-1) {
      ++json->refcount;
    }
    return json;
  }

  static inline void decref(json_t* json) {
    if (json && json->refcount != (size_t)-1 && --json->refcount == 0) {
      json_delete(json);
    }
  }
  static void json_delete(json_t* json);

 public:
  json_ref();
  /* implicit */ json_ref(json_t* ref, bool addRef = true);
  /* implicit */ json_ref(std::nullptr_t);

  ~json_ref();
  void reset(json_t* ref = nullptr);

  json_ref(const json_ref& other);
  json_ref& operator=(const json_ref& other);

  json_ref(json_ref&& other) noexcept;
  json_ref& operator=(json_ref&& other) noexcept;

  /* implicit */ operator json_t*() const {
    return ref_;
  }

  explicit operator bool() const {
    return ref_ != nullptr;
  }

  /** Returns the value associated with key in a json object.
   * Returns defval if this json value is not an object or
   * if the key was not found. */
  json_ref get_default(const char* key, json_ref defval = nullptr) const;

  /** Returns the vaule associated with key in a json object.
   * Throws domain_error if this is not a json object or
   * a range_error if the key is not present. */
  const json_ref& get(const char* key) const;

  /** Set key = value */
  void set(const char* key, json_ref&& val);
  void set(const w_string& key, json_ref&& val);

  /** Set a list of key/value pairs */
  inline void set(
      std::initializer_list<std::pair<const char*, json_ref&&>> pairs) {
    for (auto& p : pairs) {
      set(p.first, std::move(p.second));
    }
  }

  /** Returns a reference to the underlying array.
   * Throws domain_error if this is not an array.
   * This is useful both for iterating the array contents
   * and for returning the size of the array. */
  const std::vector<json_ref>& array() const;
  std::vector<json_ref>& array();

  /** Returns a reference to the underlying map object.
   * Throws domain_error if this is not an object.
   * This is useful for iterating over the object contents, etc.
   */
  const std::unordered_map<w_string, json_ref>& object() const;
  std::unordered_map<w_string, json_ref>& object();

  /** Returns a reference to the array value at the specified index.
   * Throws out_of_range or domain_error if the index is bad or if
   * this is not an array */
  inline const json_ref& at(std::size_t idx) const {
    return array().at(idx);
  }

  inline json_type type() const {
    assert(ref_ != nullptr);
    return ref_->type;
  }

  inline bool isObject() const {
    return type() == JSON_OBJECT;
  }
  inline bool isArray() const {
    return type() == JSON_ARRAY;
  }
  inline bool isString() const {
    return type() == JSON_STRING;
  }
  inline bool isBool() const {
    return (type() == JSON_TRUE || type() == JSON_FALSE);
  }
  inline bool isNull() const {
    return type() == JSON_NULL;
  }
  inline bool isNumber() const {
    return isInt() || isDouble();
  }
  inline bool isInt() const {
    return type() == JSON_INTEGER;
  }
  inline bool isDouble() const {
    return type() == JSON_REAL;
  }

  /**
   * Throws if not a string.
   */
  const w_string& asString() const;

  /**
   * If not a string, returns std::nullopt.
   *
   * A more efficient method would return a nullable pointer.
   */
  std::optional<w_string> asOptionalString() const;

  const char* asCString() const;
  bool asBool() const;
  json_int_t asInt() const;
};

inline json_type json_typeof(const json_t* json) {
  return json->type;
}
#define json_is_object(json) (json && json_typeof(json) == JSON_OBJECT)
#define json_is_array(json) (json && json_typeof(json) == JSON_ARRAY)
#define json_is_string(json) (json && json_typeof(json) == JSON_STRING)
#define json_is_integer(json) (json && json_typeof(json) == JSON_INTEGER)
#define json_is_real(json) (json && json_typeof(json) == JSON_REAL)
#define json_is_number(json) (json_is_integer(json) || json_is_real(json))
#define json_is_true(json) (json && json_typeof(json) == JSON_TRUE)
#define json_is_false(json) (json && json_typeof(json) == JSON_FALSE)
#define json_is_boolean(json) (json_is_true(json) || json_is_false(json))
#define json_is_null(json) (json && json_typeof(json) == JSON_NULL)

/* construction, destruction, reference counting */

json_ref json_object();
json_ref json_object(
    std::initializer_list<std::pair<const char*, json_ref>> values);
json_ref json_object_of_size(size_t nelems);
json_ref json_array();
json_ref json_array_of_size(size_t nelems);
json_ref json_array(std::initializer_list<json_ref> values);
json_ref w_string_to_json(w_string str);

template <typename... Args>
json_ref typed_string_to_json(Args&&... args) {
  return w_string_to_json(w_string(std::forward<Args>(args)...));
}

const w_string& json_to_w_string(const json_t* json);
json_ref json_integer(json_int_t value);
json_ref json_real(double value);
json_ref json_true();
json_ref json_false();
#define json_boolean(val) ((val) ? json_true() : json_false())
json_ref json_null();

/* error reporting */

#define JSON_ERROR_TEXT_LENGTH 160
#define JSON_ERROR_SOURCE_LENGTH 80

struct json_error_t {
  int line;
  int column;
  int position;
  char source[JSON_ERROR_SOURCE_LENGTH];
  char text[JSON_ERROR_TEXT_LENGTH];
};

/* getters, setters, manipulation */

size_t json_object_size(const json_t* object);
json_t* json_object_get(const json_t* object, const char* key);
int json_object_set_new(json_t* object, const char* key, json_ref&& value);
int json_object_set_new_nocheck(
    json_t* object,
    const char* key,
    json_ref&& value);
int json_object_del(json_t* object, const char* key);
int json_object_clear(json_t* object);
int json_object_update(const json_t* src, json_t* target);
int json_object_update_existing(const json_t* src, json_t* target);
int json_object_update_missing(const json_t* src, json_t* target);

inline int
json_object_set(json_t* object, const char* key, json_t* value) {
  return json_object_set_new(object, key, json_ref(value));
}

inline int
json_object_set_nocheck(json_t* object, const char* key, json_t* value) {
  return json_object_set_new_nocheck(object, key, json_ref(value));
}

size_t json_array_size(const json_t* array);
json_ref json_array_get(const json_t* array, size_t index);
int json_array_set_new(json_t* array, size_t index, json_ref&& value);
int json_array_append_new(json_t* array, json_ref&& value);
int json_array_insert_new(json_t* array, size_t index, json_ref&& value);
int json_array_remove(json_t* array, size_t index);
int json_array_clear(json_t* array);
int json_array_extend(json_t* array, json_t* other);
int json_array_set_template(json_t* array, json_t* templ);
int json_array_set_template_new(json_t* json, json_ref&& templ);
json_t* json_array_get_template(const json_t* array);

inline int
json_array_set(json_t* array, size_t index, json_t* value) {
  return json_array_set_new(array, index, json_ref(value));
}

inline int json_array_append(json_t* array, json_t* value) {
  return json_array_append_new(array, json_ref(value));
}

inline int
json_array_insert(json_t* array, size_t index, json_t* value) {
  return json_array_insert_new(array, index, json_ref(value));
}

const char* json_string_value(const json_t* string);
json_int_t json_integer_value(const json_t* integer);
double json_real_value(const json_t* real);
double json_number_value(const json_t* json);

int json_integer_set(json_t* integer, json_int_t value);
int json_real_set(json_t* real, double value);

#define JSON_VALIDATE_ONLY 0x1
#define JSON_STRICT 0x2

/* equality */

int json_equal(json_t* value1, json_t* value2);

/* copying */

json_ref json_copy(const json_t* value);
json_ref json_deep_copy(const json_t* value);

/* decoding */

#define JSON_REJECT_DUPLICATES 0x1
#define JSON_DISABLE_EOF_CHECK 0x2
#define JSON_DECODE_ANY 0x4

json_ref json_loads(const char* input, size_t flags, json_error_t* error);
json_ref json_loadb(
    const char* buffer,
    size_t buflen,
    size_t flags,
    json_error_t* error);
json_ref json_loadf(FILE* input, size_t flags, json_error_t* error);
json_ref json_load_file(const char* path, size_t flags);

/* encoding */

#define JSON_INDENT(n) (n & 0x1F)
#define JSON_COMPACT 0x20
#define JSON_ENSURE_ASCII 0x40
#define JSON_SORT_KEYS 0x80
#define JSON_ENCODE_ANY 0x200
#define JSON_ESCAPE_SLASH 0x400

typedef int (
    *json_dump_callback_t)(const char* buffer, size_t size, void* data);

std::string json_dumps(const json_t* json, size_t flags);
int json_dumpf(const json_t* json, FILE* output, size_t flags);
int json_dump_file(const json_t* json, const char* path, size_t flags);
int json_dump_callback(
    const json_t* json,
    json_dump_callback_t callback,
    void* data,
    size_t flags);

namespace watchman::json {

/**
 * Derive from Repr to indicate a struct has toJson and fromJson members and
 * will automatically have a Serde instance.
 */
struct Repr {};

/**
 * Specializations may provide two static members: toJson and fromJson.
 *
 * `static json_ref toJson(T value)`
 * returns a JSON encoding of the given value.

 * `static T fromJson(const json_ref& v)`
 * returns a decoded value, or throws an exception if `v` has the wrong type.
 */
template <typename T>
struct Serde {
  static_assert(
      std::is_base_of_v<Repr, T>,
      "T must either derive Repr or provide a Serde specialization");

  static json_ref toJson(const T& v) {
    return v.toJson();
  }

  static T fromJson(const json_ref& v) {
    return T::fromJson(v);
  }
};

// Primitive Serde Instances
// TODO: flesh this out

template <>
struct Serde<json_ref> {
  static json_ref toJson(json_ref v) {
    return v;
  }
  static json_ref fromJson(json_ref v) {
    return v;
  }
};

template <>
struct Serde<bool> {
  static json_ref toJson(bool v) {
    return json_boolean(v);
  }

  static bool fromJson(const json_ref& v) {
    return v.asBool();
  }
};

template <>
struct Serde<json_int_t> {
  static json_ref toJson(json_int_t i) {
    return json_integer(i);
  }

  static json_int_t fromJson(const json_ref& v) {
    return v.asInt();
  }
};

template <>
struct Serde<w_string> {
  static json_ref toJson(w_string v) {
    return w_string_to_json(v);
  }

  static w_string fromJson(const json_ref& v) {
    return v.asString();
  }
};

template <typename T>
json_ref to(T&& v) {
  using Base = std::remove_const_t<std::remove_reference_t<T>>;
  if constexpr (std::is_integral_v<Base> && !std::is_same_v<Base, bool>) {
    // TODO: json_int_t is a signed 64-bit integer. It would be nice to
    // bounds-check here. It should only be necessary if Base is uint64_t and
    // the high bit is set.
    return Serde<json_int_t>::toJson(v);
  } else {
    return Serde<Base>::toJson(std::forward<T>(v));
  }
}

template <typename T>
T from(const json_ref& j) {
  return Serde<T>::fromJson(j);
}

// Compound Serde Instances
// TODO: add new specializations as necessary

template <typename T>
struct Serde<std::optional<T>> {
  static json_ref toJson(const std::optional<T>& o) {
    return o ? Serde<T>::toJson(*o) : json_null();
  }

  static std::optional<T> fromJson(const json_ref& j) {
    if (j.isNull()) {
      return std::nullopt;
    } else {
      return Serde<T>::fromJson(j);
    }
  }
};

template <typename T>
struct Serde<std::vector<T>> {
  static json_ref toJson(const std::vector<T>& v) {
    auto a = json_array();
    auto& arr = a.array();
    arr.reserve(v.size());
    for (const auto& element : v) {
      arr.push_back(json::to(element));
    }
    return a;
  }

  static std::vector<T> fromJson(const json_ref& j) {
    auto& array = j.array();

    std::vector<T> result;
    result.reserve(array.size());
    for (auto& element : array) {
      result.push_back(Serde<T>::fromJson(element));
    }
    return result;
  }
};

template <typename V>
struct Serde<std::map<w_string, V>> {
  static json_ref toJson(const std::map<w_string, V>& m) {
    auto o = json_object_of_size(m.size());
    for (auto& [name, value] : m) {
      json_object_set(o, name.c_str(), json::to(value));
    }
    return o;
  }

  static std::map<w_string, V> fromJson(const json_ref& j) {
    auto& hashmap = j.object();

    std::map<w_string, V> result;
    for (auto& [key, value] : hashmap) {
      result.emplace(key, json::from<V>(value));
    }
    return result;
  }
};

/**
 * Sets `field` to Serde<T>'s interpretation of a JSON value. Throws if decoding
 * fails.
 */
template <typename T>
void assign(T& field, const json_ref& value) {
  field = json::from<T>(value);
}

/**
 * Sets `field` to Serde<T>'s interpretation of a JSON object field. This
 * overload exists to provide more contextual error messages. Throws if decoding
 * fails.
 */
template <typename T>
void assign(T& field, const json_ref& object, const char* key) {
  try {
    field = json::from<T>(object.get(key));
  } catch (const std::exception& e) {
    throw std::domain_error(fmt::format("field {}: {}", key, e.what()));
  }
}

/**
 * Sets `field` to Serde<T>'s interpretation of a JSON object field, but only if
 * the key is defined. Throws if decoding fails.
 *
 * If the key is not defined, `field` is assigned its zero value.
 */
template <typename T>
void assign_if(T& field, const json_ref& object, const char* key) {
  auto& map = object.object();
  auto it = map.find(key);
  if (it == map.end()) {
    field = T{};
    return;
  }

  try {
    field = json::from<T>(it->second);
  } catch (const std::exception& e) {
    throw std::domain_error(fmt::format("field {}: {}", key, e.what()));
  }
}

} // namespace watchman::json
