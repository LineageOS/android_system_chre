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

#include "chre/shmem_spmc_queue/queue.h"
#include "chre/shmem_spmc_queue/internal/queue_internal.h"
#include "chre/shmem_spmc_queue/queue_defs.h"

namespace chre::shmem_spmc_queue {
namespace internal {

pw::Status ProducerBase::initialize(uintptr_t /*shmemBase*/,
                                    uint32_t /*shmemSize*/, Queue & /*queue*/,
                                    pw::Allocator & /*allocator*/,
                                    pw::allocator::Layout /*layout*/,
                                    size_t /*count*/, size_t /*blockCapacity*/,
                                    size_t /*elementAlignment*/, bool /*local*/,
                                    IdOrNotifyFn /*idOrNotifyFn*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

ProducerBase::ProducerBase(
    uintptr_t /*shmemBase*/, uint32_t /*shmemSize*/, Queue & /*queue*/,
    pw::Allocator & /*allocator*/, pw::allocator::Layout /*blockLayout*/,
    size_t /*maxBlockCount*/, size_t /*minBlockCount*/, uint32_t /*dataOffset*/,
    DataNotifier & /*dataNotifier*/, ConsumerManager & /*consumerManager*/,
    RemoteNotifyFn /*remoteNotifyFn*/, MemoryAccess * /*memAccess*/) {
  // TODO(b/445482700): Implement.
}

ProducerBase::~ProducerBase() {
  // TODO(b/445482700): Implement.
}

pw::Status ProducerBase::setMaxBlockCountTarget(size_t /*count*/,
                                                bool /*force*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

size_t ProducerBase::getMaxBlockCountTarget() const {
  // TODO(b/445482700): Implement.
  return 0;
}

pw::Status ProducerBase::setMinBlockCountTarget(size_t /*count*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

size_t ProducerBase::getMinBlockCountTarget() const {
  // TODO(b/445482700): Implement.
  return 0;
}

size_t ProducerBase::getBlockCount() const {
  // TODO(b/445482700): Implement.
  return 0;
}

pw::Result<pw::ByteSpan> ProducerBase::reserve(size_t /*count*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ProducerBase::commit(size_t /*count*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

pw::Result<size_t> ProducerBase::push(pw::ConstByteSpan /*data*/,
                                      bool /*allOrNothing*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

bool ProducerBase::full() const {
  // TODO(b/445482700): Implement.
  return true;
}

size_t ProducerBase::size(bool /*includeReserved*/,
                          bool /*includeOverwritable*/) const {
  // TODO(b/445482700): Implement.
  return 0;
}

size_t ProducerBase::capacity() const {
  // TODO(b/445482700): Implement.
  return 0;
}

pw::Status ConsumerBase::initialize(uintptr_t /*shmemBase*/,
                                    uint32_t /*shmemSize*/, Queue * /*queue*/,
                                    ConsumerDesc * /*desc*/,
                                    IdOrNotifyFn /*idOrNotifyFn*/,
                                    ConsumerPolicy /*policy*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

ConsumerBase::ConsumerBase(uintptr_t /*shmemBase*/, uint32_t /*shmemSize*/,
                           Queue & /*queue*/, ConsumerDesc & /*desc*/,
                           uint32_t /*dataOffset*/,
                           RemoteNotifyFn /*remoteNotifyFn*/,
                           MemoryAccess * /*memAccess*/,
                           std::optional<size_t> /*overwriteResetOffset*/) {
  // TODO(b/445967147): Implement.
}

ConsumerBase::~ConsumerBase() {
  // TODO(b/445967147): Implement.
}

pw::Status ConsumerBase::updatePolicy(ConsumerPolicy /*policy*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

void ConsumerBase::disable() {
  // TODO(b/445967147): Implement.
}

pw::Status ConsumerBase::checkState() {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Result<pw::ConstByteSpan> ConsumerBase::peek(size_t /*count*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerBase::release(size_t /*count*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerBase::pop(pw::ByteSpan /*data*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerBase::resync(size_t /*offset*/) {
  // TODO(b/445967147): Implement.
  return pw::Status::Unimplemented();
}

size_t ConsumerBase::size() {
  // TODO(b/445967147): Implement.
  return 0;
}

bool ConsumerBase::empty() {
  // TODO(b/445967147): Implement.
  return true;
}

}  // namespace internal

void DataNotifier::onWrite(internal::ProducerBase & /*producer*/) {
  // TODO(b/445482700): Implement.
}

void DataNotifier::updatePeriod(internal::ProducerBase & /*producer*/,
                                pw::span<const uint8_t, 16> /*consumerId*/,
                                std::optional<uint32_t> /*periodMs*/) {
  // TODO(b/445482700): Implement.
}

ConsumerManager::ConsumerManager(uintptr_t /*base*/, void * /*queue*/,
                                 pw::Allocator & /*allocator*/) {
  // TODO(b/445482700): Implement.
}

pw::Result<uint32_t> ConsumerManager::addConsumer() {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

pw::Status ConsumerManager::removeConsumer(uint32_t /*offset*/) {
  // TODO(b/445482700): Implement.
  return pw::Status::Unimplemented();
}

}  // namespace chre::shmem_spmc_queue
