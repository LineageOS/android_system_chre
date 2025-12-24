/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "chre/platform/atomic.h"
#include "gtest/gtest.h"

using chre::AtomicBool;
using chre::AtomicUint32;
using chre::AtomicUint8;

TEST(Atomic, DefaultConstructor) {
  AtomicUint32 atomicInt(42);
  EXPECT_EQ(atomicInt.load(), 42);
}

TEST(Atomic, DefaultConstructorBool) {
  AtomicBool atomicBool(true);
  EXPECT_TRUE(atomicBool.load());
}

TEST(Atomic, ValueConstructorAndLoad) {
  AtomicUint32 atomicInt(42);
  EXPECT_EQ(atomicInt.load(), 42);

  // Test implicit conversion
  int value = atomicInt;
  EXPECT_EQ(value, 42);
}

TEST(Atomic, ValueConstructorAndLoadBool) {
  AtomicBool atomicBool(true);
  EXPECT_TRUE(atomicBool.load());

  // Test implicit conversion
  bool value = atomicBool;
  EXPECT_TRUE(value);
}

TEST(Atomic, Store) {
  AtomicUint32 atomicInt(123);
  EXPECT_EQ(atomicInt.load(), 123);

  atomicInt = 456;
  EXPECT_EQ(atomicInt.load(), 456);
}

TEST(Atomic, StoreBool) {
  AtomicBool atomicBool(true);
  EXPECT_TRUE(atomicBool.load());

  atomicBool = false;
  EXPECT_FALSE(atomicBool.load());
}

TEST(Atomic, Exchange) {
  AtomicUint32 atomicInt(10);
  int previousValue = atomicInt.exchange(20);
  EXPECT_EQ(previousValue, 10);
  EXPECT_EQ(atomicInt.load(), 20);
}

TEST(Atomic, ExchangeBool) {
  AtomicBool atomicBool(true);
  bool previousValue = atomicBool.exchange(false);
  EXPECT_TRUE(previousValue);
  EXPECT_FALSE(atomicBool.load());
}

TEST(Atomic, FetchAdd) {
  AtomicUint32 atomicInt(5);
  int previousValue = atomicInt.fetch_add(3);
  EXPECT_EQ(previousValue, 5);
  EXPECT_EQ(atomicInt.load(), 8);
}

TEST(Atomic, FetchSub) {
  AtomicUint32 atomicInt(10);
  int previousValue = atomicInt.fetch_sub(4);
  EXPECT_EQ(previousValue, 10);
  EXPECT_EQ(atomicInt.load(), 6);
}

TEST(Atomic, IncrementOperators) {
  AtomicUint32 atomicInt(0);

  // Post-increment
  EXPECT_EQ(atomicInt++, 0);
  EXPECT_EQ(atomicInt.load(), 1);
}

TEST(Atomic, DecrementOperators) {
  AtomicUint32 atomicInt(5);

  // Post-decrement
  EXPECT_EQ(atomicInt--, 5);
  EXPECT_EQ(atomicInt.load(), 4);
}

TEST(Atomic, DefaultConstructorUint8) {
  AtomicUint8 atomicInt(42);
  EXPECT_EQ(atomicInt.load(), 42);
}

TEST(Atomic, ValueConstructorAndLoadUint8) {
  AtomicUint8 atomicInt(42);
  EXPECT_EQ(atomicInt.load(), 42);

  // Test implicit conversion
  int value = atomicInt;
  EXPECT_EQ(value, 42);
}

TEST(Atomic, StoreUint8) {
  AtomicUint8 atomicInt(123);
  EXPECT_EQ(atomicInt.load(), 123);

  atomicInt = 45;
  EXPECT_EQ(atomicInt.load(), 45);
}

TEST(Atomic, ExchangeUint8) {
  AtomicUint8 atomicInt(10);
  int previousValue = atomicInt.exchange(20);
  EXPECT_EQ(previousValue, 10);
  EXPECT_EQ(atomicInt.load(), 20);
}

TEST(Atomic, FetchAddUint8) {
  AtomicUint8 atomicInt(5);
  int previousValue = atomicInt.fetch_add(3);
  EXPECT_EQ(previousValue, 5);
  EXPECT_EQ(atomicInt.load(), 8);
}

TEST(Atomic, FetchSubUint8) {
  AtomicUint8 atomicInt(10);
  int previousValue = atomicInt.fetch_sub(4);
  EXPECT_EQ(previousValue, 10);
  EXPECT_EQ(atomicInt.load(), 6);
}

TEST(Atomic, IncrementOperatorsUint8) {
  AtomicUint8 atomicInt(0);

  // Post-increment
  EXPECT_EQ(atomicInt++, 0);
  EXPECT_EQ(atomicInt.load(), 1);
}

TEST(Atomic, DecrementOperatorsUint8) {
  AtomicUint8 atomicInt(5);

  // Post-decrement
  EXPECT_EQ(atomicInt--, 5);
  EXPECT_EQ(atomicInt.load(), 4);
}
