/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#pragma once
#include "vector.h"

namespace py {

template <typename T>
class Stack {
 public:
  void push(T value) { storage_.push_back(value); }
  T pop() {
    T result = storage_.back();
    storage_.pop_back();
    return result;
  }

 private:
  Vector<T> storage_;
};

}  // namespace py
