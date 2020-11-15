//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// transaction_manager.cpp
//
// Identification: src/concurrency/transaction_manager.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "concurrency/transaction_manager.h"

#include <unordered_map>
#include <unordered_set>

#include "storage/table/table_heap.h"
#include "catalog/catalog.h"

namespace bustub {

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};

Transaction *TransactionManager::Begin(Transaction *txn) {
  // Acquire the global transaction latch in shared mode.
  global_txn_latch_.RLock();

  if (txn == nullptr) {
    txn = new Transaction(next_txn_id_++);
  }

  if (enable_logging) {
    // TODO(student): Add logging here.
  }

  txn_map[txn->GetTransactionId()] = txn;
  return txn;
}

void TransactionManager::Commit(Transaction *txn) {
  txn->SetState(TransactionState::COMMITTED);

  // Perform all deletes before we commit.
  auto write_set = txn->GetWriteSet();
  while (!write_set->empty()) {
    auto &item = write_set->back();
    auto table = item.table_;
    if (item.wtype_ == WType::DELETE) {
      // Note that this also releases the lock when holding the page latch.
      table->ApplyDelete(item.rid_, txn);
    }
    write_set->pop_back();
  }
  write_set->clear();

  if (enable_logging) {
    // TODO(student): add logging here
  }

  // Release all the locks.
  ReleaseLocks(txn);
  // Release the global transaction latch.
  global_txn_latch_.RUnlock();
}

void TransactionManager::Abort(Transaction *txn) {
  txn->SetState(TransactionState::ABORTED);
  // Rollback before releasing the lock.
  auto table_write_set = txn->GetWriteSet();
  while (!table_write_set->empty()) {
    auto &item = table_write_set->back();
    auto table = item.table_;
    if (item.wtype_ == WType::DELETE) {
      table->RollbackDelete(item.rid_, txn);
    } else if (item.wtype_ == WType::INSERT) {
      // Note that this also releases the lock when holding the page latch.
      table->ApplyDelete(item.rid_, txn);
    } else if (item.wtype_ == WType::UPDATE) {
      table->UpdateTuple(item.tuple_, item.rid_, txn);
    }
    table_write_set->pop_back();
  }
  table_write_set->clear();
  // Rollback index updates
  auto index_write_set = txn->GetIndexWriteSet();
  while (!index_write_set->empty())
  {
    auto &item = index_write_set->back();
    auto catalog = item.catalog_;
    // Metadata identifying the table that should be deleted from.
    TableMetadata *table_info = catalog->GetTable(item.table_oid_);
    std::vector<IndexInfo *> index_list_ =catalog->GetTableIndexes(table_info->name_);
    BUSTUB_ASSERT(index_list_.size() > item.index_, "index out of range!");
    IndexInfo *index_info = index_list_[item.index_];
    auto new_key = item.tuple_.KeyFromTuple(table_info->schema_, *(index_info->index_->GetKeySchema()), index_info->index_->GetKeyAttrs());
    if (item.wtype_ == WType::DELETE) {
      index_info->index_->InsertEntry(new_key, item.rid_, txn);
    } else if (item.wtype_ == WType::INSERT) {
      index_info->index_->DeleteEntry(new_key, item.rid_, txn);
    } else if (item.wtype_ == WType::UPDATE) {
      // Delete the new key and insert the old key
      index_info->index_->DeleteEntry(new_key, item.rid_, txn);
      auto old_key = item.old_tuple_.KeyFromTuple(table_info->schema_, *(index_info->index_->GetKeySchema()), index_info->index_->GetKeyAttrs());
      index_info->index_->InsertEntry(new_key, item.rid_, txn);
    }
  }
  table_write_set->clear();
  index_write_set->clear();

  if (enable_logging) {
    // TODO(student): add logging here
  }

  // Release all the locks.
  ReleaseLocks(txn);
  // Release the global transaction latch.
  global_txn_latch_.RUnlock();
}

void TransactionManager::BlockAllTransactions() { global_txn_latch_.WLock(); }

void TransactionManager::ResumeTransactions() { global_txn_latch_.WUnlock(); }

}  // namespace bustub
