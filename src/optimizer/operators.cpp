#include <unordered_set>
#include "optimizer/operators.h"
#include "parser/expression/abstract_expression.h"
#include "optimizer/operator_visitor.h"

namespace terrier::optimizer {

//===--------------------------------------------------------------------===//
// DummyScan
//===--------------------------------------------------------------------===//
Operator DummyScan::make() {
  DummyScan *dummy = new DummyScan;
  return Operator(dummy);
}

//===--------------------------------------------------------------------===//
// SeqScan
//===--------------------------------------------------------------------===//
Operator SeqScan::make(
    std::shared_ptr<catalog::TableCatalogEntry> table,
    std::string alias, std::vector<AnnotatedExpression> predicates,
    bool update) {
  TERRIER_ASSERT(table != nullptr, "Table cannot be null.");
  SeqScan *scan = new SeqScan;
  scan->table_ = table;
  scan->table_alias = alias;
  scan->predicates = std::move(predicates);
  scan->is_for_update = update;

  return Operator(scan);
}

bool SeqScan::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::SeqScan) return false;
  const SeqScan &node = *static_cast<const SeqScan *>(&r);
  if (predicates.size() != node.predicates.size()) return false;
  for (size_t i = 0; i < predicates.size(); i++) {
    if (!predicates[i].expr->ExactlyEquals(*node.predicates[i].expr.get()))
      return false;
  }
  return true;
}

common::hash_t SeqScan::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  for (auto &pred : predicates)
    hash = common::HashUtil::CombineHashes(hash, pred.expr->Hash());
  return hash;
}

//===--------------------------------------------------------------------===//
// IndexScan
//===--------------------------------------------------------------------===//
Operator IndexScan::make(
    std::shared_ptr<catalog::TableCatalogEntry> table,
    std::string alias, std::vector<AnnotatedExpression> predicates, bool update,
    catalog::index_oid_t index_id, std::vector<catalog::col_oid_t> key_column_id_list,
    std::vector<parser::ExpressionType> expr_type_list,
    std::vector<type::Value> value_list) {
  TERRIER_ASSERT(table != nullptr, "");
  IndexScan *scan = new IndexScan;
  scan->table_ = table;
  scan->is_for_update = update;
  scan->predicates = std::move(predicates);
  scan->table_alias = std::move(alias);
  scan->index_id = index_id;
  scan->key_column_id_list = std::move(key_column_id_list);
  scan->expr_type_list = std::move(expr_type_list);
  scan->value_list = std::move(value_list);

  return Operator(scan);
}

bool IndexScan::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::IndexScan) return false;
  const IndexScan &node = *static_cast<const IndexScan *>(&r);
  // TODO: Should also check value list
  if (index_id != node.index_id ||
      key_column_id_list != node.key_column_id_list ||
      expr_type_list != node.expr_type_list ||
      predicates.size() != node.predicates.size())
    return false;

  for (size_t i = 0; i < predicates.size(); i++) {
    if (!predicates[i].expr->ExactlyEquals(*node.predicates[i].expr.get()))
      return false;
  }
  return true;
}

common::hash_t IndexScan::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(&index_id));
  for (auto &pred : predicates)
    hash = common::HashUtil::CombineHashes(hash, pred.expr->Hash());
  return hash;
}

//===--------------------------------------------------------------------===//
// Physical external file scan
//===--------------------------------------------------------------------===//
Operator ExternalFileScan::make(parser::ExternalFileFormat format,
                                std::string file_name, char delimiter,
                                char quote, char escape) {
  auto *get = new ExternalFileScan();
  get->format = format;
  get->file_name = file_name;
  get->delimiter = delimiter;
  get->quote = quote;
  get->escape = escape;
  return Operator(get);
}

bool ExternalFileScan::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::QueryDerivedScan) return false;
  const auto &get = *static_cast<const ExternalFileScan *>(&r);
  return (format == get.format &&
      file_name == get.file_name && delimiter == get.delimiter &&
      quote == get.quote && escape == get.escape);
}

common::hash_t ExternalFileScan::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(&format));
  hash = common::HashUtil::CombineHashes(
      hash, common::HashUtil::HashBytes((byte *)file_name.data(), file_name.length()));
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::HashBytes((byte *)&delimiter, 1));
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::HashBytes((byte *)&quote, 1));
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::HashBytes((byte *)&escape, 1));
  return hash;
}

//===--------------------------------------------------------------------===//
// Query derived get
//===--------------------------------------------------------------------===//
Operator QueryDerivedScan::make(
    std::string alias,
    std::unordered_map<std::string,
                       std::shared_ptr<parser::AbstractExpression>>
    alias_to_expr_map) {
  QueryDerivedScan *get = new QueryDerivedScan;
  get->table_alias = alias;
  get->alias_to_expr_map = alias_to_expr_map;

  return Operator(get);
}

bool QueryDerivedScan::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::QueryDerivedScan) return false;
  return true;
}

common::hash_t QueryDerivedScan::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  return hash;
}

//===--------------------------------------------------------------------===//
// OrderBy
//===--------------------------------------------------------------------===//
Operator OrderBy::make() {
  OrderBy *order_by = new OrderBy;

  return Operator(order_by);
}

//===--------------------------------------------------------------------===//
// PhysicalLimit
//===--------------------------------------------------------------------===//
Operator Limit::make(
    int64_t offset, int64_t limit,
    std::vector<parser::AbstractExpression *> sort_columns,
    std::vector<bool> sort_ascending) {
  Limit *limit_op = new Limit;
  limit_op->offset = offset;
  limit_op->limit = limit;
  limit_op->sort_exprs = sort_columns;
  limit_op->sort_acsending = sort_ascending;
  return Operator(limit_op);
}

//===--------------------------------------------------------------------===//
// InnerNLJoin
//===--------------------------------------------------------------------===//
Operator InnerNLJoin::make(
    std::vector<AnnotatedExpression> conditions,
    std::vector<std::unique_ptr<parser::AbstractExpression>> &left_keys,
    std::vector<std::unique_ptr<parser::AbstractExpression>> &right_keys) {
  InnerNLJoin *join = new InnerNLJoin();
  join->join_predicates = std::move(conditions);
  join->left_keys = std::move(left_keys);
  join->right_keys = std::move(right_keys);

  return Operator(join);
}

common::hash_t InnerNLJoin::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  for (auto &expr : left_keys)
    hash = common::HashUtil::CombineHashes(hash, expr->Hash());
  for (auto &expr : right_keys)
    hash = common::HashUtil::CombineHashes(hash, expr->Hash());
  for (auto &pred : join_predicates)
    hash = common::HashUtil::CombineHashes(hash, pred.expr->Hash());
  return hash;
}

bool InnerNLJoin::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::InnerNLJoin) return false;
  const InnerNLJoin &node =
      *static_cast<const InnerNLJoin *>(&r);
  if (join_predicates.size() != node.join_predicates.size() ||
      left_keys.size() != node.left_keys.size() ||
      right_keys.size() != node.right_keys.size())
    return false;
  for (size_t i = 0; i < left_keys.size(); i++) {
    if (!left_keys[i]->ExactlyEquals(*node.left_keys[i].get())) return false;
  }
  for (size_t i = 0; i < right_keys.size(); i++) {
    if (!right_keys[i]->ExactlyEquals(*node.right_keys[i].get())) return false;
  }
  for (size_t i = 0; i < join_predicates.size(); i++) {
    if (!join_predicates[i].expr->ExactlyEquals(
        *node.join_predicates[i].expr.get()))
      return false;
  }
  return true;
}

//===--------------------------------------------------------------------===//
// LeftNLJoin
//===--------------------------------------------------------------------===//
Operator LeftNLJoin::make(
    std::shared_ptr<parser::AbstractExpression> join_predicate) {
  LeftNLJoin *join = new LeftNLJoin();
  join->join_predicate = join_predicate;
  return Operator(join);
}

//===--------------------------------------------------------------------===//
// RightNLJoin
//===--------------------------------------------------------------------===//
Operator RightNLJoin::make(
    std::shared_ptr<parser::AbstractExpression> join_predicate) {
  RightNLJoin *join = new RightNLJoin();
  join->join_predicate = join_predicate;
  return Operator(join);
}

//===--------------------------------------------------------------------===//
// OuterNLJoin
//===--------------------------------------------------------------------===//
Operator OuterNLJoin::make(
    std::shared_ptr<parser::AbstractExpression> join_predicate) {
  OuterNLJoin *join = new OuterNLJoin();
  join->join_predicate = join_predicate;
  return Operator(join);
}

//===--------------------------------------------------------------------===//
// InnerHashJoin
//===--------------------------------------------------------------------===//
Operator InnerHashJoin::make(
    std::vector<AnnotatedExpression> conditions,
    std::vector<std::unique_ptr<parser::AbstractExpression>> &&left_keys,
    std::vector<std::unique_ptr<parser::AbstractExpression>> &&right_keys) {
  InnerHashJoin *join = new InnerHashJoin();
  join->join_predicates = std::move(conditions);
  join->left_keys = std::move(left_keys);
  join->right_keys = std::move(right_keys);
  return Operator(join);
}

common::hash_t InnerHashJoin::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  for (auto &expr : left_keys)
    hash = common::HashUtil::CombineHashes(hash, expr->Hash());
  for (auto &expr : right_keys)
    hash = common::HashUtil::CombineHashes(hash, expr->Hash());
  for (auto &pred : join_predicates)
    hash = common::HashUtil::CombineHashes(hash, pred.expr->Hash());
  return hash;
}

bool InnerHashJoin::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::InnerHashJoin) return false;
  const InnerHashJoin &node =
      *static_cast<const InnerHashJoin *>(&r);
  if (join_predicates.size() != node.join_predicates.size() ||
      left_keys.size() != node.left_keys.size() ||
      right_keys.size() != node.right_keys.size())
    return false;
  for (size_t i = 0; i < left_keys.size(); i++) {
    if (!left_keys[i]->ExactlyEquals(*node.left_keys[i].get())) return false;
  }
  for (size_t i = 0; i < right_keys.size(); i++) {
    if (!right_keys[i]->ExactlyEquals(*node.right_keys[i].get())) return false;
  }
  for (size_t i = 0; i < join_predicates.size(); i++) {
    if (!join_predicates[i].expr->ExactlyEquals(
        *node.join_predicates[i].expr.get()))
      return false;
  }
  return true;
}

//===--------------------------------------------------------------------===//
// LeftHashJoin
//===--------------------------------------------------------------------===//
Operator LeftHashJoin::make(
    std::shared_ptr<parser::AbstractExpression> join_predicate) {
  LeftHashJoin *join = new LeftHashJoin();
  join->join_predicate = join_predicate;
  return Operator(join);
}

//===--------------------------------------------------------------------===//
// RightHashJoin
//===--------------------------------------------------------------------===//
Operator RightHashJoin::make(
    std::shared_ptr<parser::AbstractExpression> join_predicate) {
  RightHashJoin *join = new RightHashJoin();
  join->join_predicate = join_predicate;
  return Operator(join);
}

//===--------------------------------------------------------------------===//
// OuterHashJoin
//===--------------------------------------------------------------------===//
Operator OuterHashJoin::make(
    std::shared_ptr<parser::AbstractExpression> join_predicate) {
  OuterHashJoin *join = new OuterHashJoin();
  join->join_predicate = join_predicate;
  return Operator(join);
}

//===--------------------------------------------------------------------===//
// Insert
//===--------------------------------------------------------------------===//
Operator Insert::make(
    std::shared_ptr<catalog::TableCatalogEntry> target_table,
    const std::vector<std::string> *columns,
    const std::vector<std::vector<
        std::unique_ptr<parser::AbstractExpression>>> *values) {
  Insert *insert_op = new Insert;
  insert_op->target_table = target_table;
  insert_op->columns = columns;
  insert_op->values = values;
  return Operator(insert_op);
}

//===--------------------------------------------------------------------===//
// PhysicalInsertSelect
//===--------------------------------------------------------------------===//
Operator InsertSelect::make(
    std::shared_ptr<catalog::TableCatalogEntry> target_table) {
  InsertSelect *insert_op = new InsertSelect;
  insert_op->target_table = target_table;
  return Operator(insert_op);
}

//===--------------------------------------------------------------------===//
// PhysicalDelete
//===--------------------------------------------------------------------===//
Operator Delete::make(
    std::shared_ptr<catalog::TableCatalogEntry> target_table) {
  Delete *delete_op = new Delete;
  delete_op->target_table = target_table;
  return Operator(delete_op);
}

//===--------------------------------------------------------------------===//
// Update
//===--------------------------------------------------------------------===//
Operator Update::make(
    std::shared_ptr<catalog::TableCatalogEntry> target_table,
    const std::vector<std::unique_ptr<parser::UpdateClause>> *
    updates) {
  Update *update = new Update;
  update->target_table = target_table;
  update->updates = updates;
  return Operator(update);
}

//===--------------------------------------------------------------------===//
// ExportExternalFile
//===--------------------------------------------------------------------===//
Operator ExportExternalFile::make(parser::ExternalFileFormat format,
                                          std::string file_name, char delimiter,
                                          char quote, char escape) {
  auto *export_op = new ExportExternalFile();
  export_op->format = format;
  export_op->file_name = file_name;
  export_op->delimiter = delimiter;
  export_op->quote = quote;
  export_op->escape = escape;
  return Operator(export_op);
}

bool ExportExternalFile::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::ExportExternalFile) return false;
  const auto &export_op =
      *static_cast<const ExportExternalFile *>(&r);
  return (format == export_op.format && file_name == export_op.file_name &&
      delimiter == export_op.delimiter && quote == export_op.quote &&
      escape == export_op.escape);
}

common::hash_t ExportExternalFile::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::Hash(&format));
  hash = common::HashUtil::CombineHashes(
      hash, common::HashUtil::HashBytes((byte *)file_name.data(), file_name.length()));
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::HashBytes((byte *)&delimiter, 1));
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::HashBytes((byte *)&quote, 1));
  hash = common::HashUtil::CombineHashes(hash, common::HashUtil::HashBytes((byte *)&escape, 1));
  return hash;
}

//===--------------------------------------------------------------------===//
// HashGroupBy
//===--------------------------------------------------------------------===//
Operator HashGroupBy::make(
    std::vector<std::shared_ptr<parser::AbstractExpression>> columns,
    std::vector<AnnotatedExpression> having) {
  HashGroupBy *agg = new HashGroupBy;
  agg->columns = columns;
  agg->having = move(having);
  return Operator(agg);
}

bool HashGroupBy::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::HashGroupBy) return false;
  const HashGroupBy &hash_op =
      *static_cast<const HashGroupBy *>(&r);
  if (having.size() != hash_op.having.size() || columns.size() != hash_op.columns.size())
    return false;
  for (size_t i = 0; i < having.size(); i++) {
    if (!having[i].expr->ExactlyEquals(*hash_op.having[i].expr.get())) return false;
  }

  std::unordered_set<std::shared_ptr<parser::AbstractExpression>> l_set, r_set;
  for (auto expr : columns) l_set.emplace(expr.get());
  for (auto expr : hash_op.columns) r_set.emplace(expr.get());
  return l_set == r_set;
}

common::hash_t HashGroupBy::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  for (auto &pred : having) hash = common::HashUtil::SumHashes(hash, pred.expr->Hash());
  for (auto expr : columns) hash = common::HashUtil::SumHashes(hash, expr->Hash());
  return hash;
}

//===--------------------------------------------------------------------===//
// SortGroupBy
//===--------------------------------------------------------------------===//
Operator SortGroupBy::make(
    std::vector<std::shared_ptr<parser::AbstractExpression>> columns,
    std::vector<AnnotatedExpression> having) {
  SortGroupBy *agg = new SortGroupBy;
  agg->columns = std::move(columns);
  agg->having = move(having);
  return Operator(agg);
}

bool SortGroupBy::operator==(const BaseOperatorNode &r) {
  if (r.GetType() != OpType::SortGroupBy) return false;
  const SortGroupBy &sort_op =
      *static_cast<const SortGroupBy *>(&r);
  if (having.size() != sort_op.having.size() || columns.size() != sort_op.columns.size())
    return false;
  for (size_t i = 0; i < having.size(); i++) {
    if (!having[i].expr->ExactlyEquals(*sort_op.having[i].expr.get())) return false;
  }
  std::unordered_set<std::shared_ptr<parser::AbstractExpression>> l_set, r_set;
  for (auto expr : columns) l_set.emplace(expr.get());
  for (auto expr : sort_op.columns) r_set.emplace(expr.get());
  return l_set == r_set;
}

common::hash_t SortGroupBy::Hash() const {
  common::hash_t hash = BaseOperatorNode::Hash();
  for (auto &pred : having) hash = common::HashUtil::SumHashes(hash, pred.expr->Hash());
  for (auto expr : columns) hash = common::HashUtil::SumHashes(hash, expr->Hash());
  return hash;
}

//===--------------------------------------------------------------------===//
// Aggregate
//===--------------------------------------------------------------------===//
Operator Aggregate::make() {
  Aggregate *agg = new Aggregate;
  return Operator(agg);
}

//===--------------------------------------------------------------------===//
// Hash
//===--------------------------------------------------------------------===//
Operator Distinct::make() {
  Distinct *hash = new Distinct;
  return Operator(hash);
}

//===--------------------------------------------------------------------===//
template <typename T>
void OperatorNode<T>::Accept(OperatorVisitor *v) const {
  v->Visit((const T *)this);
}

//===--------------------------------------------------------------------===//
template <>
std::string OperatorNode<DummyScan>::name_ = "DummyScan";
template <>
std::string OperatorNode<SeqScan>::name_ = "SeqScan";
template <>
std::string OperatorNode<IndexScan>::name_ = "IndexScan";
template <>
std::string OperatorNode<ExternalFileScan>::name_ = "ExternalFileScan";
template <>
std::string OperatorNode<QueryDerivedScan>::name_ = "QueryDerivedScan";
template <>
std::string OperatorNode<OrderBy>::name_ = "OrderBy";
template <>
std::string OperatorNode<Limit>::name_ = "Limit";
template <>
std::string OperatorNode<InnerNLJoin>::name_ = "InnerNLJoin";
template <>
std::string OperatorNode<LeftNLJoin>::name_ = "LeftNLJoin";
template <>
std::string OperatorNode<RightNLJoin>::name_ = "RightNLJoin";
template <>
std::string OperatorNode<OuterNLJoin>::name_ = "OuterNLJoin";
template <>
std::string OperatorNode<InnerHashJoin>::name_ =
    "InnerHashJoin";
template <>
std::string OperatorNode<LeftHashJoin>::name_ = "LeftHashJoin";
template <>
std::string OperatorNode<RightHashJoin>::name_ =
    "RightHashJoin";
template <>
std::string OperatorNode<OuterHashJoin>::name_ =
    "OuterHashJoin";
template <>
std::string OperatorNode<Insert>::name_ = "Insert";
template <>
std::string OperatorNode<InsertSelect>::name_ = "InsertSelect";
template <>
std::string OperatorNode<Delete>::name_ = "Delete";
template <>
std::string OperatorNode<Update>::name_ = "Update";
template <>
std::string OperatorNode<HashGroupBy>::name_ = "HashGroupBy";
template <>
std::string OperatorNode<SortGroupBy>::name_ = "SortGroupBy";
template <>
std::string OperatorNode<Distinct>::name_ = "Distinct";
template <>
std::string OperatorNode<Aggregate>::name_ = "Aggregate";
template <>
std::string OperatorNode<ExportExternalFile>::name_ =
    "ExportExternalFile";

//===--------------------------------------------------------------------===//
template <>
OpType OperatorNode<DummyScan>::type_ = OpType::DummyScan;
template <>
OpType OperatorNode<SeqScan>::type_ = OpType::SeqScan;
template <>
OpType OperatorNode<IndexScan>::type_ = OpType::IndexScan;
template <>
OpType OperatorNode<ExternalFileScan>::type_ = OpType::ExternalFileScan;
template <>
OpType OperatorNode<QueryDerivedScan>::type_ = OpType::QueryDerivedScan;
template <>
OpType OperatorNode<OrderBy>::type_ = OpType::OrderBy;
template <>
OpType OperatorNode<Distinct>::type_ = OpType::Distinct;
template <>
OpType OperatorNode<Limit>::type_ = OpType::PhysicalLimit;
template <>
OpType OperatorNode<InnerNLJoin>::type_ = OpType::InnerNLJoin;
template <>
OpType OperatorNode<LeftNLJoin>::type_ = OpType::LeftNLJoin;
template <>
OpType OperatorNode<RightNLJoin>::type_ = OpType::RightNLJoin;
template <>
OpType OperatorNode<OuterNLJoin>::type_ = OpType::OuterNLJoin;
template <>
OpType OperatorNode<InnerHashJoin>::type_ = OpType::InnerHashJoin;
template <>
OpType OperatorNode<LeftHashJoin>::type_ = OpType::LeftHashJoin;
template <>
OpType OperatorNode<RightHashJoin>::type_ = OpType::RightHashJoin;
template <>
OpType OperatorNode<OuterHashJoin>::type_ = OpType::OuterHashJoin;
template <>
OpType OperatorNode<Insert>::type_ = OpType::Insert;
template <>
OpType OperatorNode<InsertSelect>::type_ = OpType::InsertSelect;
template <>
OpType OperatorNode<Delete>::type_ = OpType::Delete;
template <>
OpType OperatorNode<Update>::type_ = OpType::Update;
template <>
OpType OperatorNode<HashGroupBy>::type_ = OpType::HashGroupBy;
template <>
OpType OperatorNode<SortGroupBy>::type_ = OpType::SortGroupBy;
template <>
OpType OperatorNode<Aggregate>::type_ = OpType::Aggregate;
template <>
OpType OperatorNode<ExportExternalFile>::type_ =
    OpType::ExportExternalFile;

}  // namespace terrier::optimizer