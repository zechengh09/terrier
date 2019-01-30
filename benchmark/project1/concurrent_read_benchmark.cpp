#include <vector>
#include "benchmark/benchmark.h"
#include "common/scoped_timer.h"
#include "storage/garbage_collector.h"
#include "util/transaction_benchmark_util.h"

namespace terrier {

class ConcurrentReadBenchmark : public benchmark::Fixture {
 public:
  void StartGC(transaction::TransactionManager *const txn_manager) {
    gc_ = new storage::GarbageCollector(txn_manager);
    run_gc_ = true;
    gc_thread_ = std::thread([this] { GCThreadLoop(); });
  }

  void EndGC() {
    run_gc_ = false;
    gc_thread_.join();
    // Make sure all garbage is collected. This take 2 runs for unlink and deallocate
    gc_->PerformGarbageCollection();
    gc_->PerformGarbageCollection();
    delete gc_;
  }

  const std::vector<uint8_t> attr_sizes = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
  const uint32_t initial_table_size = 1000000;
  const uint32_t num_txns = 1000000;
  storage::BlockStore block_store_{1000, 1000};
  storage::RecordBufferSegmentPool buffer_pool_{1000000, 1000000};
  std::default_random_engine generator_;
  const uint32_t num_concurrent_txns_ = 16;

 private:
  std::thread gc_thread_;
  storage::GarbageCollector *gc_ = nullptr;
  volatile bool run_gc_ = false;
  const std::chrono::milliseconds gc_period_{10};

  void GCThreadLoop() {
    while (run_gc_) {
      std::this_thread::sleep_for(gc_period_);
      gc_->PerformGarbageCollection();
    }
  }
};

/**
 * Single statement select throughput. Should have no aborts.
 */
// NOLINTNEXTLINE
BENCHMARK_DEFINE_F(ConcurrentReadBenchmark, SingleStatementSelect)(benchmark::State &state) {
  uint64_t abort_count = 0;
  const uint32_t txn_length = 1;
  const std::vector<double> insert_update_select_ratio = {0, 0, 1};
  // NOLINTNEXTLINE
  for (auto _ : state) {
    LargeTransactionBenchmarkObject tested(attr_sizes, initial_table_size, txn_length, insert_update_select_ratio,
                                           &block_store_, &buffer_pool_, &generator_, true);
    StartGC(tested.GetTxnManager());
    uint64_t elapsed_ms;
    {
      common::ScopedTimer timer(&elapsed_ms);
      abort_count += tested.SimulateOltp(num_txns, num_concurrent_txns_);
    }
    state.SetIterationTime(static_cast<double>(elapsed_ms) / 1000.0);
    EndGC();
  }
  state.SetItemsProcessed(state.iterations() * num_txns - abort_count);
}

BENCHMARK_REGISTER_F(ConcurrentReadBenchmark, SingleStatementSelect)
    ->Unit(benchmark::kMillisecond)
    ->UseManualTime()
    ->MinTime(5);
}  // namespace terrier
